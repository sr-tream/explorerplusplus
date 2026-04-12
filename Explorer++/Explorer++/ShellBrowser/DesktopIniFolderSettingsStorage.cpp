// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "DesktopIniFolderSettingsStorage.h"
#include "../Helper/StringHelper.h"

namespace DesktopIniFolderSettingsStorage
{

namespace
{

constexpr wchar_t DESKTOP_INI_FILENAME[] = L"desktop.ini";
constexpr wchar_t SECTION_NAME[] = L"Explorer++";
constexpr wchar_t SETTING_VIEW_MODE[] = L"ViewMode";
constexpr wchar_t SETTING_SORT_MODE[] = L"SortMode";
constexpr wchar_t SETTING_SORT_ASCENDING[] = L"SortAscending";
constexpr wchar_t SETTING_GROUP_MODE[] = L"GroupMode";

std::wstring BuildDesktopIniPath(const std::wstring &directory)
{
	return (std::filesystem::path(directory) / DESKTOP_INI_FILENAME).wstring();
}

void LogInvalidSetting(const std::wstring &desktopIniPath, const std::wstring &settingName,
	const std::wstring &value)
{
	LOG(WARNING) << std::format("Ignoring invalid {} value \"{}\" in \"{}\".",
		wstrToUtf8Str(settingName), wstrToUtf8Str(value), wstrToUtf8Str(desktopIniPath));
}

bool HasExplorerPlusPlusSection(const std::wstring &desktopIniPath)
{
	wchar_t buffer[64];
	return GetPrivateProfileSection(SECTION_NAME, buffer, ARRAYSIZE(buffer), desktopIniPath.c_str())
		!= 0;
}

std::optional<std::wstring> ReadSetting(const std::wstring &desktopIniPath, const wchar_t *setting)
{
	wchar_t buffer[32];
	DWORD copied = GetPrivateProfileString(SECTION_NAME, setting, L"", buffer, ARRAYSIZE(buffer),
		desktopIniPath.c_str());

	if (copied == 0)
	{
		return std::nullopt;
	}

	return std::wstring(buffer, copied);
}

bool TryParseInteger(const std::wstring &text, int &output)
{
	return StrToIntEx(text.c_str(), STIF_DEFAULT, &output) != FALSE;
}

template <BetterEnum T>
void TryLoadEnumSetting(const std::wstring &desktopIniPath, const wchar_t *setting, T &output)
{
	auto value = ReadSetting(desktopIniPath, setting);

	if (!value)
	{
		return;
	}

	int integralValue;
	if (!TryParseInteger(*value, integralValue) || !T::_is_valid(integralValue))
	{
		LogInvalidSetting(desktopIniPath, setting, *value);
		return;
	}

	output = T::_from_integral(integralValue);
}

void TryLoadSortDirection(const std::wstring &desktopIniPath, SortDirection &output)
{
	auto value = ReadSetting(desktopIniPath, SETTING_SORT_ASCENDING);

	if (!value)
	{
		return;
	}

	int sortAscending;
	if (!TryParseInteger(*value, sortAscending) || (sortAscending != 0 && sortAscending != 1))
	{
		LogInvalidSetting(desktopIniPath, SETTING_SORT_ASCENDING, *value);
		return;
	}

	output = sortAscending ? SortDirection::Ascending : SortDirection::Descending;
}

void AppendSetting(std::wstring &sectionData, const wchar_t *settingName, int value)
{
	sectionData += settingName;
	sectionData.push_back(L'=');
	sectionData += std::to_wstring(value);
	sectionData.push_back(L'\0');
}

}

std::optional<FolderSettings> LoadFolderSettings(const std::wstring &directory,
	const FolderSettings &fallbackSettings)
{
	auto desktopIniPath = BuildDesktopIniPath(directory);

	if (!HasExplorerPlusPlusSection(desktopIniPath))
	{
		return std::nullopt;
	}

	FolderSettings folderSettings = fallbackSettings;
	TryLoadEnumSetting(desktopIniPath, SETTING_VIEW_MODE, folderSettings.viewMode);
	TryLoadEnumSetting(desktopIniPath, SETTING_SORT_MODE, folderSettings.sortMode);
	TryLoadSortDirection(desktopIniPath, folderSettings.sortDirection);
	TryLoadEnumSetting(desktopIniPath, SETTING_GROUP_MODE, folderSettings.groupMode);
	return folderSettings;
}

bool SaveFolderSettings(const std::wstring &directory, const FolderSettings &folderSettings)
{
	auto desktopIniPath = BuildDesktopIniPath(directory);

	std::wstring sectionData;
	AppendSetting(sectionData, SETTING_VIEW_MODE, folderSettings.viewMode._to_integral());
	AppendSetting(sectionData, SETTING_SORT_MODE, folderSettings.sortMode._to_integral());
	AppendSetting(sectionData, SETTING_SORT_ASCENDING,
		folderSettings.sortDirection == +SortDirection::Ascending ? 1 : 0);
	AppendSetting(sectionData, SETTING_GROUP_MODE, folderSettings.groupMode._to_integral());
	sectionData.push_back(L'\0');

	if (!WritePrivateProfileSection(SECTION_NAME, sectionData.c_str(), desktopIniPath.c_str()))
	{
		auto error = GetLastError();
		LOG(WARNING) << std::format("Couldn't save folder settings to \"{}\" (error {}).",
			wstrToUtf8Str(desktopIniPath), error);
		return false;
	}

	WritePrivateProfileString(nullptr, nullptr, nullptr, desktopIniPath.c_str());

	auto fileAttributes = GetFileAttributes(desktopIniPath.c_str());
	if (fileAttributes == INVALID_FILE_ATTRIBUTES)
	{
		auto error = GetLastError();
		LOG(WARNING) << std::format("Couldn't read attributes for \"{}\" after saving folder "
			"settings (error {}).", wstrToUtf8Str(desktopIniPath), error);
		return false;
	}

	WI_SetFlag(fileAttributes, FILE_ATTRIBUTE_HIDDEN);
	WI_SetFlag(fileAttributes, FILE_ATTRIBUTE_SYSTEM);

	if (!SetFileAttributes(desktopIniPath.c_str(), fileAttributes))
	{
		auto error = GetLastError();
		LOG(WARNING) << std::format("Couldn't set hidden/system attributes on \"{}\" (error {}).",
			wstrToUtf8Str(desktopIniPath), error);
		return false;
	}

	return true;
}

}
