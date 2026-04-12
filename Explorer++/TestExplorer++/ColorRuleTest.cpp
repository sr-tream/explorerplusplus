// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "pch.h"
#include "ColorRule.h"
#include "ColorRuleModel.h"
#include "ColorRuleModelFactory.h"
#include "GitStatusTracker.h"
#include "ScopedTestDir.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <cstdlib>
#include <fstream>

using namespace testing;

namespace
{

int RunGitCommand(const std::filesystem::path &directoryPath, const std::wstring &args)
{
	std::wstring command =
		L"git -C \"" + directoryPath.wstring() + L"\" " + args + L" >NUL 2>NUL";
	return _wsystem(command.c_str());
}

bool WriteFile(const std::filesystem::path &path, const std::string &contents)
{
	std::ofstream stream(path, std::ios::binary);

	if (!stream)
	{
		return false;
	}

	stream << contents;
	return stream.good();
}

}

class ColorRuleObserverMock
{
public:
	ColorRuleObserverMock(ColorRule *colorRule)
	{
		colorRule->AddUpdatedObserver(
			std::bind_front(&ColorRuleObserverMock::OnColorRuleUpdated, this));
	}

	MOCK_METHOD(void, OnColorRuleUpdated, (ColorRule * colorRule));
};

class ColorRuleTest : public Test
{
protected:
	ColorRuleTest() :
		m_colorRule(L"C++ files", L"*.cpp", true, 0, RGB(0, 0, 128)),
		m_observer(&m_colorRule)
	{
	}

	ColorRule m_colorRule;
	ColorRuleObserverMock m_observer;
};

TEST_F(ColorRuleTest, Update)
{
	EXPECT_CALL(m_observer, OnColorRuleUpdated(&m_colorRule));
	m_colorRule.SetDescription(L"Read-only header files");

	EXPECT_CALL(m_observer, OnColorRuleUpdated(&m_colorRule));
	m_colorRule.SetFilterPattern(L"*.h");

	EXPECT_CALL(m_observer, OnColorRuleUpdated(&m_colorRule));
	m_colorRule.SetFilterPatternCaseInsensitive(false);

	EXPECT_CALL(m_observer, OnColorRuleUpdated(&m_colorRule));
	m_colorRule.SetFilterAttributes(FILE_ATTRIBUTE_READONLY);

	EXPECT_CALL(m_observer, OnColorRuleUpdated(&m_colorRule));
	m_colorRule.SetColor(RGB(44, 44, 44));

	EXPECT_CALL(m_observer, OnColorRuleUpdated(&m_colorRule));
	m_colorRule.SetFilterGitStatus(GitStatus::Modified | GitStatus::Staged);
}

TEST_F(ColorRuleTest, UpdateWithoutChange)
{
	EXPECT_CALL(m_observer, OnColorRuleUpdated(&m_colorRule)).Times(0);
	m_colorRule.SetDescription(m_colorRule.GetDescription());

	EXPECT_CALL(m_observer, OnColorRuleUpdated(&m_colorRule)).Times(0);
	m_colorRule.SetFilterPattern(m_colorRule.GetFilterPattern());

	EXPECT_CALL(m_observer, OnColorRuleUpdated(&m_colorRule)).Times(0);
	m_colorRule.SetFilterPatternCaseInsensitive(m_colorRule.GetFilterPatternCaseInsensitive());

	EXPECT_CALL(m_observer, OnColorRuleUpdated(&m_colorRule)).Times(0);
	m_colorRule.SetFilterAttributes(m_colorRule.GetFilterAttributes());

	EXPECT_CALL(m_observer, OnColorRuleUpdated(&m_colorRule)).Times(0);
	m_colorRule.SetColor(m_colorRule.GetColor());

	EXPECT_CALL(m_observer, OnColorRuleUpdated(&m_colorRule)).Times(0);
	m_colorRule.SetFilterGitStatus(m_colorRule.GetFilterGitStatus());
}

