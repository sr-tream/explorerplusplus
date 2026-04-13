// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "ContextMenuAction.h"
#include "ApplicationHelper.h"
#include "../Helper/ShellHelper.h"
#include <boost/algorithm/string.hpp>
#include <shlwapi.h>
#include <unordered_set>

namespace ContextMenuActions
{

namespace
{

std::wstring QuotePath(const std::wstring &path)
{
	std::wstring quotedPath = L"\"";
	size_t numConsecutiveBackslashes = 0;

	for (wchar_t ch : path)
	{
		if (ch == L'\\')
		{
			numConsecutiveBackslashes++;
			continue;
		}

		if (ch == L'"')
		{
			quotedPath.append(numConsecutiveBackslashes * 2 + 1, L'\\');
			quotedPath += ch;
			numConsecutiveBackslashes = 0;
			continue;
		}

		quotedPath.append(numConsecutiveBackslashes, L'\\');
		numConsecutiveBackslashes = 0;
		quotedPath += ch;
	}

	quotedPath.append(numConsecutiveBackslashes * 2, L'\\');
	quotedPath += L'"';
	return quotedPath;
}

std::wstring BuildSelectedPathsParameter(const std::vector<std::wstring> &selectedItemPaths)
{
	std::wstring combinedParameters;

	for (const auto &path : selectedItemPaths)
	{
		if (!combinedParameters.empty())
		{
			combinedParameters += L" ";
		}

		combinedParameters += QuotePath(path);
	}

	return combinedParameters;
}

struct ExtensionFilter
{
	std::unordered_set<std::wstring> includedExtensions;
	std::unordered_set<std::wstring> excludedExtensions;
};

ExtensionFilter ParseFileExtensions(const std::wstring &fileExtensions)
{
	std::vector<std::wstring> rawExtensions;
	boost::split(rawExtensions, fileExtensions, boost::is_any_of(L";, \t\r\n"),
		boost::token_compress_on);

	ExtensionFilter extensionFilter;

	for (auto &rawExtension : rawExtensions)
	{
		boost::trim(rawExtension);

		if (rawExtension.empty())
		{
			continue;
		}

		auto extensionSet = &extensionFilter.includedExtensions;

		if (rawExtension[0] == L'!')
		{
			rawExtension.erase(rawExtension.begin());
			boost::trim(rawExtension);

			if (rawExtension.empty())
			{
				continue;
			}

			extensionSet = &extensionFilter.excludedExtensions;
		}

		if (rawExtension[0] != L'.')
		{
			rawExtension.insert(rawExtension.begin(), L'.');
		}

		boost::to_lower(rawExtension);
		extensionSet->insert(rawExtension);
	}

	return extensionFilter;
}

bool ContainsInvocationTokens(const std::wstring &command)
{
	return command.find(L"%directory%") != std::wstring::npos
		|| command.find(L"%path%") != std::wstring::npos
		|| command.find(L"%paths%") != std::wstring::npos;
}

bool DoesPathMatchExtensionFilter(const std::wstring &path,
	const std::unordered_set<std::wstring> &extensions)
{
	const wchar_t *fileName = PathFindFileName(path.c_str());
	std::wstring normalizedFileName = fileName;
	boost::to_lower(normalizedFileName);

	for (const auto &extension : extensions)
	{
		if (boost::ends_with(normalizedFileName, extension))
		{
			return true;
		}
	}

	return false;
}

bool DoSelectedItemsMatchExtensionFilter(const InvocationContext &context,
	const ExtensionFilter &extensionFilter)
{
	if (extensionFilter.includedExtensions.empty() && extensionFilter.excludedExtensions.empty())
	{
		return true;
	}

	if (context.numSelectedFiles == 0 || context.numSelectedFolders > 0)
	{
		return false;
	}

	for (const auto &path : context.selectedItemPaths)
	{
		if (DoesPathMatchExtensionFilter(path, extensionFilter.excludedExtensions))
		{
			return false;
		}

		if (!extensionFilter.includedExtensions.empty()
			&& !DoesPathMatchExtensionFilter(path, extensionFilter.includedExtensions))
		{
			return false;
		}
	}

	return true;
}

std::wstring BuildAutomaticParameters(const InvocationContext &context)
{
	if (context.menuType == MenuType::Background)
	{
		return QuotePath(context.directory);
	}

	return BuildSelectedPathsParameter(context.selectedItemPaths);
}

std::wstring ExpandCommandTokens(const std::wstring &command, const InvocationContext &context)
{
	std::wstring expandedCommand = command;
	boost::replace_all(expandedCommand, L"%directory%", QuotePath(context.directory));

	std::wstring firstSelectedPath =
		context.selectedItemPaths.empty() ? L"" : QuotePath(context.selectedItemPaths.front());
	boost::replace_all(expandedCommand, L"%path%", firstSelectedPath);
	boost::replace_all(expandedCommand, L"%paths%",
		BuildSelectedPathsParameter(context.selectedItemPaths));

	return expandedCommand;
}

std::wstring GetStartDirectory(const InvocationContext &context)
{
	return context.directory;
}

}

ContextMenuAction::ContextMenuAction(const std::wstring &name, const std::wstring &command,
	bool enabled, bool showInItemMenu, bool showInBackgroundMenu, bool showForFiles,
	bool showForFolders, bool allowMultipleSelection, const std::wstring &fileExtensions,
	const std::wstring &iconPath) :
	m_name(name),
	m_command(command),
	m_enabled(enabled),
	m_showInItemMenu(showInItemMenu),
	m_showInBackgroundMenu(showInBackgroundMenu),
	m_showForFiles(showForFiles),
	m_showForFolders(showForFolders),
	m_allowMultipleSelection(allowMultipleSelection),
	m_fileExtensions(fileExtensions),
	m_iconPath(iconPath)
{
}

std::wstring ContextMenuAction::GetName() const
{
	return m_name;
}

void ContextMenuAction::SetName(const std::wstring &name)
{
	SetValue(m_name, name);
}

std::wstring ContextMenuAction::GetCommand() const
{
	return m_command;
}

void ContextMenuAction::SetCommand(const std::wstring &command)
{
	SetValue(m_command, command);
}

bool ContextMenuAction::GetEnabled() const
{
	return m_enabled;
}

void ContextMenuAction::SetEnabled(bool enabled)
{
	SetValue(m_enabled, enabled);
}

bool ContextMenuAction::GetShowInItemMenu() const
{
	return m_showInItemMenu;
}

void ContextMenuAction::SetShowInItemMenu(bool showInItemMenu)
{
	SetValue(m_showInItemMenu, showInItemMenu);
}

bool ContextMenuAction::GetShowInBackgroundMenu() const
{
	return m_showInBackgroundMenu;
}

void ContextMenuAction::SetShowInBackgroundMenu(bool showInBackgroundMenu)
{
	SetValue(m_showInBackgroundMenu, showInBackgroundMenu);
}

bool ContextMenuAction::GetShowForFiles() const
{
	return m_showForFiles;
}

void ContextMenuAction::SetShowForFiles(bool showForFiles)
{
	SetValue(m_showForFiles, showForFiles);
}

bool ContextMenuAction::GetShowForFolders() const
{
	return m_showForFolders;
}

void ContextMenuAction::SetShowForFolders(bool showForFolders)
{
	SetValue(m_showForFolders, showForFolders);
}

bool ContextMenuAction::GetAllowMultipleSelection() const
{
	return m_allowMultipleSelection;
}

void ContextMenuAction::SetAllowMultipleSelection(bool allowMultipleSelection)
{
	SetValue(m_allowMultipleSelection, allowMultipleSelection);
}

std::wstring ContextMenuAction::GetFileExtensions() const
{
	return m_fileExtensions;
}

void ContextMenuAction::SetFileExtensions(const std::wstring &fileExtensions)
{
	SetValue(m_fileExtensions, fileExtensions);
}

std::wstring ContextMenuAction::GetIconPath() const
{
	return m_iconPath;
}

void ContextMenuAction::SetIconPath(const std::wstring &iconPath)
{
	SetValue(m_iconPath, iconPath);
}

boost::signals2::connection ContextMenuAction::AddUpdatedObserver(
	const UpdatedSignal::slot_type &observer)
{
	return m_updatedSignal.connect(observer);
}

template <typename T>
void ContextMenuAction::SetValue(T &currentValue, T newValue)
{
	if (currentValue == newValue)
	{
		return;
	}

	currentValue = std::move(newValue);
	m_updatedSignal(this);
}

bool IsApplicable(const ContextMenuAction &contextMenuAction, const InvocationContext &context)
{
	if (!contextMenuAction.GetEnabled())
	{
		return false;
	}

	if (context.menuType == MenuType::Background)
	{
		return contextMenuAction.GetShowInBackgroundMenu();
	}

	if (!contextMenuAction.GetShowInItemMenu())
	{
		return false;
	}

	int numSelectedItems = context.numSelectedFiles + context.numSelectedFolders;

	if (numSelectedItems == 0)
	{
		return false;
	}

	if (!contextMenuAction.GetAllowMultipleSelection() && numSelectedItems > 1)
	{
		return false;
	}

	if (context.numSelectedFiles > 0 && !contextMenuAction.GetShowForFiles())
	{
		return false;
	}

	if (context.numSelectedFolders > 0 && !contextMenuAction.GetShowForFolders())
	{
		return false;
	}

	auto fileExtensions = ParseFileExtensions(contextMenuAction.GetFileExtensions());
	return DoSelectedItemsMatchExtensionFilter(context, fileExtensions);
}

std::wstring BuildCommandLine(const ContextMenuAction &contextMenuAction,
	const InvocationContext &context)
{
	std::wstring expandedCommand = ExpandCommandTokens(contextMenuAction.GetCommand(), context);

	if (expandedCommand.empty() || ContainsInvocationTokens(contextMenuAction.GetCommand()))
	{
		return expandedCommand;
	}

	auto applicationInfo = Applications::ApplicationHelper::ParseCommandString(expandedCommand);

	if (!applicationInfo.parameters.empty())
	{
		return expandedCommand;
	}

	auto automaticParameters = BuildAutomaticParameters(context);

	if (automaticParameters.empty())
	{
		return expandedCommand;
	}

	boost::trim_right(expandedCommand);
	return expandedCommand + L" " + automaticParameters;
}

void ExecuteAction(HWND parentWindow, const ContextMenuAction &contextMenuAction,
	const InvocationContext &context)
{
	auto commandLine = BuildCommandLine(contextMenuAction, context);
	auto applicationInfo = Applications::ApplicationHelper::ParseCommandString(commandLine);

	ExecuteFileAction(parentWindow, applicationInfo.application, L"", applicationInfo.parameters,
		GetStartDirectory(context));
}

}
