/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#pragma once

#include "filepathview.h"

#include <compilermacro.h>
#include <includesearchpath.h>

#include <utils/smallstringvector.h>
#include <utils/cpplanguage_details.h>

namespace ClangBackEnd {

template<typename ProjectInfo, typename OutputContainer = std::vector<std::string>>
class CommandLineBuilder
{
public:
    CommandLineBuilder(const ProjectInfo &projectInfo,
                       const Utils::SmallStringVector &toolChainArguments = {},
                       FilePathView sourcePath = {},
                       FilePathView outputPath = {},
                       FilePathView includePchPath = {})
    {
        commandLine.reserve(128);

        addCompiler(projectInfo.language);
        addToolChainArguments(toolChainArguments);
        addLanguage(projectInfo);
        addLanguageVersion(projectInfo);
        addNoStdIncAndNoStdLibInc();
        addCompilerMacros(projectInfo.compilerMacros);
        addProjectIncludeSearchPaths(
            sortedIncludeSearchPaths(projectInfo.projectIncludeSearchPaths));
        addSystemAndBuiltInIncludeSearchPaths(
            sortedIncludeSearchPaths(projectInfo.systemIncludeSearchPaths));
        addIncludePchPath(includePchPath);
        addOutputPath(outputPath);
        addSourcePath(sourcePath);
    }

    void addCompiler(Utils::Language language)
    {
        if (language == Utils::Language::Cxx)
            commandLine.emplace_back("clang++");
        else
            commandLine.emplace_back("clang");
    }

    void addToolChainArguments(const Utils::SmallStringVector &toolChainArguments)
    {
        for (Utils::SmallStringView argument : toolChainArguments)
            commandLine.emplace_back(argument);
    }

    static const char *language(const ProjectInfo &projectInfo)
    {
        switch (projectInfo.language) {
        case Utils::Language::C:
            if (projectInfo.languageExtension && Utils::LanguageExtension::ObjectiveC)
                return "objective-c-header";

            return "c-header";
        case Utils::Language::Cxx:
            if (projectInfo.languageExtension && Utils::LanguageExtension::ObjectiveC)
                return "objective-c++-header";
        }

        return "c++-header";
    }

    void addLanguage(const ProjectInfo &projectInfo)
    {
        commandLine.emplace_back("-x");
        commandLine.emplace_back(language(projectInfo));
    }

    const char *standardLanguageVersion(Utils::LanguageVersion languageVersion)
    {
        switch (languageVersion) {
        case Utils::LanguageVersion::C89:
            return "-std=c89";
        case Utils::LanguageVersion::C99:
            return "-std=c99";
        case Utils::LanguageVersion::C11:
            return "-std=c11";
        case Utils::LanguageVersion::C18:
            return "-std=c18";
        case Utils::LanguageVersion::CXX98:
            return "-std=c++98";
        case Utils::LanguageVersion::CXX03:
            return "-std=c++03";
        case Utils::LanguageVersion::CXX11:
            return "-std=c++11";
        case Utils::LanguageVersion::CXX14:
            return "-std=c++14";
        case Utils::LanguageVersion::CXX17:
            return "-std=c++17";
        case Utils::LanguageVersion::CXX2a:
            return "-std=c++2a";
        }

        return "-std=c++2a";
    }

    const char *gnuLanguageVersion(Utils::LanguageVersion languageVersion)
    {
        switch (languageVersion) {
        case Utils::LanguageVersion::C89:
            return "-std=gnu89";
        case Utils::LanguageVersion::C99:
            return "-std=gnu99";
        case Utils::LanguageVersion::C11:
            return "-std=gnu11";
        case Utils::LanguageVersion::C18:
            return "-std=gnu18";
        case Utils::LanguageVersion::CXX98:
            return "-std=gnu++98";
        case Utils::LanguageVersion::CXX03:
            return "-std=gnu++03";
        case Utils::LanguageVersion::CXX11:
            return "-std=gnu++11";
        case Utils::LanguageVersion::CXX14:
            return "-std=gnu++14";
        case Utils::LanguageVersion::CXX17:
            return "-std=gnu++17";
        case Utils::LanguageVersion::CXX2a:
            return "-std=gnu++2a";
        }

        return "-std=gnu++2a";
    }

