// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include <boost/signals2.hpp>
#include <string>
#include <vector>

namespace ContextMenuActions
{

enum class MenuType
{
	Item,
	Background
};

struct InvocationContext
{
	MenuType menuType = MenuType::Item;
	std::wstring directory;
	std::vector<std::wstring> selectedItemPaths;
	int numSelectedFiles = 0;
	int numSelectedFolders = 0;
};

class ContextMenuAction
{
public:
	using UpdatedSignal = boost::signals2::signal<void(ContextMenuAction *contextMenuAction)>;

	ContextMenuAction(const std::wstring &name, const std::wstring &command, bool enabled = true,
		bool showInItemMenu = true, bool showInBackgroundMenu = false, bool showForFiles = true,
		bool showForFolders = true, bool allowMultipleSelection = true,
		const std::wstring &fileExtensions = L"", const std::wstring &iconPath = L"");

	std::wstring GetName() const;
	void SetName(const std::wstring &name);

	std::wstring GetCommand() const;
	void SetCommand(const std::wstring &command);

	bool GetEnabled() const;
	void SetEnabled(bool enabled);

	bool GetShowInItemMenu() const;
	void SetShowInItemMenu(bool showInItemMenu);

	bool GetShowInBackgroundMenu() const;
	void SetShowInBackgroundMenu(bool showInBackgroundMenu);

	bool GetShowForFiles() const;
	void SetShowForFiles(bool showForFiles);

	bool GetShowForFolders() const;
	void SetShowForFolders(bool showForFolders);

	bool GetAllowMultipleSelection() const;
	void SetAllowMultipleSelection(bool allowMultipleSelection);

	std::wstring GetFileExtensions() const;
	void SetFileExtensions(const std::wstring &fileExtensions);

	std::wstring GetIconPath() const;
	void SetIconPath(const std::wstring &iconPath);

	boost::signals2::connection AddUpdatedObserver(const UpdatedSignal::slot_type &observer);

private:
	template <typename T>
	void SetValue(T &currentValue, T newValue);

	std::wstring m_name;
	std::wstring m_command;
	bool m_enabled;
	bool m_showInItemMenu;
	bool m_showInBackgroundMenu;
	bool m_showForFiles;
	bool m_showForFolders;
	bool m_allowMultipleSelection;
	std::wstring m_fileExtensions;
	std::wstring m_iconPath;

	UpdatedSignal m_updatedSignal;
};

bool IsApplicable(const ContextMenuAction &contextMenuAction, const InvocationContext &context);
std::wstring BuildCommandLine(const ContextMenuAction &contextMenuAction,
	const InvocationContext &context);
void ExecuteAction(HWND parentWindow, const ContextMenuAction &contextMenuAction,
	const InvocationContext &context);

}
