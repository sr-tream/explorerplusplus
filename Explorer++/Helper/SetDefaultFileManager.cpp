// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "SetDefaultFileManager.h"
#include "Helper.h"
#include "ProcessHelper.h"
#include "RegistrySettings.h"
#include "ShellHelper.h"
#include <cwctype>
#include <filesystem>
#include <format>
#include <wil/resource.h>

/*
Notes:

 - To replace Explorer for filesystem folders only, the direct directory/drive open command keys
   should point to Explorer++.
 - To replace Explorer for all folders, the folder open/explore keys and the current
   Win+E/opennewwindow CLSID hook should be updated.
 - The folder opennewwindow verb itself should not be overridden. Files doesn't claim that surface
   for its default file manager registration and overriding it can break known-folder launches such
   as Start -> Downloads.
 - The directory open command also shouldn't be overridden in all-folders mode. Leaving it alone
   keeps plain filesystem folder launches from falling through to another app with its own folder
   integration, while the folder keys still route those launches into Explorer++.
 - Shell namespace targets (e.g. Control Panel) may be passed to Explorer++ via these overrides,
   so startup routing needs to hand those back to explorer.exe.
*/

namespace DefaultFileManagerInternal
{
const TCHAR KEY_DIRECTORY_SHELL[] = _T("Software\\Classes\\Directory\\shell");
const TCHAR KEY_DIRECTORY_OPEN_COMMAND[] = _T("Software\\Classes\\Directory\\shell\\open\\command");
const TCHAR KEY_DRIVE_SHELL[] = _T("Software\\Classes\\Drive\\shell");
const TCHAR KEY_DRIVE_OPEN_COMMAND[] = _T("Software\\Classes\\Drive\\shell\\open\\command");
const TCHAR KEY_FOLDER_SHELL[] = _T("Software\\Classes\\Folder\\shell");
const TCHAR KEY_FOLDER_OPEN_COMMAND[] = _T("Software\\Classes\\Folder\\shell\\open\\command");
const TCHAR KEY_FOLDER_EXPLORE_COMMAND[] = _T("Software\\Classes\\Folder\\shell\\explore\\command");
const TCHAR KEY_FOLDER_OPENNEWWINDOW[] = _T("Software\\Classes\\Folder\\shell\\opennewwindow");
const TCHAR KEY_OPENNEWWINDOW_CLSID[] =
	_T("Software\\Classes\\CLSID\\{52205fd8-5dfb-447d-801a-d0b52f2e83e1}\\shell\\opennewwindow");
const TCHAR KEY_OPENNEWWINDOW_CLSID_COMMAND[] =
	_T("Software\\Classes\\CLSID\\{52205fd8-5dfb-447d-801a-d0b52f2e83e1}\\shell\\opennewwindow\\command");
const TCHAR VALUE_DELEGATE_EXECUTE[] = _T("DelegateExecute");
const TCHAR VERB_OPEN[] = _T("open");
const TCHAR SYSTEM_EXPLORER_PATH[] = _T("C:\\Windows\\explorer.exe");

LSTATUS SetAsDefaultFileManagerInternal(DefaultFileManager::ReplaceExplorerMode replacementType,
	const std::wstring &applicationKeyName, const std::wstring &menuText);
LSTATUS RemoveAsDefaultFileManagerInternal(DefaultFileManager::ReplaceExplorerMode replacementType,
	const std::wstring &applicationKeyName);
LSTATUS ApplyRegistryOperation(const DefaultFileManager::RegistryOperation &operation);
std::wstring NormalizeTarget(std::wstring_view target);
bool StartsWithInsensitive(std::wstring_view value, std::wstring_view prefix);
bool IsDriveLikeTarget(std::wstring_view target);
bool IsUncLikeTarget(std::wstring_view target);
bool IsFilesystemShellTarget(std::wstring_view target);
void NotifyShellAssociationsChanged();
}

LSTATUS DefaultFileManager::SetAsDefaultFileManagerFileSystem(
	const std::wstring &applicationKeyName, const std::wstring &menuText)
{
	return DefaultFileManagerInternal::SetAsDefaultFileManagerInternal(
		ReplaceExplorerMode::FileSystem, applicationKeyName, menuText);
}

LSTATUS DefaultFileManager::SetAsDefaultFileManagerAll(const std::wstring &applicationKeyName,
	const std::wstring &menuText)
{
	return DefaultFileManagerInternal::SetAsDefaultFileManagerInternal(ReplaceExplorerMode::All,
		applicationKeyName, menuText);
}

