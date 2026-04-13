// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include "BaseDialog.h"
#include "../Helper/PassKey.h"
#include <wil/resource.h>
#include <memory>
#include <optional>

namespace ContextMenuActions
{

class ContextMenuAction;
class ContextMenuActionModel;

class ContextMenuActionEditorDialog : public BaseDialog
{
public:
	class EditDetails
	{
	private:
		using PassKey = PassKey<EditDetails>;

	public:
		enum class Type
		{
			ExistingItem,
			NewItem
		};

		EditDetails(Type type, PassKey) : type(type)
		{
		}

		static std::unique_ptr<EditDetails> AddNewAction(std::unique_ptr<ContextMenuAction> action,
			std::optional<size_t> index = std::nullopt);
		static std::unique_ptr<EditDetails> EditAction(ContextMenuAction *action);

		const Type type;
		std::unique_ptr<ContextMenuAction> newAction;
		std::optional<size_t> index;
		ContextMenuAction *existingAction = nullptr;
	};

	static ContextMenuActionEditorDialog *Create(HWND parent,
		const ResourceLoader *resourceLoader, ContextMenuActionModel *model,
		std::unique_ptr<EditDetails> editDetails);

protected:
	INT_PTR OnInitDialog() override;
	INT_PTR OnCommand(WPARAM wParam, LPARAM lParam) override;
	INT_PTR OnClose() override;

private:
	ContextMenuActionEditorDialog(HWND parent, const ResourceLoader *resourceLoader,
		ContextMenuActionModel *model, std::unique_ptr<EditDetails> editDetails);
	~ContextMenuActionEditorDialog() = default;

	void OnChooseFile();
	void OnChooseIconFile();
	void OnOk();
	void OnCancel();
	void InitializeControlFonts();
	void UpdateControlStates();
	void ApplyEdits(const std::wstring &name, const std::wstring &command, bool enabled,
		bool showInItemMenu, bool showInBackgroundMenu, bool showForFiles, bool showForFolders,
		bool allowMultipleSelection, const std::wstring &fileExtensions,
		const std::wstring &iconPath);

	ContextMenuActionModel *m_model;
	std::unique_ptr<EditDetails> m_editDetails;
	wil::unique_hfont m_boldLabelFont;
};

}