    const char *includeOption(IncludeSearchPathType type)
    {
        switch (type) {
        case IncludeSearchPathType::User:
        case IncludeSearchPathType::System:
        case IncludeSearchPathType::BuiltIn:
            return "-isystem";
        case IncludeSearchPathType::Framework:
            return "-F";
        case IncludeSearchPathType::Invalid:
            return "";
        }

        return "-I";
    }

    void addLanguageVersion(const ProjectInfo &projectInfo)
    {
        if (projectInfo.languageExtension && Utils::LanguageExtension::Gnu)
            commandLine.emplace_back(gnuLanguageVersion(projectInfo.languageVersion));
        else
            commandLine.emplace_back(standardLanguageVersion(projectInfo.languageVersion));
    }

    void addCompilerMacros(const CompilerMacros &compilerMacros)
    {
        CompilerMacros macros = compilerMacros;

        std::sort(macros.begin(),
                  macros.end(),
                  [](const CompilerMacro &first, const CompilerMacro &second) {
                      return first.index < second.index;
                  });

        for (const CompilerMacro &macro : macros)
            commandLine.emplace_back(Utils::SmallString{"-D", macro.key, "=", macro.value});
    }

    IncludeSearchPaths sortedIncludeSearchPaths(const IncludeSearchPaths &unsortedPaths)
    {
        IncludeSearchPaths paths = unsortedPaths;
        std::sort(paths.begin(), paths.end(), [](const auto &first, const auto &second) {
            return first.index < second.index;
        });

        return paths;
    }

    void addProjectIncludeSearchPaths(const IncludeSearchPaths &projectIncludeSearchPaths)
    {
        for (const IncludeSearchPath &path : projectIncludeSearchPaths) {
            commandLine.emplace_back("-I");
            commandLine.emplace_back(path.path);
        }
    }

    void addSystemAndBuiltInIncludeSearchPaths(const IncludeSearchPaths &systemIncludeSearchPaths)
    {
        addSystemIncludeSearchPaths(systemIncludeSearchPaths);
        addBuiltInSystemSearchPaths(systemIncludeSearchPaths);
    }

    void addSystemIncludeSearchPaths(const IncludeSearchPaths &systemIncludeSearchPaths)
    {
        for (const IncludeSearchPath &path : systemIncludeSearchPaths) {
            if (path.type != IncludeSearchPathType::BuiltIn) {
                commandLine.emplace_back(includeOption(path.type));
                commandLine.emplace_back(path.path);
            }
        }
    }

    void addBuiltInSystemSearchPaths(const IncludeSearchPaths &systemIncludeSearchPaths)
    {
        for (const IncludeSearchPath &path : systemIncludeSearchPaths) {
            if (path.type == IncludeSearchPathType::BuiltIn) {
                commandLine.emplace_back(includeOption(path.type));
                commandLine.emplace_back(path.path);
            }
        }
    }

    void addOutputPath(FilePathView outputPath)
    {
        if (!outputPath.isEmpty()) {
            commandLine.emplace_back("-o");
            commandLine.emplace_back(outputPath);
        }
    }

    void addSourcePath(FilePathView sourcePath)
    {
        if (!sourcePath.isEmpty())
            commandLine.emplace_back(sourcePath);
    }

    void addIncludePchPath(FilePathView includePchPath)
    {
        if (!includePchPath.isEmpty()) {
            commandLine.emplace_back("-Xclang");
            commandLine.emplace_back("-include-pch");
            commandLine.emplace_back("-Xclang");
            commandLine.emplace_back(includePchPath);
        }
    }

    void addNoStdIncAndNoStdLibInc()
    {
        commandLine.emplace_back("-nostdinc");
        commandLine.emplace_back("-nostdlibinc");
    }

public:
    OutputContainer commandLine;
};

} // namespace ClangBackEnd
