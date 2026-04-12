// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "ProcessManager.h"
#include "BrowserList.h"
#include "BrowserWindow.h"
#include "CommandLine.h"
#include "Config.h"
#include "ShellBrowser/DocumentServiceProvider.h"
#include "ShellBrowser/ShellBrowser.h"
#include "ShellView.h"
#include "ShellBrowser/WebBrowserApp.h"
#include "TestHelper.h"
#include "Version.h"
#include "VersionHelper.h"
#include "../Helper/MessageWindowHelper.h"
#include "../Helper/ShellHelper.h"
#include "../Helper/WilExtraTypes.h"
#include "../Helper/WindowSubclass.h"
#include <propvarutil.h>

#pragma comment(lib, "runtimeobject.lib")

namespace
{

enum class CopyDataOperation : ULONG_PTR
{
	OpenDefaultItem = 0,
	OpenDirectory = 1,
	ShowItemInFolder = 2
};

constexpr DWORD SHELL_SELECTION_PROXY_TIMEOUT_MS = 5000;

void SendCopyDataMessage(HWND existingWindow, CopyDataOperation operation,
	const std::wstring *payload = nullptr)
{
	COPYDATASTRUCT cds = {};
	cds.dwData = static_cast<ULONG_PTR>(operation);

	if (payload)
	{
		cds.cbData = static_cast<DWORD>(payload->size() * sizeof(wchar_t));
		cds.lpData = const_cast<wchar_t *>(payload->c_str());
	}

	SendMessage(existingWindow, WM_COPYDATA, NULL, reinterpret_cast<LPARAM>(&cds));
}

class SelectionForwardingShellView :
	public winrt::implements<SelectionForwardingShellView, IShellView, winrt::non_agile>
{
public:
	SelectionForwardingShellView(HWND existingWindow, PCIDLIST_ABSOLUTE directoryPidl,
		bool *selectionForwarded) :
		m_existingWindow(existingWindow),
		m_directoryPidl(directoryPidl),
		m_selectionForwarded(selectionForwarded)
	{
	}

	// IShellView
	IFACEMETHODIMP TranslateAccelerator(MSG *msg)
	{
		UNREFERENCED_PARAMETER(msg);
		return E_NOTIMPL;
	}

	IFACEMETHODIMP EnableModeless(BOOL enable)
	{
		UNREFERENCED_PARAMETER(enable);
		return E_NOTIMPL;
	}

	IFACEMETHODIMP UIActivate(UINT state)
	{
		UNREFERENCED_PARAMETER(state);
		return E_NOTIMPL;
	}

	IFACEMETHODIMP Refresh()
	{
		return E_NOTIMPL;
	}

	IFACEMETHODIMP CreateViewWindow(IShellView *previous, LPCFOLDERSETTINGS folderSettings,
		IShellBrowser *shellBrowser, RECT *view, HWND *hwnd)
	{
		UNREFERENCED_PARAMETER(previous);
		UNREFERENCED_PARAMETER(folderSettings);
		UNREFERENCED_PARAMETER(shellBrowser);
		UNREFERENCED_PARAMETER(view);
		UNREFERENCED_PARAMETER(hwnd);
		return E_NOTIMPL;
	}

	IFACEMETHODIMP DestroyViewWindow()
	{
		return E_NOTIMPL;
	}

	IFACEMETHODIMP GetCurrentInfo(LPFOLDERSETTINGS folderSettings)
	{
		UNREFERENCED_PARAMETER(folderSettings);
		return E_NOTIMPL;
	}

	IFACEMETHODIMP AddPropertySheetPages(DWORD reserved, LPFNSVADDPROPSHEETPAGE callback,
		LPARAM lParam)
	{
		UNREFERENCED_PARAMETER(reserved);
		UNREFERENCED_PARAMETER(callback);
		UNREFERENCED_PARAMETER(lParam);
		return E_NOTIMPL;
	}

	IFACEMETHODIMP SaveViewState()
	{
		return E_NOTIMPL;
	}

	IFACEMETHODIMP SelectItem(PCUITEMID_CHILD pidlItem, SVSIF flags)
	{
		if (!WI_IsFlagSet(flags, SVSI_SELECT))
		{
			return E_NOTIMPL;
		}

		unique_pidl_absolute pidlComplete(ILCombine(m_directoryPidl.Raw(), pidlItem));

		if (!pidlComplete)
		{
			return E_OUTOFMEMORY;
		}

		std::wstring itemPath;
		HRESULT hr = GetDisplayName(pidlComplete.get(), SHGDN_FORPARSING, itemPath);

		if (FAILED(hr) || itemPath.empty())
		{
			return E_FAIL;
		}

		SendCopyDataMessage(m_existingWindow, CopyDataOperation::ShowItemInFolder, &itemPath);
		*m_selectionForwarded = true;

		return S_OK;
	}

	IFACEMETHODIMP GetItemObject(UINT item, REFIID riid, void **ppv)
	{
		UNREFERENCED_PARAMETER(item);
		UNREFERENCED_PARAMETER(riid);
		UNREFERENCED_PARAMETER(ppv);
		return E_NOTIMPL;
	}

	// IOleWindow
	IFACEMETHODIMP GetWindow(HWND *hwnd)
	{
		UNREFERENCED_PARAMETER(hwnd);
		return E_NOTIMPL;
	}

	IFACEMETHODIMP ContextSensitiveHelp(BOOL enterMode)
	{
		UNREFERENCED_PARAMETER(enterMode);
		return E_NOTIMPL;
	}

private:
	const HWND m_existingWindow;
	const PidlAbsolute m_directoryPidl;
	bool *const m_selectionForwarded;
};

class ShellSelectionProxy
{
public:
	ShellSelectionProxy(HWND existingWindow, const std::wstring &directory) :
		m_existingWindow(existingWindow),
		m_directory(directory)
	{
	}

