/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "fakeprocess.h"

#include "mockclangpathwatcher.h"
#include "mockpchmanagerclient.h"
#include "testenvironment.h"

#include <refactoringdatabaseinitializer.h>
#include <filepathcaching.h>
#include <generatedfiles.h>
#include <pchcreator.h>
#include <precompiledheadersupdatedmessage.h>
#include <progressmessage.h>

#include <sqlitedatabase.h>

#include <QFileInfo>

namespace {

using ClangBackEnd::FilePath;
using ClangBackEnd::FilePathId;
using ClangBackEnd::FilePathIds;
using ClangBackEnd::FilePathView;
using ClangBackEnd::GeneratedFiles;
using ClangBackEnd::IdPaths;
using ClangBackEnd::IncludeSearchPathType;
using ClangBackEnd::PchTask;
using ClangBackEnd::ProjectPartPch;
using ClangBackEnd::SourceEntries;
using ClangBackEnd::SourceEntry;
using ClangBackEnd::SourceType;
using ClangBackEnd::V2::FileContainer;
using ClangBackEnd::ProjectPartContainer;

using Utils::PathString;
using Utils::SmallString;

using UnitTests::EndsWith;

MATCHER_P2(HasIdAndType,
           sourceId,
           sourceType,
           std::string(negation ? "hasn't" : "has")
               + PrintToString(ClangBackEnd::SourceEntry(sourceId, sourceType, -1)))
{
    const ClangBackEnd::SourceEntry &entry = arg;
    return entry.sourceId == sourceId && entry.sourceType == sourceType;
}

class PchCreator: public ::testing::Test
{
protected:
    PchCreator()
    {
        creator.setUnsavedFiles({generatedFile});
    }