TEST(ColorRuleModelFactoryTest, DefaultRulesUseGitStatuses)
{
	auto model = ColorRuleModelFactory::Create();
	const auto &items = model->GetItems();

	ASSERT_EQ(items.size(), 4u);

	EXPECT_EQ(items[0]->GetDescription(), L"Git modified");
	EXPECT_EQ(items[0]->GetFilterAttributes(), 0u);
	EXPECT_EQ(items[0]->GetFilterGitStatus(), GitStatus::Modified);
	EXPECT_EQ(items[0]->GetColor(), RGB(255, 140, 0));

	EXPECT_EQ(items[1]->GetDescription(), L"Git staged");
	EXPECT_EQ(items[1]->GetFilterAttributes(), 0u);
	EXPECT_EQ(items[1]->GetFilterGitStatus(), GitStatus::Staged);
	EXPECT_EQ(items[1]->GetColor(), RGB(0, 128, 0));

	EXPECT_EQ(items[2]->GetDescription(), L"Git untracked");
	EXPECT_EQ(items[2]->GetFilterAttributes(), 0u);
	EXPECT_EQ(items[2]->GetFilterGitStatus(), GitStatus::Untracked);
	EXPECT_EQ(items[2]->GetColor(), RGB(220, 20, 60));

	EXPECT_EQ(items[3]->GetDescription(), L"Git ignored");
	EXPECT_EQ(items[3]->GetFilterAttributes(), 0u);
	EXPECT_EQ(items[3]->GetFilterGitStatus(), GitStatus::Ignored);
	EXPECT_EQ(items[3]->GetColor(), RGB(128, 128, 128));
}

class GitStatusTrackerIgnoredFolderTest : public Test
{
protected:
	void SetUp() override
	{
		if (_wsystem(L"git --version >NUL 2>NUL") != 0)
		{
			GTEST_SKIP();
		}

		m_repoPath = m_testDir.GetPath();
		ASSERT_EQ(RunGitCommand(m_repoPath, L"init --quiet"), 0);
	}

	int RunGit(const std::wstring &args) const
	{
		return RunGitCommand(m_repoPath, args);
	}

	ScopedTestDir m_testDir;
	std::filesystem::path m_repoPath;
};

TEST_F(GitStatusTrackerIgnoredFolderTest, FolderWithTrackedContentIsNotMarkedIgnored)
{
	std::filesystem::create_directories(m_repoPath / L"mixed");

	ASSERT_TRUE(WriteFile(m_repoPath / L"mixed" / L"tracked.txt", "tracked"));
	ASSERT_EQ(RunGit(L"add \"mixed/tracked.txt\""), 0);
	ASSERT_EQ(RunGit(L"config user.email \"test@example.com\""), 0);
	ASSERT_EQ(RunGit(L"config user.name \"Explorer++ Test\""), 0);
	ASSERT_EQ(RunGit(L"commit --quiet -m \"initial\""), 0);

	ASSERT_TRUE(WriteFile(m_repoPath / L".gitignore", "*.tmp\n"));
	ASSERT_TRUE(WriteFile(m_repoPath / L"mixed" / L"ignored.tmp", "ignored"));

	auto statusMap = GitStatusTracker::GetStatusForDirectory(m_repoPath.wstring());
	auto it = statusMap.find(L"mixed");

	EXPECT_TRUE(it == statusMap.end() || !WI_IsFlagSet(it->second, GitStatus::Ignored));
}

TEST_F(GitStatusTrackerIgnoredFolderTest, FullyIgnoredFolderIsMarkedIgnored)
{
	std::filesystem::create_directories(m_repoPath / L"ignored-folder");

	ASSERT_TRUE(WriteFile(m_repoPath / L".gitignore", "ignored-folder/\n"));
	ASSERT_TRUE(WriteFile(m_repoPath / L"ignored-folder" / L"file.txt", "ignored"));

	auto statusMap = GitStatusTracker::GetStatusForDirectory(m_repoPath.wstring());
	auto it = statusMap.find(L"ignored-folder");

	ASSERT_NE(it, statusMap.end());
	EXPECT_TRUE(WI_IsFlagSet(it->second, GitStatus::Ignored));
}
