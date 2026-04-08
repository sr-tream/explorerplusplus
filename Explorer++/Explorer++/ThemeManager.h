// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include <boost/signals2.hpp>
#include <commctrl.h>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class DarkModeColorProvider;
class DarkModeManager;
class WindowSubclass;

// Based on
// https://github.com/TortoiseGit/TortoiseGit/blob/2419d47129410d0aa371929d674bf21122c0b581/src/Utils/Theme.cpp.
class ThemeManager
{
public:
	ThemeManager(DarkModeManager *darkModeManager,
		const DarkModeColorProvider *darkModeColorProvider);
	~ThemeManager();

	// This will theme a top-level window, plus all of its nested children. Once a window is
	// tracked, any changes to the dark mode status will result in the window theme being
	// automatically updated.
	// Note that these methods shouldn't be called directly; instead ThemeWindowTracker should be
	// used.
	void TrackTopLevelWindow(HWND hwnd);
	void UntrackTopLevelWindow(HWND hwnd);

	// This should only be called for child windows that are dynamically created. It will theme the
	// child window (plus all nested children). Child windows that exist when the parent is
	// initialized will be covered by TrackTopLevelWindow().
	void ApplyThemeToWindowAndChildren(HWND hwnd);

private:
	static constexpr wchar_t DIALOG_CLASS_NAME[] = L"#32770";

	void OnDarkModeStatusChanged();

	void ApplyThemeToWindow(HWND hwnd);
	BOOL ProcessChildWindow(HWND hwnd);
	BOOL ProcessThreadWindow(HWND hwnd);
	void ApplyThemeToMainWindow(HWND hwnd, bool enableDarkMode);
	void ApplyThemeToDialog(HWND hwnd, bool enableDarkMode);
	void ApplyThemeToTabControl(HWND hwnd, bool enableDarkMode);
	void ApplyThemeToListView(HWND hwnd, bool enableDarkMode);
	void ApplyThemeToHeader(HWND hwnd);
	void ApplyThemeToTreeView(HWND hwnd, bool enableDarkMode);
	void ApplyThemeToRichEdit(HWND hwnd, bool enableDarkMode);
	void ApplyThemeToRebar(HWND hwnd, bool enableDarkMode);
	void ApplyThemeToToolbar(HWND hwnd, bool enableDarkMode);
	void ApplyThemeToComboBoxEx(HWND hwnd, bool enableDarkMode);
	void ApplyThemeToComboBox(HWND hwnd);
	void ApplyThemeToEditControl(HWND hwnd, bool enableDarkMode);
	void ApplyThemeToButton(HWND hwnd, bool enableDarkMode);
	void ApplyThemeToTooltips(HWND hwnd);
	void ApplyThemeToStatusBar(HWND hwnd, bool enableDarkMode);
	void ApplyThemeToScrollBar(HWND hwnd, bool enableDarkMode);
	void ApplyThemeToUpDownControl(HWND hwnd);

	LRESULT MainWindowSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	HBRUSH GetMenuBarBackgroundBrush(bool enableDarkMode);
	bool ShouldAlwaysShowAccessKeys();
	LRESULT DialogSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT ToolbarParentSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT OnCustomDraw(NMCUSTOMDRAW *customDraw);
	LRESULT OnButtonCustomDraw(NMCUSTOMDRAW *customDraw);
	LRESULT OnToolbarCustomDraw(NMTBCUSTOMDRAW *customDraw);
	LRESULT ComboBoxExSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT TabControlSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT ListViewSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT RebarSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT GroupBoxSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT ScrollBarSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	// Shell dialog dark mode support (property sheets and other shell-created dialogs).
	// These run on the shell dialog's own thread via WINEVENT_INCONTEXT, so all state they access
	// must be static and thread-safe.
	void SetupShellDialogDarkModeHook();
	void TeardownShellDialogDarkModeHook();

	static void CALLBACK ShellDialogWinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event,
		HWND hwnd, LONG idObject, LONG idChild, DWORD idEventThread, DWORD dwmsEventTime);
	static void ApplyDarkModeToShellDialog(HWND hwnd);
	static BOOL CALLBACK ThemeShellDialogChild(HWND hwnd, LPARAM lParam);
	static LRESULT CALLBACK ShellDialogSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
		LPARAM lParam, UINT_PTR subclassId, DWORD_PTR refData);
	static LRESULT CALLBACK ShellTabControlSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
		LPARAM lParam, UINT_PTR subclassId, DWORD_PTR refData);
	static LRESULT CALLBACK ShellGroupBoxSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
		LPARAM lParam, UINT_PTR subclassId, DWORD_PTR refData);
	static LRESULT CALLBACK ShellListViewSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
		LPARAM lParam, UINT_PTR subclassId, DWORD_PTR refData);
	static LRESULT CALLBACK ShellSysLinkSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
		LPARAM lParam, UINT_PTR subclassId, DWORD_PTR refData);
	static LRESULT OnShellButtonCustomDraw(NMCUSTOMDRAW *customDraw);
	static LRESULT OnShellListViewCustomDraw(NMLVCUSTOMDRAW *customDraw);
	static void PaintShellTabControl(HWND hwnd, HDC hdc, const RECT &paintRect);

	using AllowDarkModeForWindowFn = bool(WINAPI *)(HWND hwnd, bool allow);

	static constexpr UINT_PTR SHELL_DIALOG_SUBCLASS_ID = 0x45505044;
	static constexpr UINT_PTR SHELL_TAB_SUBCLASS_ID = 0x45505045;
	static constexpr UINT_PTR SHELL_GROUPBOX_SUBCLASS_ID = 0x45505046;
	static constexpr UINT_PTR SHELL_LISTVIEW_SUBCLASS_ID = 0x45505047;
	static constexpr UINT_PTR SHELL_SYSLINK_SUBCLASS_ID = 0x45505048;
	static constexpr COLORREF SHELL_DLG_BG_COLOR = RGB(32, 32, 32);
	static constexpr COLORREF SHELL_DLG_TEXT_COLOR = RGB(255, 255, 255);
	static constexpr COLORREF SHELL_DLG_BACKGROUND_TEXT_COLOR = RGB(180, 180, 180);
	static constexpr COLORREF SHELL_DLG_DISABLED_TEXT_COLOR = RGB(121, 121, 121);
	static constexpr COLORREF SHELL_DLG_TAB_BG_COLOR = RGB(38, 38, 38);
	static constexpr COLORREF SHELL_DLG_BORDER_COLOR = RGB(120, 120, 120);
	static constexpr COLORREF SHELL_DLG_LINK_COLOR = RGB(100, 180, 255);

	static inline HBRUSH s_shellDlgBgBrush = nullptr;
	static inline HBRUSH s_shellTabBgBrush = nullptr;
	static inline HBRUSH s_shellBorderBrush = nullptr;
	static inline std::atomic<bool> s_shellDarkModeActive{ false };
	static inline DWORD s_mainThreadId = 0;
	static inline bool s_hookInContext = false;
	static inline AllowDarkModeForWindowFn s_AllowDarkModeForWindow = nullptr;

	HWINEVENTHOOK m_shellDialogHook = nullptr;

	DarkModeManager *const m_darkModeManager;
	const DarkModeColorProvider *const m_darkModeColorProvider;
	std::unordered_set<HWND> m_trackedTopLevelWindows;
	std::unordered_map<HWND, int> m_hotTabMap;
	std::vector<boost::signals2::scoped_connection> m_connections;
	std::vector<std::unique_ptr<WindowSubclass>> m_windowSubclasses;
};
