// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "BrowserWindow.h"
#include "ShellBrowser/ShellBrowserImpl.h"
#include "ShellBrowser/ShellNavigationController.h"
#include "ShellBrowser/ShellBrowser.h"
#include "Tab.h"
#include "TabContainer.h"
#include "../Helper/ShellHelper.h"

namespace
{

std::optional<std::wstring> MaybeResolveParsingPath(PCIDLIST_ABSOLUTE pidl)
{
	std::wstring resolvedPath;
	HRESULT hr = GetDisplayName(pidl, SHGDN_FORPARSING, resolvedPath);

	if (FAILED(hr) || resolvedPath.empty())
	{
		return std::nullopt;
	}

	return resolvedPath;
}

Tab *FindTabByResolvedPath(TabContainer *tabContainer, const std::wstring &resolvedPath)
{
	if (!tabContainer)
	{
		return nullptr;
	}

	for (const auto *tab : tabContainer->GetAllTabsInOrder())
	{
		auto *shellBrowserImpl = tab->GetShellBrowserImpl();

		if (!shellBrowserImpl)
		{
			continue;
		}

		if (StrCmpIW(shellBrowserImpl->GetDirectoryPath().c_str(), resolvedPath.c_str()) == 0)
		{
			return const_cast<Tab *>(tab);
		}
	}

	return nullptr;
}

bool SetCurrentEntrySelection(Tab *tab, PCIDLIST_ABSOLUTE pidlItem)
{
	if (!tab)
	{
		return false;
	}

	auto *navigationController = tab->GetShellBrowser()->GetNavigationController();

	if (!navigationController)
	{
		return false;
	}

	auto *currentEntry = navigationController->GetCurrentEntry();

	if (!currentEntry)
	{
		return false;
	}

	currentEntry->SetSelectedItems({ pidlItem });
	return true;
}

}

BrowserWindow::BrowserWindow() : m_id(idCounter++), m_commandTargetManager(this)
{
}

int BrowserWindow::GetId() const
{
	return m_id;
}

BrowserWindow::LifecycleState BrowserWindow::GetLifecycleState() const
{
	return m_lifecycleState;
}

void BrowserWindow::SetLifecycleState(LifecycleState state)
{
	if (state == LifecycleState::Main)
	{
		CHECK(m_lifecycleState == LifecycleState::Starting);
	}
	else if (state == LifecycleState::WillClose)
	{
		CHECK(m_lifecycleState == LifecycleState::Main);
	}
	else if (state == LifecycleState::Closing)
	{
		CHECK(m_lifecycleState == LifecycleState::WillClose);
	}
	else
	{
		CHECK(false);
	}

	m_lifecycleState = state;

	m_lifecycleStateChangedSignal(state);
}

boost::signals2::connection BrowserWindow::AddLifecycleStateChangedObserver(
	const LifecycleStateChangedSignal::slot_type &observer)
{
	return m_lifecycleStateChangedSignal.connect(observer);
}

BrowserCommandTargetManager *BrowserWindow::GetCommandTargetManager()
{
	return &m_commandTargetManager;
}

bool BrowserWindow::IsShellBrowserActive(const ShellBrowser *shellBrowser) const
{
	return shellBrowser == GetActiveShellBrowser();
}

void BrowserWindow::OpenDefaultItem()
{
	OpenDefaultItem(OpenFolderDisposition::CurrentTab);
}

void BrowserWindow::OpenItem(const std::wstring &itemPath)
{
	OpenItem(itemPath, OpenFolderDisposition::CurrentTab);
}

void BrowserWindow::OpenItem(PCIDLIST_ABSOLUTE pidlItem)
{
	OpenItem(pidlItem, OpenFolderDisposition::CurrentTab);
}

bool BrowserWindow::CanOpenItemInNewTab(PCIDLIST_ABSOLUTE pidlItem) const
{
	return ::CanOpenItemAsFolder(pidlItem, ShouldOpenContainerFiles());
}

bool BrowserWindow::SelectTabByPath(const std::wstring &itemPath)
{
	unique_pidl_absolute pidlItem;
	HRESULT hr = ParseDisplayNameForNavigation(itemPath, pidlItem);

	if (FAILED(hr) || !pidlItem)
	{
		return false;
	}

	auto resolvedPath = MaybeResolveParsingPath(pidlItem.get());

	if (!resolvedPath)
	{
		return false;
	}

	auto *tabContainer = GetActiveTabContainer();
	auto *tab = FindTabByResolvedPath(tabContainer, *resolvedPath);

	if (!tab)
	{
		return false;
	}

	tabContainer->SelectTab(*tab);

	auto *shellBrowserImpl = tab->GetShellBrowserImpl();

	if (!shellBrowserImpl)
	{
		return true;
	}

	shellBrowserImpl->PostNotifyShellOfCurrentLocation();
	return true;
}

bool BrowserWindow::ShowItemInFolder(const std::wstring &itemPath,
	OpenFolderDisposition openFolderDisposition)
{
	unique_pidl_absolute pidlItem;
	HRESULT hr = ParseDisplayNameForNavigation(itemPath, pidlItem);

	if (FAILED(hr) || !pidlItem)
	{
		return false;
	}

	unique_pidl_absolute pidlParent(ILCloneFull(pidlItem.get()));

	if (!pidlParent || !ILRemoveLastID(pidlParent.get()))
	{
		return false;
	}

	auto parentPath = MaybeResolveParsingPath(pidlParent.get());

	if (!parentPath)
	{
		return false;
	}

	auto *tabContainer = GetActiveTabContainer();
	Tab *targetTab = FindTabByResolvedPath(tabContainer, *parentPath);
	bool reusedExistingTab = targetTab != nullptr;

	if (targetTab)
	{
		if (!SetCurrentEntrySelection(targetTab, pidlItem.get()))
		{
			return false;
		}

		tabContainer->SelectTab(*targetTab);
	}
	else
	{
		OpenItem(*parentPath, openFolderDisposition);

		tabContainer = GetActiveTabContainer();

		if (!tabContainer)
		{
			return false;
		}

		targetTab = &tabContainer->GetSelectedTab();

		if (!SetCurrentEntrySelection(targetTab, pidlItem.get()))
		{
			return false;
		}
	}

	auto *shellBrowserImpl = targetTab->GetShellBrowserImpl();

	if (!shellBrowserImpl)
	{
		return false;
	}

	if (reusedExistingTab)
	{
		shellBrowserImpl->PostNotifyShellOfCurrentLocation();
	}

	if (StrCmpIW(shellBrowserImpl->GetDirectoryPath().c_str(), parentPath->c_str()) == 0)
	{
		shellBrowserImpl->SelectItems({ pidlItem.get() });
	}

	return true;
}
