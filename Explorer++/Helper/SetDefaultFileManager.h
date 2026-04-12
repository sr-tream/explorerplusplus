// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include "BetterEnumsWrapper.h"
#include <string>
#include <string_view>
#include <vector>

namespace DefaultFileManager
{

// clang-format off
BETTER_ENUM(ReplaceExplorerMode, int,
	None = 1,
	FileSystem = 2,
	All = 3
)
// clang-format on

enum class RegistryOperationType
{
	SetValue,
	DeleteValue,
	DeleteTree
};

struct RegistryOperation
{
	RegistryOperationType type;
	std::wstring keyPath;
	std::wstring valueName;
	std::wstring value;
};

enum class TargetRoute
{
	ExplorerPlusPlus,
	SystemExplorer
};

LSTATUS SetAsDefaultFileManagerFileSystem(const std::wstring &applicationKeyName,
	const std::wstring &menuText);
LSTATUS SetAsDefaultFileManagerAll(const std::wstring &applicationKeyName,
	const std::wstring &menuText);
LSTATUS RemoveAsDefaultFileManagerFileSystem(const std::wstring &applicationKeyName);
LSTATUS RemoveAsDefaultFileManagerAll(const std::wstring &applicationKeyName);
std::vector<RegistryOperation> BuildRegistrationOperations(ReplaceExplorerMode replacementType,
	const std::wstring &applicationPath);
std::vector<RegistryOperation> BuildRemovalOperations(ReplaceExplorerMode replacementType);
bool IsShellLikeTarget(std::wstring_view target);
bool IsFilesystemLikeTarget(std::wstring_view target);
TargetRoute PickTargetRoute(std::wstring_view target);
BOOL LaunchTargetInSystemExplorer(const std::wstring &target);

}

// This is needed to be able to use the enum as a key in std::unordered_map.
BETTER_ENUMS_DECLARE_STD_HASH(DefaultFileManager::ReplaceExplorerMode);