LSTATUS DefaultFileManagerInternal::SetAsDefaultFileManagerInternal(
	DefaultFileManager::ReplaceExplorerMode replacementType, const std::wstring &applicationKeyName,
	const std::wstring &menuText)
{
	TCHAR executable[MAX_PATH];
	GetProcessImageName(GetCurrentProcessId(), executable, std::size(executable));
	UNREFERENCED_PARAMETER(applicationKeyName);
	UNREFERENCED_PARAMETER(menuText);

	auto operations = DefaultFileManager::BuildRegistrationOperations(replacementType, executable);

	for (const auto &operation : operations)
	{
		LSTATUS res = ApplyRegistryOperation(operation);

		if (res != ERROR_SUCCESS)
		{
			return res;
		}
	}

	NotifyShellAssociationsChanged();

	return ERROR_SUCCESS;
}

LSTATUS DefaultFileManager::RemoveAsDefaultFileManagerFileSystem(
	const std::wstring &applicationKeyName)
{
	return DefaultFileManagerInternal::RemoveAsDefaultFileManagerInternal(
		ReplaceExplorerMode::FileSystem, applicationKeyName);
}

LSTATUS DefaultFileManager::RemoveAsDefaultFileManagerAll(const std::wstring &applicationKeyName)
{
	return DefaultFileManagerInternal::RemoveAsDefaultFileManagerInternal(ReplaceExplorerMode::All,
		applicationKeyName);
}

LSTATUS DefaultFileManagerInternal::RemoveAsDefaultFileManagerInternal(
	DefaultFileManager::ReplaceExplorerMode replacementType, const std::wstring &applicationKeyName)
{
	UNREFERENCED_PARAMETER(applicationKeyName);

	auto operations = DefaultFileManager::BuildRemovalOperations(replacementType);

	for (const auto &operation : operations)
	{
		LSTATUS res = ApplyRegistryOperation(operation);

		if (res != ERROR_SUCCESS)
		{
			return res;
		}
	}

	NotifyShellAssociationsChanged();

	return ERROR_SUCCESS;
}

std::vector<DefaultFileManager::RegistryOperation> DefaultFileManager::BuildRegistrationOperations(
	ReplaceExplorerMode replacementType, const std::wstring &applicationPath)
{
	std::vector<RegistryOperation> operations;

	auto addSet = [&operations](const std::wstring &keyPath, const std::wstring &valueName,
				 const std::wstring &value)
	{
		operations.push_back({ RegistryOperationType::SetValue, keyPath, valueName, value });
	};

	auto addDeleteTree = [&operations](const std::wstring &keyPath)
	{
		operations.push_back({ RegistryOperationType::DeleteTree, keyPath, L"", L"" });
	};

	std::wstring commandWithTarget = std::format(L"\"{}\" \"%1\"", applicationPath);
	std::wstring commandWithoutTarget = std::format(L"\"{}\"", applicationPath);

	addSet(DefaultFileManagerInternal::KEY_DRIVE_SHELL, L"", DefaultFileManagerInternal::VERB_OPEN);
	addSet(DefaultFileManagerInternal::KEY_DRIVE_OPEN_COMMAND, L"", commandWithTarget);

	if (replacementType._to_integral() == +ReplaceExplorerMode::FileSystem)
	{
		addSet(DefaultFileManagerInternal::KEY_DIRECTORY_SHELL, L"",
			DefaultFileManagerInternal::VERB_OPEN);
		addSet(DefaultFileManagerInternal::KEY_DIRECTORY_OPEN_COMMAND, L"", commandWithTarget);
	}

	if (replacementType._to_integral() == +ReplaceExplorerMode::All)
	{
		addSet(DefaultFileManagerInternal::KEY_FOLDER_SHELL, L"",
			DefaultFileManagerInternal::VERB_OPEN);
		addSet(DefaultFileManagerInternal::KEY_FOLDER_OPEN_COMMAND, L"", commandWithTarget);
		addSet(DefaultFileManagerInternal::KEY_FOLDER_OPEN_COMMAND,
			DefaultFileManagerInternal::VALUE_DELEGATE_EXECUTE, L"");
		addSet(DefaultFileManagerInternal::KEY_FOLDER_EXPLORE_COMMAND, L"", commandWithTarget);
		addSet(DefaultFileManagerInternal::KEY_FOLDER_EXPLORE_COMMAND,
			DefaultFileManagerInternal::VALUE_DELEGATE_EXECUTE, L"");
		addDeleteTree(DefaultFileManagerInternal::KEY_FOLDER_OPENNEWWINDOW);
		addSet(DefaultFileManagerInternal::KEY_OPENNEWWINDOW_CLSID_COMMAND, L"", commandWithoutTarget);
		addSet(DefaultFileManagerInternal::KEY_OPENNEWWINDOW_CLSID_COMMAND,
			DefaultFileManagerInternal::VALUE_DELEGATE_EXECUTE, L"");
	}

	return operations;
}

