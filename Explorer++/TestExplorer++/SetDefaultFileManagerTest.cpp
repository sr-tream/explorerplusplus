// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "pch.h"
#include "../Helper/SetDefaultFileManager.h"
#include <algorithm>
#include <gtest/gtest.h>

using namespace DefaultFileManager;

namespace
{

bool HasSetOperation(const std::vector<RegistryOperation> &operations, std::wstring_view keyPath,
	std::wstring_view valueName, std::wstring_view value)
{
	return std::ranges::any_of(operations,
		[keyPath, valueName, value](const auto &operation)
		{
			return operation.type == RegistryOperationType::SetValue
				&& operation.keyPath == keyPath && operation.valueName == valueName
				&& operation.value == value;
		});
}

bool HasDeleteTreeOperation(const std::vector<RegistryOperation> &operations,
	std::wstring_view keyPath)
{
	return std::ranges::any_of(operations,
		[keyPath](const auto &operation)
		{ return operation.type == RegistryOperationType::DeleteTree && operation.keyPath == keyPath; });
}

}

TEST(SetDefaultFileManagerTest, FileSystemRegistrationUsesFilesystemKeysOnly)
{
	std::wstring applicationPath = L"C:\\Program Files\\$bin\\Explorer++.exe";
	auto operations = BuildRegistrationOperations(ReplaceExplorerMode::FileSystem, applicationPath);
	std::wstring commandWithTarget = L"\"" + applicationPath + L"\" \"%1\"";

	EXPECT_TRUE(HasSetOperation(operations, L"Software\\Classes\\Directory\\shell", L"", L"open"));
	EXPECT_TRUE(HasSetOperation(operations, L"Software\\Classes\\Directory\\shell\\open\\command",
		L"", commandWithTarget));
	EXPECT_TRUE(HasSetOperation(operations, L"Software\\Classes\\Drive\\shell", L"", L"open"));
	EXPECT_TRUE(HasSetOperation(operations, L"Software\\Classes\\Drive\\shell\\open\\command", L"",
		commandWithTarget));
	EXPECT_FALSE(std::ranges::any_of(operations,
		[](const auto &operation)
		{
			return operation.keyPath.find(L"Software\\Classes\\Folder\\shell") != std::wstring::npos
				|| operation.keyPath.find(L"Software\\Classes\\CLSID\\") != std::wstring::npos;
		}));
}

TEST(SetDefaultFileManagerTest, AllRegistrationIncludesFolderAndWinEHooks)
{
	std::wstring applicationPath = L"C:\\Program Files\\$bin\\Explorer++.exe";
	auto operations = BuildRegistrationOperations(ReplaceExplorerMode::All, applicationPath);
	std::wstring commandWithTarget = L"\"" + applicationPath + L"\" \"%1\"";
	std::wstring commandWithoutTarget = L"\"" + applicationPath + L"\"";
	std::wstring driveShellKey = L"Software\\Classes\\Drive\\shell";
	std::wstring driveOpenCommandKey = L"Software\\Classes\\Drive\\shell\\open\\command";
	std::wstring directoryKeyPrefix = L"Software\\Classes\\Directory\\shell";
	std::wstring folderOpenNewWindowKey = L"Software\\Classes\\Folder\\shell\\opennewwindow";
	std::wstring winEKey =
		L"Software\\Classes\\CLSID\\{52205fd8-5dfb-447d-801a-d0b52f2e83e1}\\shell\\opennewwindow\\command";

	EXPECT_TRUE(HasSetOperation(operations, driveShellKey, L"", L"open"));
	EXPECT_TRUE(HasSetOperation(operations, driveOpenCommandKey, L"", commandWithTarget));
	EXPECT_FALSE(std::ranges::any_of(operations,
		[directoryKeyPrefix](const auto &operation)
		{ return operation.keyPath.find(directoryKeyPrefix) == 0; }));
	EXPECT_TRUE(HasSetOperation(operations, L"Software\\Classes\\Folder\\shell", L"", L"open"));
	EXPECT_TRUE(HasSetOperation(operations, L"Software\\Classes\\Folder\\shell\\open\\command", L"",
		commandWithTarget));
	EXPECT_TRUE(HasSetOperation(operations, L"Software\\Classes\\Folder\\shell\\open\\command",
		L"DelegateExecute", L""));
	EXPECT_TRUE(HasSetOperation(operations,
		L"Software\\Classes\\Folder\\shell\\explore\\command", L"", commandWithTarget));
	EXPECT_TRUE(HasDeleteTreeOperation(operations, folderOpenNewWindowKey));
	EXPECT_FALSE(std::ranges::any_of(operations,
		[folderOpenNewWindowKey](const auto &operation)
		{
			return operation.type == RegistryOperationType::SetValue
				&& operation.keyPath.find(folderOpenNewWindowKey) == 0;
		}));
	EXPECT_TRUE(HasSetOperation(operations, winEKey, L"", commandWithoutTarget));
	EXPECT_TRUE(HasSetOperation(operations, winEKey, L"DelegateExecute", L""));
}

TEST(SetDefaultFileManagerTest, AllRemovalIncludesWinEAndFolderKeys)
{
	auto operations = BuildRemovalOperations(ReplaceExplorerMode::All);

	EXPECT_TRUE(HasDeleteTreeOperation(operations, L"Software\\Classes\\Folder\\shell\\open\\command"));
	EXPECT_TRUE(
		HasDeleteTreeOperation(operations, L"Software\\Classes\\Folder\\shell\\explore\\command"));
	EXPECT_TRUE(HasDeleteTreeOperation(operations,
		L"Software\\Classes\\Folder\\shell\\opennewwindow"));
	EXPECT_TRUE(HasDeleteTreeOperation(operations,
		L"Software\\Classes\\CLSID\\{52205fd8-5dfb-447d-801a-d0b52f2e83e1}\\shell\\opennewwindow"));
}

TEST(SetDefaultFileManagerTest, PickTargetRouteMatchesRouterBehavior)
{
	EXPECT_EQ(PickTargetRoute(L""), TargetRoute::ExplorerPlusPlus);
	EXPECT_EQ(PickTargetRoute(L"C:\\Users\\SR_team"), TargetRoute::ExplorerPlusPlus);
	EXPECT_EQ(PickTargetRoute(L"Z:"), TargetRoute::ExplorerPlusPlus);
	EXPECT_EQ(PickTargetRoute(L"\\\\server\\share"), TargetRoute::ExplorerPlusPlus);
	EXPECT_EQ(PickTargetRoute(L"shell:Downloads"), TargetRoute::ExplorerPlusPlus);
	EXPECT_EQ(PickTargetRoute(L"shell:::{26EE0668-A00A-44D7-9371-BEB064C98683}"),
		TargetRoute::SystemExplorer);
	EXPECT_EQ(PickTargetRoute(L"control:"), TargetRoute::SystemExplorer);
	EXPECT_EQ(PickTargetRoute(L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}"),
		TargetRoute::SystemExplorer);
}
