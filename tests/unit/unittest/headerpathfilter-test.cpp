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

#include "googletest.h"

#include <cpptools/headerpathfilter.h>

namespace {

using ProjectExplorer::HeaderPath;
using ProjectExplorer::HeaderPathType;

MATCHER_P(HasBuiltIn,
          path,
          std::string(negation ? "isn't " : "is ")
              + PrintToString(HeaderPath{QString::fromUtf8(path), HeaderPathType::BuiltIn}))
{
    return arg.path == path && arg.type == HeaderPathType::BuiltIn;
}

MATCHER_P(HasSystem,
          path,
          std::string(negation ? "isn't " : "is ")
              + PrintToString(HeaderPath{QString::fromUtf8(path), HeaderPathType::BuiltIn}))
{
    return arg.path == path && arg.type == HeaderPathType::System;
}

MATCHER_P(HasFramework,
          path,
          std::string(negation ? "isn't " : "is ")
              + PrintToString(HeaderPath{QString::fromUtf8(path), HeaderPathType::BuiltIn}))
{
    return arg.path == path && arg.type == HeaderPathType::Framework;
}

MATCHER_P(HasUser,
          path,
          std::string(negation ? "isn't " : "is ")
              + PrintToString(HeaderPath{QString::fromUtf8(path), HeaderPathType::BuiltIn}))
{
    return arg.path == path && arg.type == HeaderPathType::User;
}

class HeaderPathFilter : public testing::Test
{
protected:
    HeaderPathFilter()
    {
        auto headerPaths = {HeaderPath{"", HeaderPathType::BuiltIn},
                            HeaderPath{"/builtin_path", HeaderPathType::BuiltIn},
                            HeaderPath{"/system_path", HeaderPathType::System},
                            HeaderPath{"/framework_path", HeaderPathType::Framework},
                            HeaderPath{"/user_path", HeaderPathType::User}};

        projectPart.headerPaths = headerPaths;
    }

protected:
    CppTools::ProjectPart projectPart;
    CppTools::HeaderPathFilter filter{projectPart, CppTools::UseTweakedHeaderPaths::No};
};

TEST_F(HeaderPathFilter, BuiltIn)
{
    filter.process();

    ASSERT_THAT(filter.builtInHeaderPaths, Contains(HasBuiltIn("/builtin_path")));
}

TEST_F(HeaderPathFilter, System)
{
    filter.process();

    ASSERT_THAT(filter.systemHeaderPaths, Contains(HasSystem("/system_path")));
}

TEST_F(HeaderPathFilter, User)
{
    filter.process();

    ASSERT_THAT(filter.userHeaderPaths, Contains(HasUser("/user_path")));
}

TEST_F(HeaderPathFilter, Framework)
{
    filter.process();

    ASSERT_THAT(filter.systemHeaderPaths, Contains(HasFramework("/framework_path")));
}

TEST_F(HeaderPathFilter, DontAddInvalidPath)
{
    filter.process();

    ASSERT_THAT(filter,
                AllOf(Field(&CppTools::HeaderPathFilter::builtInHeaderPaths,
                            ElementsAre(HasBuiltIn("/builtin_path"))),
                      Field(&CppTools::HeaderPathFilter::systemHeaderPaths,
                            ElementsAre(HasSystem("/system_path"), HasFramework("/framework_path"))),
                      Field(&CppTools::HeaderPathFilter::userHeaderPaths,
                            ElementsAre(HasUser("/user_path")))));
}

TEST_F(HeaderPathFilter, ClangHeadersPath)
{
    CppTools::HeaderPathFilter filter{projectPart,
                                      CppTools::UseTweakedHeaderPaths::Yes,
                                      "6.0",
                                      CLANG_RESOURCE_DIR};

    filter.process();

    ASSERT_THAT(filter.builtInHeaderPaths,
                ElementsAre(HasBuiltIn(CLANG_RESOURCE_DIR), HasBuiltIn("/builtin_path")));
}

TEST_F(HeaderPathFilter, ClangHeadersPathWitoutClangVersion)
{
    CppTools::HeaderPathFilter filter{projectPart,
                                      CppTools::UseTweakedHeaderPaths::Yes};

    filter.process();

    ASSERT_THAT(filter.builtInHeaderPaths,
                ElementsAre(HasBuiltIn("/builtin_path")));
}

TEST_F(HeaderPathFilter, ClangHeadersAndCppIncludesPathsOrderMacOs)
{
    auto builtIns = {HeaderPath{"/usr/include/c++/4.2.1", HeaderPathType::BuiltIn},
                     HeaderPath{"/usr/include/c++/4.2.1/backward", HeaderPathType::BuiltIn},
                     HeaderPath{"/usr/local/include", HeaderPathType::BuiltIn},
                     HeaderPath{"/Applications/Xcode.app/Contents/Developer/Toolchains/"
                                "XcodeDefault.xctoolchain/usr/bin/../lib/clang/6.0/include",
                                HeaderPathType::BuiltIn},
                     HeaderPath{"/Applications/Xcode.app/Contents/Developer/Toolchains/"
                                "XcodeDefault.xctoolchain/usr/include",
                                HeaderPathType::BuiltIn},
                     HeaderPath{"/usr/include", HeaderPathType::BuiltIn}};
    projectPart.toolChainTargetTriple = "x86_64-apple-darwin10";
    std::copy(builtIns.begin(),
              builtIns.end(),
              std::inserter(projectPart.headerPaths, projectPart.headerPaths.begin()));
    CppTools::HeaderPathFilter filter{projectPart,
                                      CppTools::UseTweakedHeaderPaths::Yes,
                                      "6.0",
                                      CLANG_RESOURCE_DIR};

    filter.process();

    ASSERT_THAT(filter.builtInHeaderPaths,
                ElementsAre(HasBuiltIn("/usr/include/c++/4.2.1"),
                            HasBuiltIn("/usr/include/c++/4.2.1/backward"),
                            HasBuiltIn("/usr/local/include"),
                            HasBuiltIn(CLANG_RESOURCE_DIR),
                            HasBuiltIn("/Applications/Xcode.app/Contents/Developer/Toolchains/"
                                       "XcodeDefault.xctoolchain/usr/include"),
                            HasBuiltIn("/usr/include"),
                            HasBuiltIn("/builtin_path")));
}

TEST_F(HeaderPathFilter, ClangHeadersAndCppIncludesPathsOrderLinux)
{
    auto builtIns = {
        HeaderPath{"/usr/lib/gcc/x86_64-linux-gnu/4.8/../../../../include/c++/4.8",
                   HeaderPathType::BuiltIn},
        HeaderPath{"/usr/lib/gcc/x86_64-linux-gnu/4.8/../../../../include/c++/4.8/backward",
                   HeaderPathType::BuiltIn},
        HeaderPath{"/usr/lib/gcc/x86_64-linux-gnu/4.8/../../../../include/x86_64-linux-gnu/c++/4.8",
                   HeaderPathType::BuiltIn},
        HeaderPath{"/usr/local/include", HeaderPathType::BuiltIn},
        HeaderPath{"/usr/lib/gcc/x86_64-linux-gnu/4.8/include", HeaderPathType::BuiltIn},
        HeaderPath{"/usr/include/x86_64-linux-gnu", HeaderPathType::BuiltIn},
        HeaderPath{"/usr/include", HeaderPathType::BuiltIn}};
    std::copy(builtIns.begin(),
              builtIns.end(),
              std::inserter(projectPart.headerPaths, projectPart.headerPaths.begin()));
    projectPart.toolChainTargetTriple = "x86_64-linux-gnu";
    CppTools::HeaderPathFilter filter{projectPart,
                                      CppTools::UseTweakedHeaderPaths::Yes,
                                      "6.0",
                                      CLANG_RESOURCE_DIR};

    filter.process();

    ASSERT_THAT(filter.builtInHeaderPaths,
                ElementsAre(HasBuiltIn(
                                "/usr/lib/gcc/x86_64-linux-gnu/4.8/../../../../include/c++/4.8"),
                            HasBuiltIn("/usr/lib/gcc/x86_64-linux-gnu/4.8/../../../../include/"
                                       "c++/4.8/backward"),
                            HasBuiltIn("/usr/lib/gcc/x86_64-linux-gnu/4.8/../../../../include/"
                                       "x86_64-linux-gnu/c++/4.8"),
                            HasBuiltIn(CLANG_RESOURCE_DIR),
                            HasBuiltIn("/usr/local/include"),
                            HasBuiltIn("/usr/lib/gcc/x86_64-linux-gnu/4.8/include"),
                            HasBuiltIn("/usr/include/x86_64-linux-gnu"),
                            HasBuiltIn("/usr/include"),
                            HasBuiltIn("/builtin_path")));
}

TEST_F(HeaderPathFilter, ClangHeadersAndCppIncludesPathsOrderNoVersion)
{
    projectPart.headerPaths = {
        HeaderPath{"C:/Qt/Tools/mingw530_32/i686-w64-mingw32/include", HeaderPathType::BuiltIn},
        HeaderPath{"C:/Qt/Tools/mingw530_32/i686-w64-mingw32/include/c++", HeaderPathType::BuiltIn},
        HeaderPath{"C:/Qt/Tools/mingw530_32/i686-w64-mingw32/include/c++/i686-w64-mingw32",
                   HeaderPathType::BuiltIn},
        HeaderPath{"C:/Qt/Tools/mingw530_32/i686-w64-mingw32/include/c++/backward",
                   HeaderPathType::BuiltIn}};
    projectPart.toolChainTargetTriple = "x86_64-w64-windows-gnu";
    CppTools::HeaderPathFilter filter{projectPart,
                                      CppTools::UseTweakedHeaderPaths::Yes,
                                      "6.0",
                                      CLANG_RESOURCE_DIR};

    filter.process();

    ASSERT_THAT(
        filter.builtInHeaderPaths,
        ElementsAre(HasBuiltIn("C:/Qt/Tools/mingw530_32/i686-w64-mingw32/include/c++"),
                    HasBuiltIn(
                        "C:/Qt/Tools/mingw530_32/i686-w64-mingw32/include/c++/i686-w64-mingw32"),
                    HasBuiltIn("C:/Qt/Tools/mingw530_32/i686-w64-mingw32/include/c++/backward"),
                    HasBuiltIn(CLANG_RESOURCE_DIR),
                    HasBuiltIn("C:/Qt/Tools/mingw530_32/i686-w64-mingw32/include")));
}

TEST_F(HeaderPathFilter, ClangHeadersAndCppIncludesPathsOrderAndroidClang)
{
    projectPart.headerPaths = {
        HeaderPath{"C:/Users/test/AppData/Local/Android/sdk/ndk-"
                   "bundle/sysroot/usr/include/i686-linux-android",
                   HeaderPathType::BuiltIn},
        HeaderPath{"C:/Users/test/AppData/Local/Android/sdk/ndk-bundle/sources/cxx-"
                   "stl/llvm-libc++/include",
                   HeaderPathType::BuiltIn},
        HeaderPath{"C:/Users/test/AppData/Local/Android/sdk/ndk-"
                   "bundle/sources/android/support/include",
                   HeaderPathType::BuiltIn},
        HeaderPath{"C:/Users/test/AppData/Local/Android/sdk/ndk-bundle/sources/cxx-"
                   "stl/llvm-libc++abi/include",
                   HeaderPathType::BuiltIn},
        HeaderPath{"C:/Users/test/AppData/Local/Android/sdk/ndk-bundle/sysroot/usr/include",
                   HeaderPathType::BuiltIn}};
    projectPart.toolChainTargetTriple = "i686-linux-android";
    CppTools::HeaderPathFilter filter{projectPart,
                                      CppTools::UseTweakedHeaderPaths::Yes,
                                      "6.0",
                                      CLANG_RESOURCE_DIR};

    filter.process();

    ASSERT_THAT(
        filter.builtInHeaderPaths,
        ElementsAre(HasBuiltIn("C:/Users/test/AppData/Local/Android/sdk/ndk-"
                               "bundle/sources/cxx-stl/llvm-libc++/include"),
                    HasBuiltIn(
                        "C:/Users/test/AppData/Local/Android/sdk/ndk-"
                        "bundle/sources/cxx-stl/llvm-libc++abi/include"),
                    HasBuiltIn(CLANG_RESOURCE_DIR),
                    HasBuiltIn("C:/Users/test/AppData/Local/Android/sdk/ndk-"
                               "bundle/sysroot/usr/include/i686-linux-android"),
                    HasBuiltIn("C:/Users/test/AppData/Local/Android/sdk/ndk-"
                               "bundle/sources/android/support/include"),
                    HasBuiltIn("C:/Users/test/AppData/Local/Android/sdk/ndk-"
                               "bundle/sysroot/usr/include")));
}

} // namespace
