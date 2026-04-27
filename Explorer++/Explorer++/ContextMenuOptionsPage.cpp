// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "App.h"
#include "ContextMenuOptionsPage.h"
#include "Config.h"
#include "MainResource.h"
#include "ManageContextMenuActionsDialog.h"
#include "../Helper/ResizableDialogHelper.h"

ContextMenuOptionsPage::ContextMenuOptionsPage(HWND parent,
	const ResourceLoader *resourceLoader, Config *config,
	SettingChangedCallback settingChangedCallback, HWND tooltipWindow, App *app) :
	OptionsPage(IDD_OPTIONS_CONTEXT_MENU, IDS_OPTIONS_CONTEXT_MENU_TITLE, parent, resourceLoader,
		config, settingChangedCallback, tooltipWindow),
	m_app(app)
{
}

std::unique_ptr<ResizableDialogHelper> ContextMenuOptionsPage::InitializeResizeDialogHelper()
{
	std::vector<ResizableDialogControl> controls;
	controls.emplace_back(GetDlgItem(GetDialog(), IDC_STATIC_INPUT_GROUP_MOUSE),
		MovingType::None, SizingType::Horizontal);
	controls.emplace_back(GetDlgItem(GetDialog(), IDC_SETTINGS_CHECK_QUICK_CONTEXT_MENUS),
		MovingType::None, SizingType::Horizontal);
	controls.emplace_back(
		GetDlgItem(GetDialog(), IDC_SETTINGS_CHECK_QUICK_CONTEXT_MENU_CUSTOM_ACTIONS),
		MovingType::None, SizingType::Horizontal);
	controls.emplace_back(
		GetDlgItem(GetDialog(), IDC_SETTINGS_CHECK_QUICK_CONTEXT_MENU_OPEN_ITEM_ICON),
		MovingType::None, SizingType::Horizontal);
	controls.emplace_back(GetDlgItem(GetDialog(), IDC_BUTTON_MANAGE_CONTEXT_MENU_ACTIONS),
		MovingType::None, SizingType::Horizontal);
	controls.emplace_back(GetDlgItem(GetDialog(), IDC_STATIC_INPUT_GROUP_TOUCHPAD),
		MovingType::None, SizingType::Horizontal);
	return std::make_unique<ResizableDialogHelper>(GetDialog(), controls);
}

void ContextMenuOptionsPage::InitializeControls()
{
	if (m_config->showQuickContextMenus)
	{
		CheckDlgButton(GetDialog(), IDC_SETTINGS_CHECK_QUICK_CONTEXT_MENUS, BST_CHECKED);
	}

	if (m_config->showQuickContextMenuCustomActions)
	{
		CheckDlgButton(GetDialog(), IDC_SETTINGS_CHECK_QUICK_CONTEXT_MENU_CUSTOM_ACTIONS,
			BST_CHECKED);
	}

	if (m_config->showQuickContextMenuOpenItemIcon)
	{
		CheckDlgButton(GetDialog(), IDC_SETTINGS_CHECK_QUICK_CONTEXT_MENU_OPEN_ITEM_ICON,
			BST_CHECKED);
	}

	SetDlgItemInt(GetDialog(), IDC_SETTINGS_EDIT_TOUCHPAD_SENSITIVITY,
		m_config->touchpadScrollSensitivity, FALSE);

	SetQuickContextMenuControlStates();
}

void ContextMenuOptionsPage::OnCommand(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);

	switch (LOWORD(wParam))
	{
	case IDC_SETTINGS_CHECK_QUICK_CONTEXT_MENUS:
		SetQuickContextMenuControlStates();
		m_settingChangedCallback();
		break;

	case IDC_SETTINGS_CHECK_QUICK_CONTEXT_MENU_CUSTOM_ACTIONS:
	case IDC_SETTINGS_CHECK_QUICK_CONTEXT_MENU_OPEN_ITEM_ICON:
		m_settingChangedCallback();
		break;

	case IDC_SETTINGS_EDIT_TOUCHPAD_SENSITIVITY:
		if (HIWORD(wParam) == EN_CHANGE)
		{
			m_settingChangedCallback();
		}
		break;

	case IDC_BUTTON_MANAGE_CONTEXT_MENU_ACTIONS:
	{
		auto *dialog = ContextMenuActions::ManageContextMenuActionsDialog::Create(m_resourceLoader,
			GetDialog(), m_app->GetContextMenuActionModel());
		dialog->ShowModalDialog();
	}
		break;
	}
}

void ContextMenuOptionsPage::SetQuickContextMenuControlStates()
{
	HWND quickContextMenuCustomActionsCheckBox =
		GetDlgItem(GetDialog(), IDC_SETTINGS_CHECK_QUICK_CONTEXT_MENU_CUSTOM_ACTIONS);
	HWND quickContextMenuOpenItemIconCheckBox =
		GetDlgItem(GetDialog(), IDC_SETTINGS_CHECK_QUICK_CONTEXT_MENU_OPEN_ITEM_ICON);

	BOOL enable =
		(IsDlgButtonChecked(GetDialog(), IDC_SETTINGS_CHECK_QUICK_CONTEXT_MENUS) == BST_CHECKED);

	EnableWindow(quickContextMenuCustomActionsCheckBox, enable);
	EnableWindow(quickContextMenuOpenItemIconCheckBox, enable);
}

void ContextMenuOptionsPage::SaveSettings()
{
	m_config->showQuickContextMenus =
		(IsDlgButtonChecked(GetDialog(), IDC_SETTINGS_CHECK_QUICK_CONTEXT_MENUS) == BST_CHECKED);
	m_config->showQuickContextMenuCustomActions =
		(IsDlgButtonChecked(GetDialog(), IDC_SETTINGS_CHECK_QUICK_CONTEXT_MENU_CUSTOM_ACTIONS)
			== BST_CHECKED);
	m_config->showQuickContextMenuOpenItemIcon =
		(IsDlgButtonChecked(GetDialog(), IDC_SETTINGS_CHECK_QUICK_CONTEXT_MENU_OPEN_ITEM_ICON)
			== BST_CHECKED);

	BOOL ok = FALSE;
	UINT sensitivity =
		GetDlgItemInt(GetDialog(), IDC_SETTINGS_EDIT_TOUCHPAD_SENSITIVITY, &ok, FALSE);
	if (ok && sensitivity >= 1 && sensitivity <= 1000)
	{
		m_config->touchpadScrollSensitivity = static_cast<int>(sensitivity);
	}
}
