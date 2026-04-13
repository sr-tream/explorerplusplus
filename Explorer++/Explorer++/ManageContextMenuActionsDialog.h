// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include "BaseDialog.h"
#include <boost/signals2.hpp>
#include <memory>
#include <vector>

namespace ContextMenuActions
{

class ContextMenuAction;
class ContextMenuActionModel;

class ManageContextMenuActionsDialog : public BaseDialog
{
public:
	static ManageContextMenuActionsDialog *Create(const ResourceLoader *resourceLoader, HWND parent,
		ContextMenuActionModel *model);

protected:
	INT_PTR OnInitDialog() override;
	std::vector<ResizableDialogControl> GetResizableControls() override;
	INT_PTR OnNotify(NMHDR *nmhdr) override;
	INT_PTR OnCommand(WPARAM wParam, LPARAM lParam) override;
	INT_PTR OnClose() override;

private:
	enum class MovementDirection
	{
		Up,
		Down
	};

	ManageContextMenuActionsDialog(const ResourceLoader *resourceLoader, HWND parent,
		ContextMenuActionModel *model);
	~ManageContextMenuActionsDialog() = default;

	void InitializeListView();
	void RebuildActionList(const ContextMenuAction *actionToSelect = nullptr);
	void UpdateControlStates();
	ContextMenuAction *MaybeGetSelectedAction() const;
	void SelectAction(const ContextMenuAction *action);
	std::wstring BuildScopeText(const ContextMenuAction &action) const;

	void OnNew();
	void OnEdit();
	void OnMove(MovementDirection direction);
	void OnDelete();

	HWND GetListView() const;

	ContextMenuActionModel *m_model;
	std::vector<boost::signals2::scoped_connection> m_connections;
};

}
