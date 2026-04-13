// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "ManageContextMenuActionsDialog.h"
#include "App.h"
#include "ContextMenuAction.h"
#include "ContextMenuActionEditorDialog.h"
#include "ContextMenuActionModel.h"
#include "MainResource.h"
#include "../Helper/Controls.h"

namespace ContextMenuActions
{

ManageContextMenuActionsDialog *ManageContextMenuActionsDialog::Create(
	const ResourceLoader *resourceLoader, HWND parent, ContextMenuActionModel *model)
{
	return new ManageContextMenuActionsDialog(resourceLoader, parent, model);
}

ManageContextMenuActionsDialog::ManageContextMenuActionsDialog(
	const ResourceLoader *resourceLoader, HWND parent, ContextMenuActionModel *model) :
	BaseDialog(resourceLoader, IDD_CONTEXT_MENU_ACTIONS, parent, DialogSizingType::Both),
	m_model(model)
{
}

INT_PTR ManageContextMenuActionsDialog::OnInitDialog()
{
	InitializeListView();

	m_connections.push_back(m_model->AddItemAddedObserver(
		[this](ContextMenuAction *action, size_t) { RebuildActionList(action); }));
	m_connections.push_back(m_model->AddItemUpdatedObserver(
		[this](ContextMenuAction *action) { RebuildActionList(action); }));
	m_connections.push_back(m_model->AddItemMovedObserver(
		[this](ContextMenuAction *action, size_t, size_t) { RebuildActionList(action); }));
	m_connections.push_back(m_model->AddItemRemovedObserver(
		[this](const ContextMenuAction *, size_t) { RebuildActionList(); }));
	m_connections.push_back(m_model->AddAllItemsRemovedObserver(
		[this]() { RebuildActionList(); }));

	RebuildActionList();
	UpdateControlStates();

	SetFocus(GetListView());
	return 0;
}

std::vector<ResizableDialogControl> ManageContextMenuActionsDialog::GetResizableControls()
{
	std::vector<ResizableDialogControl> controls;
	controls.emplace_back(GetListView(), MovingType::None, SizingType::Both);
	controls.emplace_back(GetDlgItem(m_hDlg, IDC_BUTTON_NEW), MovingType::Horizontal,
		SizingType::None);
	controls.emplace_back(GetDlgItem(m_hDlg, IDC_BUTTON_EDIT), MovingType::Horizontal,
		SizingType::None);
	controls.emplace_back(GetDlgItem(m_hDlg, IDC_BUTTON_MOVE_UP), MovingType::Horizontal,
		SizingType::None);
	controls.emplace_back(GetDlgItem(m_hDlg, IDC_BUTTON_MOVE_DOWN), MovingType::Horizontal,
		SizingType::None);
	controls.emplace_back(GetDlgItem(m_hDlg, IDC_BUTTON_DELETE), MovingType::Horizontal,
		SizingType::None);
	controls.emplace_back(GetDlgItem(m_hDlg, IDOK), MovingType::Both, SizingType::None);
	return controls;
}

INT_PTR ManageContextMenuActionsDialog::OnNotify(NMHDR *nmhdr)
{
	if (nmhdr->idFrom != IDC_LISTVIEW_CONTEXT_MENU_ACTIONS)
	{
		return 0;
	}

	switch (nmhdr->code)
	{
	case LVN_ITEMCHANGED:
		UpdateControlStates();
		break;

	case NM_DBLCLK:
		OnEdit();
		break;
	}

	return 0;
}

INT_PTR ManageContextMenuActionsDialog::OnCommand(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);

	switch (LOWORD(wParam))
	{
	case IDC_BUTTON_NEW:
		OnNew();
		break;

	case IDC_BUTTON_EDIT:
		OnEdit();
		break;

	case IDC_BUTTON_MOVE_UP:
		OnMove(MovementDirection::Up);
		break;

	case IDC_BUTTON_MOVE_DOWN:
		OnMove(MovementDirection::Down);
		break;

	case IDC_BUTTON_DELETE:
		OnDelete();
		break;

	case IDOK:
	case IDCANCEL:
		EndDialog(m_hDlg, 0);
		break;
	}

	return 0;
}

INT_PTR ManageContextMenuActionsDialog::OnClose()
{
	EndDialog(m_hDlg, 0);
	return 0;
}

