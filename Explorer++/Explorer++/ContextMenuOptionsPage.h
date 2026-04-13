// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include "OptionsPage.h"

class App;

class ContextMenuOptionsPage : public OptionsPage
{
public:
	ContextMenuOptionsPage(HWND parent, const ResourceLoader *resourceLoader, Config *config,
		SettingChangedCallback settingChangedCallback, HWND tooltipWindow, App *app);

	void SaveSettings() override;

private:
	std::unique_ptr<ResizableDialogHelper> InitializeResizeDialogHelper() override;
	void InitializeControls() override;

	void OnCommand(WPARAM wParam, LPARAM lParam) override;

	void SetQuickContextMenuControlStates();

	App *const m_app;
};
