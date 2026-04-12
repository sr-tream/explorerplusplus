// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "ShellView.h"
#include "BrowserWindow.h"
#include "ShellBrowser/ShellBrowserImpl.h"
#include "ShellBrowser/ShellNavigationController.h"
#include "Tab.h"
#include "TabContainer.h"

namespace
{

std::optional<std::wstring> ResolveSelectedItemPath(PCIDLIST_ABSOLUTE directoryPidl,
	PCUITEMID_CHILD pidlItem)
{
	unique_pidl_absolute pidlComplete(ILCombine(directoryPidl, pidlItem));

	if (!pidlComplete)
	{
		return std::nullopt;
	}

	std::wstring itemPath;
	HRESULT hr = GetDisplayName(pidlComplete.get(), SHGDN_FORPARSING, itemPath);

	if (FAILED(hr) || itemPath.empty())
	{
		return std::nullopt;
	}

	return itemPath;
}

}

ShellView::ShellView(WeakPtr<ShellBrowserImpl> shellBrowserWeak, const BrowserList *browserList,
	int browserId, PCIDLIST_ABSOLUTE directoryPidl, bool switchToTabOnSelect) :
	m_shellBrowserWeak(shellBrowserWeak),
	m_browserList(browserList),
	m_browserId(browserId),
	m_directoryPidl(directoryPidl),
	m_switchToTabOnSelect(switchToTabOnSelect)
{
}

// IShellView
IFACEMETHODIMP ShellView::TranslateAccelerator(MSG *msg)
{
	UNREFERENCED_PARAMETER(msg);

	return E_NOTIMPL;
}

IFACEMETHODIMP ShellView::EnableModeless(BOOL enable)
{
	UNREFERENCED_PARAMETER(enable);

	return E_NOTIMPL;
}

IFACEMETHODIMP ShellView::UIActivate(UINT state)
{
	UNREFERENCED_PARAMETER(state);

	return E_NOTIMPL;
}

IFACEMETHODIMP ShellView::Refresh()
{
	return E_NOTIMPL;
}

IFACEMETHODIMP ShellView::CreateViewWindow(IShellView *previous, LPCFOLDERSETTINGS folderSettings,
	IShellBrowser *shellBrowser, RECT *view, HWND *hwnd)
{
	UNREFERENCED_PARAMETER(previous);
	UNREFERENCED_PARAMETER(folderSettings);
	UNREFERENCED_PARAMETER(shellBrowser);
	UNREFERENCED_PARAMETER(view);
	UNREFERENCED_PARAMETER(hwnd);

	return E_NOTIMPL;
}

IFACEMETHODIMP ShellView::DestroyViewWindow()
{
	return E_NOTIMPL;
}

IFACEMETHODIMP ShellView::GetCurrentInfo(LPFOLDERSETTINGS folderSettings)
{
	UNREFERENCED_PARAMETER(folderSettings);

	return E_NOTIMPL;
}

IFACEMETHODIMP ShellView::AddPropertySheetPages(DWORD reserved, LPFNSVADDPROPSHEETPAGE callback,
	LPARAM lParam)
{
	UNREFERENCED_PARAMETER(reserved);
	UNREFERENCED_PARAMETER(callback);
	UNREFERENCED_PARAMETER(lParam);

	return E_NOTIMPL;
}

IFACEMETHODIMP ShellView::SaveViewState()
{
	return E_NOTIMPL;
}

IFACEMETHODIMP ShellView::SelectItem(PCUITEMID_CHILD pidlItem, SVSIF flags)
{
	if (!m_shellBrowserWeak)
	{
		if (WI_IsFlagSet(flags, SVSI_SELECT))
		{
			return FallbackSelectItem(pidlItem);
		}

		return E_FAIL;
	}

	if (flags == SVSI_EDIT)
	{
		auto pidlComplete =
			unique_pidl_absolute(ILCombine(m_shellBrowserWeak->GetDirectoryIdl().get(), pidlItem));
		m_shellBrowserWeak->QueueRename(pidlComplete.get());
		return S_OK;
	}
	else if (WI_IsFlagSet(flags, SVSI_SELECT))
	{
		if (m_switchToTabOnSelect)
		{
			m_shellBrowserWeak->GetTab()->GetTabContainer()->SelectTab(
				*m_shellBrowserWeak->GetTab());
		}

		auto pidlComplete =
			unique_pidl_absolute(ILCombine(m_shellBrowserWeak->GetDirectoryIdl().get(), pidlItem));

		auto *currentEntry = m_shellBrowserWeak->GetNavigationController()->GetCurrentEntry();

		if (currentEntry)
		{
			currentEntry->SetSelectedItems({ pidlComplete.get() });
		}

		m_shellBrowserWeak->SelectItems({ pidlComplete.get() });

		return S_OK;
	}

	return E_NOTIMPL;
}

HRESULT ShellView::FallbackSelectItem(PCUITEMID_CHILD pidlItem) const
{
	auto itemPath = ResolveSelectedItemPath(m_directoryPidl.Raw(), pidlItem);

	if (!itemPath)
	{
		return E_FAIL;
	}

	if (!m_browserList)
	{
		return E_FAIL;
	}

	auto *browser = m_browserList->MaybeGetById(m_browserId);

	if (!browser)
	{
		return E_FAIL;
	}

	if (!browser->ShowItemInFolder(*itemPath, OpenFolderDisposition::ForegroundTab))
	{
		return E_FAIL;
	}

	browser->Activate();
	browser->FocusActiveTab();

	return S_OK;
}

IFACEMETHODIMP ShellView::GetItemObject(UINT item, REFIID riid, void **ppv)
{
	UNREFERENCED_PARAMETER(item);
	UNREFERENCED_PARAMETER(riid);
	UNREFERENCED_PARAMETER(ppv);

	return E_NOTIMPL;
}

// IOleWindow
IFACEMETHODIMP ShellView::GetWindow(HWND *hwnd)
{
	UNREFERENCED_PARAMETER(hwnd);

	return E_NOTIMPL;
}

IFACEMETHODIMP ShellView::ContextSensitiveHelp(BOOL enterMode)
{
	UNREFERENCED_PARAMETER(enterMode);

	return E_NOTIMPL;
}

namespace winrt
{
template <>
bool is_guid_of<IShellView>(guid const &id) noexcept
{
	return is_guid_of<IShellView, IOleWindow>(id);
}
}