void ManageContextMenuActionsDialog::InitializeListView()
{
	HWND listView = GetListView();
	ListView_SetExtendedListViewStyle(listView, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

	LVCOLUMN column = {};
	column.mask = LVCF_TEXT | LVCF_WIDTH;

	column.pszText = const_cast<LPWSTR>(L"Name");
	column.cx = 110;
	ListView_InsertColumn(listView, 0, &column);

	column.pszText = const_cast<LPWSTR>(L"Scope");
	column.cx = 90;
	ListView_InsertColumn(listView, 1, &column);

	column.pszText = const_cast<LPWSTR>(L"Command");
	column.cx = 180;
	ListView_InsertColumn(listView, 2, &column);
}

void ManageContextMenuActionsDialog::RebuildActionList(const ContextMenuAction *actionToSelect)
{
	HWND listView = GetListView();
	ListView_DeleteAllItems(listView);

	int index = 0;

	for (const auto &action : m_model->GetItems())
	{
		auto name = action->GetName();
		auto scopeText = BuildScopeText(*action);
		auto command = action->GetCommand();

		LVITEM item = {};
		item.mask = LVIF_PARAM;
		item.iItem = index;
		item.lParam = reinterpret_cast<LPARAM>(action.get());
		int insertedIndex = ListView_InsertItem(listView, &item);

		ListView_SetItemText(listView, insertedIndex, 0, name.data());
		ListView_SetItemText(listView, insertedIndex, 1, scopeText.data());
		ListView_SetItemText(listView, insertedIndex, 2, command.data());

		index++;
	}

	SelectAction(actionToSelect);
	UpdateControlStates();
}

void ManageContextMenuActionsDialog::UpdateControlStates()
{
	auto *selectedAction = MaybeGetSelectedAction();
	bool enableItemButtons = (selectedAction != nullptr);

	EnableWindow(GetDlgItem(m_hDlg, IDC_BUTTON_EDIT), enableItemButtons);
	EnableWindow(GetDlgItem(m_hDlg, IDC_BUTTON_MOVE_UP), enableItemButtons);
	EnableWindow(GetDlgItem(m_hDlg, IDC_BUTTON_MOVE_DOWN), enableItemButtons);
	EnableWindow(GetDlgItem(m_hDlg, IDC_BUTTON_DELETE), enableItemButtons);
}

ContextMenuAction *ManageContextMenuActionsDialog::MaybeGetSelectedAction() const
{
	int selectedIndex = ListView_GetNextItem(GetListView(), -1, LVNI_SELECTED);

	if (selectedIndex == -1)
	{
		return nullptr;
	}

	LVITEM item = {};
	item.mask = LVIF_PARAM;
	item.iItem = selectedIndex;
	ListView_GetItem(GetListView(), &item);
	return reinterpret_cast<ContextMenuAction *>(item.lParam);
}

void ManageContextMenuActionsDialog::SelectAction(const ContextMenuAction *action)
{
	if (!action)
	{
		return;
	}

	for (int i = 0; i < ListView_GetItemCount(GetListView()); i++)
	{
		LVITEM item = {};
		item.mask = LVIF_PARAM;
		item.iItem = i;
		ListView_GetItem(GetListView(), &item);

		if (reinterpret_cast<const ContextMenuAction *>(item.lParam) != action)
		{
			continue;
		}

		ListView_SetItemState(GetListView(), i, LVIS_SELECTED | LVIS_FOCUSED,
			LVIS_SELECTED | LVIS_FOCUSED);
		ListView_EnsureVisible(GetListView(), i, FALSE);
		break;
	}
}

std::wstring ManageContextMenuActionsDialog::BuildScopeText(const ContextMenuAction &action) const
{
	std::vector<std::wstring> scopes;

	if (action.GetShowInItemMenu())
	{
		scopes.push_back(L"Items");
	}

	if (action.GetShowInBackgroundMenu())
	{
		scopes.push_back(L"Background");
	}

	if (scopes.empty())
	{
		scopes.push_back(L"Hidden");
	}

	std::wstring scopeText;

	for (const auto &scope : scopes)
	{
		if (!scopeText.empty())
		{
			scopeText += L", ";
		}

		scopeText += scope;
	}

	if (!action.GetEnabled())
	{
		scopeText += L" (Disabled)";
	}

	return scopeText;
}

void ManageContextMenuActionsDialog::OnNew()
{
	auto *editorDialog = ContextMenuActionEditorDialog::Create(m_hDlg, m_resourceLoader, m_model,
		ContextMenuActionEditorDialog::EditDetails::AddNewAction(
			std::make_unique<ContextMenuAction>(L"", L"", true, true, false, true, true, true)));
	editorDialog->ShowModalDialog();
}

void ManageContextMenuActionsDialog::OnEdit()
{
	auto *selectedAction = MaybeGetSelectedAction();

	if (!selectedAction)
	{
		return;
	}

	auto *editorDialog = ContextMenuActionEditorDialog::Create(m_hDlg, m_resourceLoader, m_model,
		ContextMenuActionEditorDialog::EditDetails::EditAction(selectedAction));
	editorDialog->ShowModalDialog();
}

void ManageContextMenuActionsDialog::OnMove(MovementDirection direction)
{
	auto *selectedAction = MaybeGetSelectedAction();

	if (!selectedAction)
	{
		return;
	}

	auto index = m_model->GetItemIndex(selectedAction);
	size_t newIndex = index;

	if (direction == MovementDirection::Up)
	{
		if (index == 0)
		{
			return;
		}

		newIndex--;
	}
	else
	{
		if (index + 1 >= m_model->GetItems().size())
		{
			return;
		}

		newIndex++;
	}

	m_model->MoveItem(selectedAction, newIndex);
}

void ManageContextMenuActionsDialog::OnDelete()
{
	auto *selectedAction = MaybeGetSelectedAction();

	if (!selectedAction)
	{
		return;
	}

	int result = MessageBox(m_hDlg, L"Delete the selected custom action?", App::APP_NAME,
		MB_YESNO | MB_ICONINFORMATION | MB_DEFBUTTON2);

	if (result != IDYES)
	{
		return;
	}

	m_model->RemoveItem(selectedAction);
}

HWND ManageContextMenuActionsDialog::GetListView() const
{
	return GetDlgItem(m_hDlg, IDC_LISTVIEW_CONTEXT_MENU_ACTIONS);
}

}