    ClangBackEnd::FilePathId id(ClangBackEnd::FilePathView path)
    {
        return creator.filePathCache().filePathId(path);
    }

protected:
    Sqlite::Database database{":memory:", Sqlite::JournalMode::Memory};
    ClangBackEnd::RefactoringDatabaseInitializer<Sqlite::Database> databaseInitializer{database};
    FilePath main1Path = TESTDATA_DIR "/builddependencycollector/project/main3.cpp";
    FilePath main2Path = TESTDATA_DIR "/builddependencycollector/project/main2.cpp";
    FilePath header1Path = TESTDATA_DIR "/builddependencycollector/project/header1.h";
    FilePath header2Path = TESTDATA_DIR "/builddependencycollector/project/header2.h";
    Utils::SmallStringView generatedFileName = "builddependencycollector/project/generated_file.h";
    FilePath generatedFilePath = TESTDATA_DIR "/builddependencycollector/project/generated_file.h";
    TestEnvironment environment;
    FileContainer generatedFile{{TESTDATA_DIR, generatedFileName}, "#pragma once", {}};
    NiceMock<MockPchManagerClient> mockPchManagerClient;
    NiceMock<MockClangPathWatcher> mockClangPathWatcher;
    ClangBackEnd::PchCreator creator{environment, database, mockPchManagerClient, mockClangPathWatcher};
    PchTask pchTask1{
        "project1",
        {id(TESTDATA_DIR "/builddependencycollector/project/header2.h"),
         id(TESTDATA_DIR "/builddependencycollector/external/external1.h"),
         id(TESTDATA_DIR "/builddependencycollector/external/external2.h")},
        {id(TESTDATA_DIR "/builddependencycollector/project/header2.h"),
         id(TESTDATA_DIR "/builddependencycollector/external/external1.h"),
         id(TESTDATA_DIR "/builddependencycollector/external/external2.h")},
        {},
        {},
        {},
        {{TESTDATA_DIR "/builddependencycollector/system", 2, IncludeSearchPathType::BuiltIn},
         {TESTDATA_DIR "/builddependencycollector/external", 1, IncludeSearchPathType::System}},
        {{TESTDATA_DIR "/builddependencycollector/project", 1, IncludeSearchPathType::User}},
    };
};
using PchCreatorSlowTest = PchCreator;
using PchCreatorVerySlowTest = PchCreator;

TEST_F(PchCreator, CreateProjectPartPchFileContent)
{
    auto content = creator.generatePchIncludeFileContent(pchTask1.includes);

    ASSERT_THAT(std::string(content),
                AllOf(HasSubstr("#include \"" TESTDATA_DIR "/builddependencycollector/project/header2.h\"\n"),
                      HasSubstr("#include \"" TESTDATA_DIR "/builddependencycollector/external/external1.h\"\n"),
                      HasSubstr("#include \"" TESTDATA_DIR "/builddependencycollector/external/external2.h\"\n")));
}

TEST_F(PchCreator, CreatePchIncludeFile)
{
    auto content = creator.generatePchIncludeFileContent(pchTask1.includes);
    auto pchIncludeFilePath = creator.generatePchHeaderFilePath();
    auto file = creator.generateFileWithContent(pchIncludeFilePath, content);
    file->open(QIODevice::ReadOnly);

    auto fileContent = file->readAll();

    ASSERT_THAT(fileContent.toStdString(),
                AllOf(HasSubstr("#include \"" TESTDATA_DIR "/builddependencycollector/project/header2.h\"\n"),
                      HasSubstr("#include \"" TESTDATA_DIR "/builddependencycollector/external/external1.h\"\n"),
                      HasSubstr("#include \"" TESTDATA_DIR "/builddependencycollector/external/external2.h\"\n")));
}

TEST_F(PchCreator, CreateProjectPartClangCompilerArguments)
{
    auto arguments = creator.generateClangCompilerArguments(std::move(pchTask1),
                                                            "project.h",
                                                            "project.pch");

    ASSERT_THAT(arguments,
                ElementsAre("clang++",
                            "-x",
                            "c++-header",
                            "-std=c++98",
                            "-nostdinc",
                            "-nostdlibinc",
                            "-I",
                            TESTDATA_DIR "/builddependencycollector/project",
                            "-isystem",
                            TESTDATA_DIR "/builddependencycollector/external",
                            "-isystem",
                            TESTDATA_DIR "/builddependencycollector/system",
                            "-o",
                            "project.pch",
                            "project.h"));
}

TEST_F(PchCreator, CreateProjectPartClangCompilerArgumentsWithSystemPch)
{
    pchTask1.systemPchPath = "system.pch";

    auto arguments = creator.generateClangCompilerArguments(std::move(pchTask1),
                                                            "project.h",
                                                            "project.pch");

    ASSERT_THAT(arguments,
                ElementsAre("clang++",
                            "-x",
                            "c++-header",
                            "-std=c++98",
                            "-nostdinc",
                            "-nostdlibinc",
                            "-I",
                            TESTDATA_DIR "/builddependencycollector/project",
                            "-isystem",
                            TESTDATA_DIR "/builddependencycollector/external",
                            "-isystem",
                            TESTDATA_DIR "/builddependencycollector/system",
                            "-Xclang",
                            "-include-pch",
                            "-Xclang",
                            "system.pch",
                            "-o",
                            "project.pch",
                            "project.h"));
}

TEST_F(PchCreatorVerySlowTest, ProjectPartPchsSendToPchManagerClient)
{
    creator.generatePch(std::move(pchTask1));

    EXPECT_CALL(mockPchManagerClient,
                precompiledHeadersUpdated(
                    Field(&ClangBackEnd::PrecompiledHeadersUpdatedMessage::projectPartPchs,
                          ElementsAre(Eq(creator.projectPartPch())))));

    creator.doInMainThreadAfterFinished();
}

TEST_F(PchCreatorVerySlowTest, AllIncludesAreWatchedAfterSucess)
{
    creator.generatePch(std::move(pchTask1));

    EXPECT_CALL(
        mockClangPathWatcher,
        updateIdPaths(ElementsAre(
            AllOf(Field(&ClangBackEnd::IdPaths::id, "project1"),
                  Field(&ClangBackEnd::IdPaths::filePathIds,
                        UnorderedElementsAre(
                            id(TESTDATA_DIR "/builddependencycollector/project/header2.h"),
                            id(TESTDATA_DIR "/builddependencycollector/external/external1.h"),
                            id(TESTDATA_DIR "/builddependencycollector/external/external2.h")))))));

    creator.doInMainThreadAfterFinished();
}


TEST_F(PchCreatorVerySlowTest, AllIncludesAreNotWatchedAfterFail)
{
    pchTask1.systemIncludeSearchPaths = {};
    pchTask1.projectIncludeSearchPaths = {};
    creator.generatePch(std::move(pchTask1));

    EXPECT_CALL(mockClangPathWatcher,
                updateIdPaths(
                    ElementsAre(AllOf(Field(&ClangBackEnd::IdPaths::id, "project1"),
                                      Field(&ClangBackEnd::IdPaths::filePathIds, IsEmpty())))));

    creator.doInMainThreadAfterFinished();
}

TEST_F(PchCreatorVerySlowTest, ProjectPartPchForCreatesPchForPchTask)
{
    creator.generatePch(std::move(pchTask1));

    ASSERT_THAT(creator.projectPartPch(),
                AllOf(Field(&ProjectPartPch::projectPartId, Eq("project1")),
                      Field(&ProjectPartPch::pchPath, Not(IsEmpty())),
                      Field(&ProjectPartPch::lastModified, Not(Eq(-1)))));
}

TEST_F(PchCreatorVerySlowTest, ProjectPartPchCleared)
{
    creator.generatePch(std::move(pchTask1));

    creator.clear();

    ASSERT_THAT(creator.projectPartPch(), ClangBackEnd::ProjectPartPch{});
}

TEST_F(PchCreatorVerySlowTest, ClangToolCleared)
{
    creator.generatePch(std::move(pchTask1));

    creator.clear();

    ASSERT_TRUE(creator.clangTool().isClean());
}

TEST_F(PchCreatorVerySlowTest, FaultyProjectPartPchForCreatesFaultyPchForPchTask)
{
    PchTask faultyPchTask{"faultyProjectPart",
                          {id(TESTDATA_DIR "/builddependencycollector/project/faulty.cpp")},
                          {},
                          {{"DEFINE", "1", 1}},
                          {},
                          {},
                          {{TESTDATA_DIR "/builddependencycollector/external", 1, IncludeSearchPathType::System}},
                          {{TESTDATA_DIR "/builddependencycollector/project", 1, IncludeSearchPathType::User}}};

    creator.generatePch(std::move(faultyPchTask));

    ASSERT_THAT(creator.projectPartPch(),
                AllOf(Field(&ProjectPartPch::projectPartId, Eq("faultyProjectPart")),
                      Field(&ProjectPartPch::pchPath, IsEmpty()),
                      Field(&ProjectPartPch::lastModified, Eq(-1))));
}

TEST_F(PchCreatorVerySlowTest, GeneratedFile)
{
    creator.clear();

    creator.setUnsavedFiles({generatedFile});

    ASSERT_FALSE(creator.clangTool().isClean());
}
}