std::vector<DefaultFileManager::RegistryOperation> DefaultFileManager::BuildRemovalOperations(
	ReplaceExplorerMode replacementType)
{
	std::vector<RegistryOperation> operations;

	auto addDeleteValue = [&operations](const std::wstring &keyPath)
	{
		operations.push_back({ RegistryOperationType::DeleteValue, keyPath, L"", L"" });
	};

	auto addDeleteTree = [&operations](const std::wstring &keyPath)
	{
		operations.push_back({ RegistryOperationType::DeleteTree, keyPath, L"", L"" });
	};

	addDeleteValue(DefaultFileManagerInternal::KEY_DIRECTORY_SHELL);
	addDeleteValue(DefaultFileManagerInternal::KEY_DRIVE_SHELL);
	addDeleteTree(DefaultFileManagerInternal::KEY_DIRECTORY_OPEN_COMMAND);
	addDeleteTree(DefaultFileManagerInternal::KEY_DRIVE_OPEN_COMMAND);

	if (replacementType._to_integral() == +ReplaceExplorerMode::All)
	{
		addDeleteValue(DefaultFileManagerInternal::KEY_FOLDER_SHELL);
		addDeleteTree(DefaultFileManagerInternal::KEY_FOLDER_OPEN_COMMAND);
		addDeleteTree(DefaultFileManagerInternal::KEY_FOLDER_EXPLORE_COMMAND);
		addDeleteTree(DefaultFileManagerInternal::KEY_FOLDER_OPENNEWWINDOW);
		addDeleteTree(DefaultFileManagerInternal::KEY_OPENNEWWINDOW_CLSID);
	}

	return operations;
}

bool DefaultFileManagerInternal::StartsWithInsensitive(std::wstring_view value,
	std::wstring_view prefix)
{
	if (value.size() < prefix.size())
	{
		return false;
	}

	for (size_t i = 0; i < prefix.size(); i++)
	{
		if (towlower(value[i]) != towlower(prefix[i]))
		{
			return false;
		}
	}

	return true;
}

std::wstring DefaultFileManagerInternal::NormalizeTarget(std::wstring_view target)
{
	std::wstring normalized(target);

	while (!normalized.empty() && normalized.back() == L'\0')
	{
		normalized.pop_back();
	}

	while (normalized.size() >= 2 && normalized.ends_with(L"\\0"))
	{
		normalized.resize(normalized.size() - 2);
	}

	return normalized;
}

bool DefaultFileManager::IsShellLikeTarget(std::wstring_view target)
{
	auto normalized = DefaultFileManagerInternal::NormalizeTarget(target);

	if (normalized.empty())
	{
		return false;
	}

	return DefaultFileManagerInternal::StartsWithInsensitive(normalized, L"shell:")
		|| DefaultFileManagerInternal::StartsWithInsensitive(normalized, L"shell::")
		|| DefaultFileManagerInternal::StartsWithInsensitive(normalized, L"::")
		|| DefaultFileManagerInternal::StartsWithInsensitive(normalized, L"ms-")
		|| DefaultFileManagerInternal::StartsWithInsensitive(normalized, L"control:")
		|| DefaultFileManagerInternal::StartsWithInsensitive(normalized, L"search-ms:")
		|| DefaultFileManagerInternal::StartsWithInsensitive(normalized, L"http:")
		|| DefaultFileManagerInternal::StartsWithInsensitive(normalized, L"https:")
		|| DefaultFileManagerInternal::StartsWithInsensitive(normalized, L"ftp:")
		|| normalized.find(L"::{") != std::wstring::npos;
}

bool DefaultFileManagerInternal::IsDriveLikeTarget(std::wstring_view target)
{
	return target.size() >= 2
		&& ((target[0] >= L'A' && target[0] <= L'Z') || (target[0] >= L'a' && target[0] <= L'z'))
		&& target[1] == L':'
		&& (target.size() == 2 || target[2] == L'\\' || target[2] == L'/');
}

bool DefaultFileManagerInternal::IsUncLikeTarget(std::wstring_view target)
{
	return target.size() >= 2 && target[0] == L'\\' && target[1] == L'\\';
}

