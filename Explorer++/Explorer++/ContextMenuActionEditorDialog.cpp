// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "ContextMenuActionEditorDialog.h"
#include "ContextMenuAction.h"
#include "ContextMenuActionModel.h"
#include "MainResource.h"
#include "ResourceLoader.h"
#include "../Helper/Controls.h"
#include "../Helper/WindowHelper.h"

namespace ContextMenuActions
{

std::unique_ptr<ContextMenuActionEditorDialog::EditDetails>
ContextMenuActionEditorDialog::EditDetails::AddNewAction(std::unique_ptr<ContextMenuAction> action,
	std::optional<size_t> index)
{
	auto editDetails = std::make_unique<EditDetails>(Type::NewItem, PassKey());
	editDetails->newAction = std::move(action);
	editDetails->index = index;
	return editDetails;
}

std::unique_ptr<ContextMenuActionEditorDialog::EditDetails>
ContextMenuActionEditorDialog::EditDetails::EditAction(ContextMenuAction *action)
{
	auto editDetails = std::make_unique<EditDetails>(Type::ExistingItem, PassKey());
	editDetails->existingAction = action;
	return editDetails;
}

ContextMenuActionEditorDialog *ContextMenuActionEditorDialog::Create(HWND parent,
	const ResourceLoader *resourceLoader, ContextMenuActionModel *model,
	std::unique_ptr<EditDetails> editDetails)
{
	return new ContextMenuActionEditorDialog(parent, resourceLoader, model, std::move(editDetails));
}

ContextMenuActionEditorDialog::ContextMenuActionEditorDialog(HWND parent,
	const ResourceLoader *resourceLoader, ContextMenuActionModel *model,
	std::unique_ptr<EditDetails> editDetails) :
	BaseDialog(resourceLoader, IDD_EDIT_CONTEXT_MENU_ACTION, parent, DialogSizingType::None),
	m_model(model),
	m_editDetails(std::move(editDetails))
{
}

INT_PTR ContextMenuActionEditorDialog::OnInitDialog()
{
	if (m_editDetails->type == EditDetails::Type::NewItem)
	{
		SetWindowText(m_hDlg, L"New custom action");
	}

	const ContextMenuAction *targetAction = m_editDetails->type == EditDetails::Type::NewItem
		? m_editDetails->newAction.get()
		: m_editDetails->existingAction;

	SetDlgItemText(m_hDlg, IDC_CONTEXT_MENU_ACTION_NAME, targetAction->GetName().c_str());
	SetDlgItemText(m_hDlg, IDC_CONTEXT_MENU_ACTION_COMMAND, targetAction->GetCommand().c_str());
	SetDlgItemText(m_hDlg, IDC_CONTEXT_MENU_ACTION_FILE_EXTENSIONS,
		targetAction->GetFileExtensions().c_str());
	SetDlgItemText(m_hDlg, IDC_CONTEXT_MENU_ACTION_ICON_PATH, targetAction->GetIconPath().c_str());

	CheckDlgButton(m_hDlg, IDC_CONTEXT_MENU_ACTION_ENABLED,
		targetAction->GetEnabled() ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(m_hDlg, IDC_CONTEXT_MENU_ACTION_SHOW_IN_ITEM_MENU,
		targetAction->GetShowInItemMenu() ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(m_hDlg, IDC_CONTEXT_MENU_ACTION_SHOW_IN_BACKGROUND_MENU,
		targetAction->GetShowInBackgroundMenu() ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(m_hDlg, IDC_CONTEXT_MENU_ACTION_SHOW_FOR_FILES,
		targetAction->GetShowForFiles() ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(m_hDlg, IDC_CONTEXT_MENU_ACTION_SHOW_FOR_FOLDERS,
		targetAction->GetShowForFolders() ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(m_hDlg, IDC_CONTEXT_MENU_ACTION_ALLOW_MULTIPLE_SELECTION,
		targetAction->GetAllowMultipleSelection() ? BST_CHECKED : BST_UNCHECKED);

	InitializeControlFonts();

	BOOL enableOk = (GetWindowTextLength(GetDlgItem(m_hDlg, IDC_CONTEXT_MENU_ACTION_NAME)) != 0
		&& GetWindowTextLength(GetDlgItem(m_hDlg, IDC_CONTEXT_MENU_ACTION_COMMAND)) != 0);
	EnableWindow(GetDlgItem(m_hDlg, IDOK), enableOk);

	UpdateControlStates();

	HWND nameEdit = GetDlgItem(m_hDlg, IDC_CONTEXT_MENU_ACTION_NAME);
	SendMessage(nameEdit, EM_SETSEL, 0, -1);
	SetFocus(nameEdit);

	return 0;
}

INT_PTR ContextMenuActionEditorDialog::OnCommand(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);

	if (HIWORD(wParam) != 0)
	{
		switch (HIWORD(wParam))
		{
		case EN_CHANGE:
			if (LOWORD(wParam) != IDC_CONTEXT_MENU_ACTION_NAME
				&& LOWORD(wParam) != IDC_CONTEXT_MENU_ACTION_COMMAND)
			{
				break;
			}

			EnableWindow(GetDlgItem(m_hDlg, IDOK),
				GetWindowTextLength(GetDlgItem(m_hDlg, IDC_CONTEXT_MENU_ACTION_NAME)) != 0
					&& GetWindowTextLength(GetDlgItem(m_hDlg, IDC_CONTEXT_MENU_ACTION_COMMAND)) != 0);
			break;
		}
	}
	else
	{
		switch (LOWORD(wParam))
		{
		case IDC_CONTEXT_MENU_ACTION_CHOOSE_FILE:
			OnChooseFile();
			break;

		case IDC_CONTEXT_MENU_ACTION_CHOOSE_ICON_FILE:
			OnChooseIconFile();
			break;

		case IDC_CONTEXT_MENU_ACTION_SHOW_IN_ITEM_MENU:
		case IDC_CONTEXT_MENU_ACTION_SHOW_FOR_FILES:
			UpdateControlStates();
			break;

		case IDOK:
			OnOk();
			break;

		case IDCANCEL:
			OnCancel();
			break;
		}
	}

	return 0;
}

void ContextMenuActionEditorDialog::OnChooseFile()
{
	const TCHAR *filter = _T("Programs (*.exe)\0*.exe\0All Files\0*.*\0\0");
	TCHAR fullFileName[MAX_PATH] = L"";

	OPENFILENAME ofn;
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = m_hDlg;
	ofn.lpstrFilter = filter;
	ofn.lpstrCustomFilter = nullptr;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = fullFileName;
	ofn.nMaxFile = std::size(fullFileName);
	ofn.lpstrFileTitle = nullptr;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = nullptr;
	ofn.lpstrTitle = nullptr;
	ofn.Flags = OFN_ENABLESIZING | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
	ofn.lpstrDefExt = _T("exe");
	ofn.lCustData = NULL;
	ofn.lpfnHook = nullptr;
	ofn.pvReserved = nullptr;
	ofn.dwReserved = 0;
	ofn.FlagsEx = 0;

	BOOL result = GetOpenFileName(&ofn);

	if (!result)
	{
		return;
	}

	std::wstring finalFileName = fullFileName;

	if (finalFileName.find(' ') != std::wstring::npos)
	{
		finalFileName = L"\"" + finalFileName + L"\"";
	}

	SetDlgItemText(m_hDlg, IDC_CONTEXT_MENU_ACTION_COMMAND, finalFileName.c_str());
}

void ContextMenuActionEditorDialog::OnChooseIconFile()
{
	const TCHAR *filter =
		_T("Image files (*.png;*.ico;*.bmp;*.jpg;*.jpeg;*.gif)\0*.png;*.ico;*.bmp;*.jpg;*.jpeg;*.gif\0All Files\0*.*\0\0");
	TCHAR fullFileName[MAX_PATH] = L"";

	OPENFILENAME ofn;
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = m_hDlg;
	ofn.lpstrFilter = filter;
	ofn.lpstrCustomFilter = nullptr;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = fullFileName;
	ofn.nMaxFile = std::size(fullFileName);
	ofn.lpstrFileTitle = nullptr;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = nullptr;
	ofn.lpstrTitle = nullptr;
	ofn.Flags = OFN_ENABLESIZING | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
	ofn.lpstrDefExt = _T("png");
	ofn.lCustData = NULL;
	ofn.lpfnHook = nullptr;
	ofn.pvReserved = nullptr;
	ofn.dwReserved = 0;
	ofn.FlagsEx = 0;

	BOOL result = GetOpenFileName(&ofn);

	if (!result)
	{
		return;
	}

	SetDlgItemText(m_hDlg, IDC_CONTEXT_MENU_ACTION_ICON_PATH, fullFileName);
}

void ContextMenuActionEditorDialog::OnOk()
{
	std::wstring name = GetDlgItemString(m_hDlg, IDC_CONTEXT_MENU_ACTION_NAME);
	std::wstring command = GetDlgItemString(m_hDlg, IDC_CONTEXT_MENU_ACTION_COMMAND);

	if (name.empty() || command.empty())
	{
		EndDialog(m_hDlg, 0);
		return;
	}

	ApplyEdits(name, command,
		IsDlgButtonChecked(m_hDlg, IDC_CONTEXT_MENU_ACTION_ENABLED) == BST_CHECKED,
		IsDlgButtonChecked(m_hDlg, IDC_CONTEXT_MENU_ACTION_SHOW_IN_ITEM_MENU) == BST_CHECKED,
		IsDlgButtonChecked(m_hDlg, IDC_CONTEXT_MENU_ACTION_SHOW_IN_BACKGROUND_MENU) == BST_CHECKED,
		IsDlgButtonChecked(m_hDlg, IDC_CONTEXT_MENU_ACTION_SHOW_FOR_FILES) == BST_CHECKED,
		IsDlgButtonChecked(m_hDlg, IDC_CONTEXT_MENU_ACTION_SHOW_FOR_FOLDERS) == BST_CHECKED,
		IsDlgButtonChecked(m_hDlg, IDC_CONTEXT_MENU_ACTION_ALLOW_MULTIPLE_SELECTION) == BST_CHECKED,
		GetDlgItemString(m_hDlg, IDC_CONTEXT_MENU_ACTION_FILE_EXTENSIONS),
		GetDlgItemString(m_hDlg, IDC_CONTEXT_MENU_ACTION_ICON_PATH));

	EndDialog(m_hDlg, 1);
}

void ContextMenuActionEditorDialog::OnCancel()
{
	EndDialog(m_hDlg, 0);
}

void ContextMenuActionEditorDialog::InitializeControlFonts()
{
	auto currentFont = reinterpret_cast<HFONT>(SendMessage(m_hDlg, WM_GETFONT, 0, 0));

	if (!currentFont)
	{
		return;
	}

	LOGFONT logFont = {};

	if (GetObject(currentFont, sizeof(logFont), &logFont) == 0)
	{
		return;
	}

	logFont.lfWeight = FW_BOLD;
	m_boldLabelFont.reset(CreateFontIndirect(&logFont));

	if (!m_boldLabelFont)
	{
		return;
	}

	for (int controlId : { IDC_CONTEXT_MENU_ACTION_NAME_LABEL, IDC_CONTEXT_MENU_ACTION_COMMAND_LABEL,
			 IDC_CONTEXT_MENU_ACTION_ICON_PATH_LABEL, IDC_CONTEXT_MENU_ACTION_FILE_EXTENSIONS_LABEL })
	{
		SendDlgItemMessage(m_hDlg, controlId, WM_SETFONT,
			reinterpret_cast<WPARAM>(m_boldLabelFont.get()), TRUE);
	}
}

void ContextMenuActionEditorDialog::UpdateControlStates()
{
	BOOL enableItemMenuControls =
		(IsDlgButtonChecked(m_hDlg, IDC_CONTEXT_MENU_ACTION_SHOW_IN_ITEM_MENU) == BST_CHECKED);
	BOOL enableFileExtensionEdit = enableItemMenuControls
		&& (IsDlgButtonChecked(m_hDlg, IDC_CONTEXT_MENU_ACTION_SHOW_FOR_FILES) == BST_CHECKED);

	EnableWindow(GetDlgItem(m_hDlg, IDC_CONTEXT_MENU_ACTION_SHOW_FOR_FILES),
		enableItemMenuControls);
	EnableWindow(GetDlgItem(m_hDlg, IDC_CONTEXT_MENU_ACTION_SHOW_FOR_FOLDERS),
		enableItemMenuControls);
	EnableWindow(GetDlgItem(m_hDlg, IDC_CONTEXT_MENU_ACTION_ALLOW_MULTIPLE_SELECTION),
		enableItemMenuControls);
	EnableWindow(GetDlgItem(m_hDlg, IDC_CONTEXT_MENU_ACTION_FILE_EXTENSIONS_LABEL),
		enableFileExtensionEdit);
	EnableWindow(GetDlgItem(m_hDlg, IDC_CONTEXT_MENU_ACTION_FILE_EXTENSIONS),
		enableFileExtensionEdit);
}

void ContextMenuActionEditorDialog::ApplyEdits(const std::wstring &name,
	const std::wstring &command, bool enabled, bool showInItemMenu, bool showInBackgroundMenu,
	bool showForFiles, bool showForFolders, bool allowMultipleSelection,
	const std::wstring &fileExtensions, const std::wstring &iconPath)
{
	ContextMenuAction *targetAction = m_editDetails->type == EditDetails::Type::NewItem
		? m_editDetails->newAction.get()
		: m_editDetails->existingAction;

	targetAction->SetName(name);
	targetAction->SetCommand(command);
	targetAction->SetEnabled(enabled);
	targetAction->SetShowInItemMenu(showInItemMenu);
	targetAction->SetShowInBackgroundMenu(showInBackgroundMenu);
	targetAction->SetShowForFiles(showForFiles);
	targetAction->SetShowForFolders(showForFolders);
	targetAction->SetAllowMultipleSelection(allowMultipleSelection);
	targetAction->SetFileExtensions(fileExtensions);
	targetAction->SetIconPath(iconPath);

	if (m_editDetails->type == EditDetails::Type::NewItem)
	{
		size_t index = m_editDetails->index.value_or(m_model->GetItems().size());
		m_model->AddItem(std::move(m_editDetails->newAction), index);
	}
}

INT_PTR ContextMenuActionEditorDialog::OnClose()
{
	EndDialog(m_hDlg, 0);
	return 0;
}

}