	bool Initialize()
	{
		HRESULT hr = ParseDisplayNameForNavigation(m_directory, m_directoryPidl);

		if (FAILED(hr) || !m_directoryPidl)
		{
			return false;
		}

		m_shellWindows = winrt::try_create_instance<IShellWindows>(CLSID_ShellWindows, CLSCTX_ALL);

		if (!m_shellWindows)
		{
			return false;
		}

		m_proxyWindow = MessageWindowHelper::CreateMessageOnlyWindow();

		wil::unique_variant pidlVariant;
		hr = InitVariantFromBuffer(m_directoryPidl.get(), ILGetSize(m_directoryPidl.get()),
			&pidlVariant);

		if (FAILED(hr))
		{
			return false;
		}

		wil::unique_variant empty;

		m_shellWindowCookie.associate(m_shellWindows.get());
		hr = m_shellWindows->RegisterPending(GetCurrentThreadId(), &pidlVariant, &empty, SWC_BROWSER,
			&m_shellWindowCookie);

		if (FAILED(hr))
		{
			return false;
		}

		m_document = winrt::make_self<DocumentServiceProvider>();
		m_shellView = winrt::make_self<SelectionForwardingShellView>(m_existingWindow,
			m_directoryPidl.get(), &m_selectionForwarded);
		m_document->RegisterService(IID_IFolderView, m_shellView.as<IUnknown>());
		m_document->RegisterService(SID_DefView, m_shellView.as<IUnknown>());

		m_browserApp = winrt::make_self<WebBrowserApp>(m_proxyWindow.get(), m_document.get());

		long registeredCookie;
#ifdef __clang__
		#pragma clang diagnostic push
		#pragma clang diagnostic ignored "-Wpointer-to-int-cast"
#endif
#pragma warning(push)
#pragma warning(disable : 4311 4302)
		hr = m_shellWindows->Register(m_browserApp.get(), reinterpret_cast<long>(m_proxyWindow.get()),
			SWC_BROWSER, &registeredCookie);
#pragma warning(pop)
#ifdef __clang__
		#pragma clang diagnostic pop
#endif

		if (FAILED(hr))
		{
			return false;
		}

		return true;
	}

