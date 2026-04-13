// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "pch.h"
#include "ContextMenuActionStorageTestHelper.h"
#include "ContextMenuAction.h"
#include "ContextMenuActionModel.h"

using namespace ContextMenuActions;

namespace ContextMenuActions
{

bool operator==(const ContextMenuAction &first, const ContextMenuAction &second)
{
	return first.GetName() == second.GetName() && first.GetCommand() == second.GetCommand()
		&& first.GetEnabled() == second.GetEnabled()
		&& first.GetShowInItemMenu() == second.GetShowInItemMenu()
		&& first.GetShowInBackgroundMenu() == second.GetShowInBackgroundMenu()
		&& first.GetShowForFiles() == second.GetShowForFiles()
		&& first.GetShowForFolders() == second.GetShowForFolders()
		&& first.GetAllowMultipleSelection() == second.GetAllowMultipleSelection()
		&& first.GetFileExtensions() == second.GetFileExtensions()
		&& first.GetIconPath() == second.GetIconPath();
}

}

void BuildLoadSaveReferenceModel(ContextMenuActionModel *model)
{
	model->AddItem(std::make_unique<ContextMenuAction>(L"Open terminal here",
		L"cmd.exe /K cd /d %directory%", true, false, true, false, false, true));
	model->AddItem(std::make_unique<ContextMenuAction>(L"Open in VS Code",
		L"\"C:\\Program Files\\Microsoft VS Code\\Code.exe\" %path%", true, true, false, true,
		false, false, L".txt; md", L"C:\\Icons\\code.png"));
	model->AddItem(std::make_unique<ContextMenuAction>(L"Archive selection",
		L"C:\\Tools\\archive.exe %paths%", false, true, false, true, true, true));
}
