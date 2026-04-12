// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include "FolderSettings.h"
#include <optional>
#include <string>

namespace DesktopIniFolderSettingsStorage
{

std::optional<FolderSettings> LoadFolderSettings(const std::wstring &directory,
	const FolderSettings &fallbackSettings);
bool SaveFolderSettings(const std::wstring &directory, const FolderSettings &folderSettings);

}