	void WaitForSelection()
	{
		DWORD startTick = GetTickCount();

		while (!m_selectionForwarded)
		{
			DWORD elapsed = GetTickCount() - startTick;

			if (elapsed >= SHELL_SELECTION_PROXY_TIMEOUT_MS)
			{
				break;
			}

			DWORD waitTime = SHELL_SELECTION_PROXY_TIMEOUT_MS - elapsed;
			DWORD waitResult = MsgWaitForMultipleObjectsEx(0, nullptr, waitTime, QS_ALLINPUT,
				MWMO_INPUTAVAILABLE);

			if (waitResult != WAIT_OBJECT_0)
			{
				break;
			}

			MSG msg;

			while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}

	void NotifyShellOfNavigation()
	{
		wil::unique_variant pidlVariant;
		HRESULT hr =
			InitVariantFromBuffer(m_directoryPidl.get(), ILGetSize(m_directoryPidl.get()), &pidlVariant);

		if (FAILED(hr))
		{
			return;
		}

		m_shellWindows->OnNavigate(m_shellWindowCookie.get(), &pidlVariant);
	}

private:
	const HWND m_existingWindow;
	const std::wstring m_directory;
	bool m_selectionForwarded = false;
	unique_pidl_absolute m_directoryPidl;
	winrt::com_ptr<IShellWindows> m_shellWindows;
	wil::unique_hwnd m_proxyWindow;
	unique_shell_window_cookie m_shellWindowCookie;
	winrt::com_ptr<DocumentServiceProvider> m_document;
	winrt::com_ptr<SelectionForwardingShellView> m_shellView;
	winrt::com_ptr<WebBrowserApp> m_browserApp;
};

std::unique_ptr<ShellSelectionProxy> MaybeCreateShellSelectionProxy(HWND existingWindow,
	const CommandLine::Settings *commandLineSettings)
{
	if (IsInTest())
	{
		return nullptr;
	}

	if (!commandLineSettings->filesToSelect.empty() || commandLineSettings->directories.size() != 1)
	{
		return nullptr;
	}

	auto selectionProxy = std::make_unique<ShellSelectionProxy>(existingWindow,
		commandLineSettings->directories.front());

	if (!selectionProxy->Initialize())
	{
		return nullptr;
	}

	return selectionProxy;
}

}

ProcessManager::ProcessManager(const BrowserList *browserList) : m_browserList(browserList)
{
}

bool ProcessManager::InitializeCurrentProcess(const CommandLine::Settings *commandLineSettings,
	const Config *config, const std::wstring &overriddenWindowName)
{
	CHECK(!m_initializationRun);

	m_initializationRun = true;

	wil::unique_mutex_nothrow mutex;
	bool res = mutex.try_create(L"Explorer++FindExistingWindow");

	if (!res)
	{
		return false;
	}

	auto lock = mutex.acquire();

	// Using the version number as the window name will mean that when attempting to find an
	// existing window, only a window that's created by the current version will match. Running
	// different versions simultaneously isn't a supported scenario, so the fact that
	// allowMultipleInstances won't work in that situation isn't a bug.
	auto windowName = VersionHelper::GetVersion().GetString();

	if (!overriddenWindowName.empty())
	{
		CHECK(IsInTest());
		windowName = overriddenWindowName;
	}

	HWND existingWindow = FindWindowEx(HWND_MESSAGE, nullptr,
		MessageWindowHelper::MESSAGE_CLASS_NAME, windowName.c_str());

	if (existingWindow)
	{
		if (commandLineSettings->jumplistNewTab)
		{
			AttemptToNotifyExistingProcess(existingWindow);
			return false;
		}

		if (!config->allowMultipleInstances)
		{
			auto selectionProxy = MaybeCreateShellSelectionProxy(existingWindow, commandLineSettings);
			AttemptToNotifyExistingProcess(existingWindow, commandLineSettings->directories,
				commandLineSettings->filesToSelect);

			if (selectionProxy)
			{
				selectionProxy->NotifyShellOfNavigation();
				selectionProxy->WaitForSelection();
			}

			return false;
		}
	}

	m_messageWindow = MessageWindowHelper::CreateMessageOnlyWindow(windowName);
	m_messageWindowSubclass = std::make_unique<WindowSubclass>(m_messageWindow.get(),
		std::bind_front(&ProcessManager::MessageWindowProc, this));

	return true;
}

LRESULT ProcessManager::MessageWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_COPYDATA:
		OnCopyData(reinterpret_cast<COPYDATASTRUCT *>(lParam));
		return TRUE;
	}

	return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void ProcessManager::OnCopyData(const COPYDATASTRUCT *cds)
{
	auto *browser = m_browserList->GetLastActive();
	CopyDataOperation operation = CopyDataOperation::OpenDefaultItem;

	if (!browser)
	{
		return;
	}

	if (cds->cbData > 0)
	{
		operation = static_cast<CopyDataOperation>(cds->dwData);

		// Older builds didn't set dwData, so a message with a payload and a zero value still
		// represents a directory open request.
		if (operation == CopyDataOperation::OpenDefaultItem)
		{
			operation = CopyDataOperation::OpenDirectory;
		}
	}

	switch (operation)
	{
	case CopyDataOperation::OpenDefaultItem:
		browser->OpenDefaultItem(OpenFolderDisposition::ForegroundTab);
		break;

	case CopyDataOperation::OpenDirectory:
	{
		std::wstring directory(static_cast<wchar_t *>(cds->lpData), cds->cbData / sizeof(wchar_t));

		if (!browser->SelectTabByPath(directory))
		{
			browser->OpenItem(directory, OpenFolderDisposition::ForegroundTab);
		}
	}
	break;

	case CopyDataOperation::ShowItemInFolder:
	{
		std::wstring itemPath(static_cast<wchar_t *>(cds->lpData), cds->cbData / sizeof(wchar_t));
		browser->ShowItemInFolder(itemPath, OpenFolderDisposition::ForegroundTab);
	}
	break;
	}

	browser->Activate();
	browser->FocusActiveTab();
}

void ProcessManager::AttemptToNotifyExistingProcess(HWND existingWindow,
	const std::vector<std::wstring> &directories, const std::vector<std::wstring> &filesToSelect)
{
	DWORD processId;
	auto threadId = GetWindowThreadProcessId(existingWindow, &processId);

	if (threadId == 0)
	{
		return;
	}

	AllowSetForegroundWindow(processId);

	for (const auto &fileToSelect : filesToSelect)
	{
		SendCopyDataMessage(existingWindow, CopyDataOperation::ShowItemInFolder, &fileToSelect);
	}

	for (const auto &directory : directories)
	{
		SendCopyDataMessage(existingWindow, CopyDataOperation::OpenDirectory, &directory);
	}

	if (directories.empty() && filesToSelect.empty())
	{
		SendCopyDataMessage(existingWindow, CopyDataOperation::OpenDefaultItem);
	}
}
