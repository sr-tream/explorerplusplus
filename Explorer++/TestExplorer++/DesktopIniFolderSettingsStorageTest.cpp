// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "pch.h"
#include "ShellBrowser/DesktopIniFolderSettingsStorage.h"
#include <gtest/gtest.h>

using namespace testing;

class DesktopIniFolderSettingsStorageTest : public Test
{
protected:
	void SetUp() override
	{
		wchar_t tempPath[MAX_PATH];
		DWORD tempPathLength = GetTempPath(ARRAYSIZE(tempPath), tempPath);
		ASSERT_GT(tempPathLength, 0u);
		ASSERT_LT(tempPathLength, ARRAYSIZE(tempPath));

		wchar_t tempFileName[MAX_PATH];
		UINT result = GetTempFileName(tempPath, L"epp", 0, tempFileName);
		ASSERT_NE(result, 0u);

		ASSERT_NE(DeleteFile(tempFileName), 0);
		ASSERT_NE(CreateDirectory(tempFileName, nullptr), 0);

		m_testDirectory = tempFileName;
	}

	void TearDown() override
	{
		if (m_testDirectory.empty())
		{
			return;
		}

		auto desktopIniPath = GetDesktopIniPath();
		auto attributes = GetFileAttributes(desktopIniPath.c_str());

		if (attributes != INVALID_FILE_ATTRIBUTES)
		{
			SetFileAttributes(desktopIniPath.c_str(), FILE_ATTRIBUTE_NORMAL);
		}

		std::error_code error;
		std::filesystem::remove_all(m_testDirectory, error);
	}

	std::wstring GetDirectory() const
	{
		return m_testDirectory.wstring();
	}

	std::filesystem::path GetDesktopIniPath() const
	{
		return m_testDirectory / L"desktop.ini";
	}

private:
	std::filesystem::path m_testDirectory;
};

TEST_F(DesktopIniFolderSettingsStorageTest, MissingDesktopIniReturnsNullopt)
{
	FolderSettings fallbackSettings;
	auto folderSettings =
		DesktopIniFolderSettingsStorage::LoadFolderSettings(GetDirectory(), fallbackSettings);
	EXPECT_FALSE(folderSettings);
}

TEST_F(DesktopIniFolderSettingsStorageTest, SaveAndLoadRoundTrip)
{
	FolderSettings folderSettingsToSave;
	folderSettingsToSave.viewMode = ViewMode::Details;
	folderSettingsToSave.sortMode = SortMode::DateModified;
	folderSettingsToSave.sortDirection = SortDirection::Descending;
	folderSettingsToSave.groupMode = SortMode::Type;

	EXPECT_TRUE(
		DesktopIniFolderSettingsStorage::SaveFolderSettings(GetDirectory(), folderSettingsToSave));

	auto attributes = GetFileAttributes(GetDesktopIniPath().c_str());
	ASSERT_NE(attributes, INVALID_FILE_ATTRIBUTES);
	EXPECT_TRUE(WI_IsFlagSet(attributes, FILE_ATTRIBUTE_HIDDEN));
	EXPECT_TRUE(WI_IsFlagSet(attributes, FILE_ATTRIBUTE_SYSTEM));

	FolderSettings fallbackSettings;
	fallbackSettings.viewMode = ViewMode::Icons;
	fallbackSettings.sortMode = SortMode::Name;
	fallbackSettings.sortDirection = SortDirection::Ascending;
	fallbackSettings.groupMode = SortMode::Name;
	fallbackSettings.autoArrangeEnabled = false;
	fallbackSettings.showHidden = false;
	fallbackSettings.filterEnabled = true;
	fallbackSettings.filterCaseSensitive = true;
	fallbackSettings.filter = L"*.txt";

	auto loadedFolderSettings =
		DesktopIniFolderSettingsStorage::LoadFolderSettings(GetDirectory(), fallbackSettings);
	ASSERT_TRUE(loadedFolderSettings);
	EXPECT_EQ(loadedFolderSettings->viewMode, folderSettingsToSave.viewMode);
	EXPECT_EQ(loadedFolderSettings->sortMode, folderSettingsToSave.sortMode);
	EXPECT_EQ(loadedFolderSettings->sortDirection, folderSettingsToSave.sortDirection);
	EXPECT_EQ(loadedFolderSettings->groupMode, folderSettingsToSave.groupMode);
	EXPECT_EQ(loadedFolderSettings->autoArrangeEnabled, fallbackSettings.autoArrangeEnabled);
	EXPECT_EQ(loadedFolderSettings->showHidden, fallbackSettings.showHidden);
	EXPECT_EQ(loadedFolderSettings->filterEnabled, fallbackSettings.filterEnabled);
	EXPECT_EQ(loadedFolderSettings->filterCaseSensitive, fallbackSettings.filterCaseSensitive);
	EXPECT_EQ(loadedFolderSettings->filter, fallbackSettings.filter);
}

TEST_F(DesktopIniFolderSettingsStorageTest, InvalidValuesAreIgnored)
{
	const wchar_t sectionData[] =
		L"ViewMode=9999\0SortMode=3\0SortAscending=2\0GroupMode=invalid\0\0";
	ASSERT_NE(WritePrivateProfileSection(L"Explorer++", sectionData, GetDesktopIniPath().c_str()),
		0);

	FolderSettings fallbackSettings;
	fallbackSettings.viewMode = ViewMode::List;
	fallbackSettings.sortMode = SortMode::Name;
	fallbackSettings.sortDirection = SortDirection::Ascending;
	fallbackSettings.groupMode = SortMode::Type;
	fallbackSettings.autoArrangeEnabled = false;
	fallbackSettings.filter = L"keep";

	auto loadedFolderSettings =
		DesktopIniFolderSettingsStorage::LoadFolderSettings(GetDirectory(), fallbackSettings);
	ASSERT_TRUE(loadedFolderSettings);
	EXPECT_EQ(loadedFolderSettings->viewMode, fallbackSettings.viewMode);
	EXPECT_EQ(loadedFolderSettings->sortMode, +SortMode::Size);
	EXPECT_EQ(loadedFolderSettings->sortDirection, fallbackSettings.sortDirection);
	EXPECT_EQ(loadedFolderSettings->groupMode, fallbackSettings.groupMode);
	EXPECT_EQ(loadedFolderSettings->autoArrangeEnabled, fallbackSettings.autoArrangeEnabled);
	EXPECT_EQ(loadedFolderSettings->filter, fallbackSettings.filter);
}