bool DefaultFileManagerInternal::IsFilesystemShellTarget(std::wstring_view target)
{
	auto normalized = NormalizeTarget(target);

	if (!StartsWithInsensitive(normalized, L"shell:"))
	{
		return false;
	}

	HRESULT comInitHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

	if (FAILED(comInitHr) && comInitHr != RPC_E_CHANGED_MODE)
	{
		return false;
	}

	unique_pidl_absolute pidl;
	HRESULT hr = ParseDisplayNameForNavigation(normalized, pidl);

	if (FAILED(hr) || !pidl)
	{
		if (SUCCEEDED(comInitHr))
		{
			CoUninitialize();
		}

		return false;
	}

	SFGAOF attributes = SFGAO_FILESYSTEM;
	hr = GetItemAttributes(pidl.get(), &attributes);

	if (SUCCEEDED(comInitHr))
	{
		CoUninitialize();
	}

	if (FAILED(hr))
	{
		return false;
	}

	return WI_IsFlagSet(attributes, SFGAO_FILESYSTEM);
}

void DefaultFileManagerInternal::NotifyShellAssociationsChanged()
{
	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

bool DefaultFileManager::IsFilesystemLikeTarget(std::wstring_view target)
{
	auto normalized = DefaultFileManagerInternal::NormalizeTarget(target);

	if (normalized.empty())
	{
		return true;
	}

	std::error_code error;

	if (std::filesystem::exists(normalized, error))
	{
		return true;
	}

	return DefaultFileManagerInternal::IsDriveLikeTarget(normalized)
		|| DefaultFileManagerInternal::IsUncLikeTarget(normalized);
}

DefaultFileManager::TargetRoute DefaultFileManager::PickTargetRoute(std::wstring_view target)
{
	auto normalized = DefaultFileManagerInternal::NormalizeTarget(target);

	if (normalized.empty())
	{
		return TargetRoute::ExplorerPlusPlus;
	}

	if (DefaultFileManagerInternal::IsFilesystemShellTarget(normalized))
	{
		return TargetRoute::ExplorerPlusPlus;
	}

	if (IsShellLikeTarget(normalized))
	{
		return TargetRoute::SystemExplorer;
	}

	if (IsFilesystemLikeTarget(normalized))
	{
		return TargetRoute::ExplorerPlusPlus;
	}

	return TargetRoute::SystemExplorer;
}

BOOL DefaultFileManager::LaunchTargetInSystemExplorer(const std::wstring &target)
{
	auto normalized = DefaultFileManagerInternal::NormalizeTarget(target);
	std::wstring parameters;

	if (!normalized.empty())
	{
		parameters = std::format(L"\"{}\"", normalized);
	}

	return LaunchProcess(nullptr, DefaultFileManagerInternal::SYSTEM_EXPLORER_PATH, parameters, L"",
		LaunchProcessFlags::None);
}

LSTATUS DefaultFileManagerInternal::ApplyRegistryOperation(
	const DefaultFileManager::RegistryOperation &operation)
{
	switch (operation.type)
	{
	case DefaultFileManager::RegistryOperationType::SetValue:
	{
		wil::unique_hkey key;
		LSTATUS res = RegCreateKeyEx(HKEY_CURRENT_USER, operation.keyPath.c_str(), 0, nullptr,
			REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, nullptr);

		if (res != ERROR_SUCCESS)
		{
			return res;
		}

		return RegistrySettings::SaveString(key.get(), operation.valueName, operation.value);
	}

	case DefaultFileManager::RegistryOperationType::DeleteValue:
	{
		wil::unique_hkey key;
		LSTATUS res =
			RegOpenKeyEx(HKEY_CURRENT_USER, operation.keyPath.c_str(), 0, KEY_WRITE, &key);

		if (res == ERROR_FILE_NOT_FOUND)
		{
			return ERROR_SUCCESS;
		}

		if (res != ERROR_SUCCESS)
		{
			return res;
		}

		res = RegDeleteValue(key.get(),
			operation.valueName.empty() ? nullptr : operation.valueName.c_str());

		return (res == ERROR_SUCCESS || res == ERROR_FILE_NOT_FOUND) ? ERROR_SUCCESS : res;
	}

	case DefaultFileManager::RegistryOperationType::DeleteTree:
	{
		LSTATUS res = SHDeleteKey(HKEY_CURRENT_USER, operation.keyPath.c_str());
		return (res == ERROR_SUCCESS || res == ERROR_FILE_NOT_FOUND) ? ERROR_SUCCESS : res;
	}
	}

	return ERROR_INVALID_PARAMETER;
}
