// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
// clang-format off
#include "ThemeManager.h"
// clang-format on
#include "Explorer++.h"
#include "DarkModeColorProvider.h"
#include "DarkModeManager.h"
#include "SystemFontHelper.h"
#include "ThemedTabControlPainter.h"
#include "../Helper/Controls.h"
#include "../Helper/DoubleBufferedPaint.h"
#include "../Helper/DpiCompatibility.h"
#include "../Helper/MenuHelper.h"
#include "../Helper/WindowHelper.h"
#include "../Helper/WindowSubclass.h"
#include <glog/logging.h>
#include <wil/resource.h>
#include <vssym32.h>

ThemeManager::ThemeManager(DarkModeManager *darkModeManager,
	const DarkModeColorProvider *darkModeColorProvider) :
	m_darkModeManager(darkModeManager),
	m_darkModeColorProvider(darkModeColorProvider)
{
	s_mainThreadId = GetCurrentThreadId();

	if (!s_shellDlgBgBrush)
	{
		s_shellDlgBgBrush = CreateSolidBrush(SHELL_DLG_BG_COLOR);
		s_shellTabBgBrush = CreateSolidBrush(SHELL_DLG_TAB_BG_COLOR);
		s_shellBorderBrush = CreateSolidBrush(SHELL_DLG_BORDER_COLOR);
	}

	HMODULE uxtheme = GetModuleHandle(L"uxtheme.dll");

	if (uxtheme)
	{
		s_AllowDarkModeForWindow =
			std::bit_cast<AllowDarkModeForWindowFn>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(133)));
	}

	if (darkModeManager->IsDarkModeEnabled())
	{
		s_shellDarkModeActive.store(true, std::memory_order_relaxed);
		SetupShellDialogDarkModeHook();
	}

	m_connections.push_back(darkModeManager->darkModeStatusChanged.AddObserver(
		std::bind(&ThemeManager::OnDarkModeStatusChanged, this)));
}

ThemeManager::~ThemeManager()
{
	TeardownShellDialogDarkModeHook();
}

void ThemeManager::OnDarkModeStatusChanged()
{
	m_windowSubclasses.clear();

	bool darkMode = m_darkModeManager->IsDarkModeEnabled();
	s_shellDarkModeActive.store(darkMode, std::memory_order_relaxed);

	if (darkMode)
	{
		SetupShellDialogDarkModeHook();
	}
	else
	{
		TeardownShellDialogDarkModeHook();
	}

	for (HWND hwnd : m_trackedTopLevelWindows)
	{
		ApplyThemeToWindowAndChildren(hwnd);
	}
}

void ThemeManager::TrackTopLevelWindow(HWND hwnd)
{
	ApplyThemeToWindowAndChildren(hwnd);

	auto [itr, didInsert] = m_trackedTopLevelWindows.insert(hwnd);
	DCHECK(didInsert);
}

void ThemeManager::UntrackTopLevelWindow(HWND hwnd)
{
	auto numErased = m_trackedTopLevelWindows.erase(hwnd);
	DCHECK_EQ(numErased, 1u);
}

void ThemeManager::ApplyThemeToWindowAndChildren(HWND hwnd)
{
	ApplyThemeToWindow(hwnd);
	EnumChildWindows(
		hwnd,
		[](HWND childWindow, LPARAM lParam)
		{
			auto themeManager = reinterpret_cast<ThemeManager *>(lParam);
			return themeManager->ProcessChildWindow(childWindow);
		},
		reinterpret_cast<LPARAM>(this));

	// Tooltip windows won't be enumerated by EnumChildWindows(). They will, however, be enumerated
	// by EnumThreadWindows(), which is why that's called here.
	//
	// Note that this is explicitly called after EnumChildWindows(). That way, tooltip windows can
	// be initialized during the call to EnumChildWindows() (since they won't necessarily exist
	// initially). Those tooltip windows will then be processed as part of the call to
	// EnumThreadWindows().
	EnumThreadWindows(
		GetCurrentThreadId(),
		[](HWND threadWindow, LPARAM lParam)
		{
			auto themeManager = reinterpret_cast<ThemeManager *>(lParam);
			return themeManager->ProcessThreadWindow(threadWindow);
		},
		reinterpret_cast<LPARAM>(this));

	RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_ERASE | RDW_FRAME);
}

BOOL ThemeManager::ProcessChildWindow(HWND hwnd)
{
	ApplyThemeToWindow(hwnd);
	return TRUE;
}

BOOL ThemeManager::ProcessThreadWindow(HWND hwnd)
{
	WCHAR className[256];
	auto res = GetClassName(hwnd, className, std::size(className));

	if (res == 0)
	{
		DCHECK(false);
		return TRUE;
	}

	if (lstrcmp(className, TOOLTIPS_CLASS) != 0)
	{
		return TRUE;
	}

	ApplyThemeToWindow(hwnd);

	return TRUE;
}

void ThemeManager::ApplyThemeToWindow(HWND hwnd)
{
	bool enableDarkMode = m_darkModeManager->IsDarkModeEnabled();
	m_darkModeManager->AllowDarkModeForWindow(hwnd, enableDarkMode);

	// The maximum length of a class name is 256 characters (see the documentation for lpszClassName
	// in https://learn.microsoft.com/en-au/windows/win32/api/winuser/ns-winuser-wndclassw).
	WCHAR className[256];
	auto res = GetClassName(hwnd, className, std::size(className));

	if (res == 0)
	{
		DCHECK(false);
		return;
	}

	if (lstrcmp(className, Explorerplusplus::WINDOW_CLASS_NAME) == 0)
	{
		ApplyThemeToMainWindow(hwnd, enableDarkMode);
	}
	else if (lstrcmp(className, DIALOG_CLASS_NAME) == 0)
	{
		ApplyThemeToDialog(hwnd, enableDarkMode);
	}
	else if (lstrcmp(className, WC_TABCONTROL) == 0)
	{
		ApplyThemeToTabControl(hwnd, enableDarkMode);
	}
	else if (lstrcmp(className, WC_LISTVIEW) == 0)
	{
		ApplyThemeToListView(hwnd, enableDarkMode);
	}
	else if (lstrcmp(className, WC_HEADER) == 0)
	{
		ApplyThemeToHeader(hwnd);
	}
	else if (lstrcmp(className, WC_TREEVIEW) == 0)
	{
		ApplyThemeToTreeView(hwnd, enableDarkMode);
	}
	else if (lstrcmp(className, MSFTEDIT_CLASS) == 0)
	{
		ApplyThemeToRichEdit(hwnd, enableDarkMode);
	}
	else if (lstrcmp(className, REBARCLASSNAME) == 0)
	{
		ApplyThemeToRebar(hwnd, enableDarkMode);
	}
	else if (lstrcmp(className, TOOLBARCLASSNAME) == 0)
	{
		ApplyThemeToToolbar(hwnd, enableDarkMode);
	}
	else if (lstrcmp(className, WC_COMBOBOXEX) == 0)
	{
		ApplyThemeToComboBoxEx(hwnd, enableDarkMode);
	}
	else if (lstrcmp(className, WC_COMBOBOX) == 0)
	{
		ApplyThemeToComboBox(hwnd);
	}
	else if (lstrcmp(className, WC_EDIT) == 0)
	{
		ApplyThemeToEditControl(hwnd, enableDarkMode);
	}
	else if (lstrcmp(className, WC_BUTTON) == 0)
	{
		ApplyThemeToButton(hwnd, enableDarkMode);
	}
	else if (lstrcmp(className, TOOLTIPS_CLASS) == 0)
	{
		ApplyThemeToTooltips(hwnd);
	}
	else if (lstrcmp(className, STATUSCLASSNAME) == 0)
	{
		ApplyThemeToStatusBar(hwnd, enableDarkMode);
	}
	else if (lstrcmp(className, WC_SCROLLBAR) == 0)
	{
		ApplyThemeToScrollBar(hwnd, enableDarkMode);
	}
	else if (lstrcmp(className, UPDOWN_CLASS) == 0)
	{
		ApplyThemeToUpDownControl(hwnd);
	}
}

void ThemeManager::ApplyThemeToMainWindow(HWND hwnd, bool enableDarkMode)
{
	// There's no need to owner-draw the menu bar if dark mode isn't supported (in practice, this
	// means that the menu bar will only be owner-drawn on Windows 10 and 11). Additionally,
	// owner-drawing the menu bar is problematic on Windows 7, for at least two reasons:
	//
	// 1. In the default theme, the menu bar background isn't a flat color. Rather, it's more like a
	// gradient that's specified in a bitmap. DrawThemeBackground() can be used to easily draw that
	// background, however, it appears that Windows only allows you to set the background brush for
	// the menu bar, rather than providing a DC to paint into.
	//
	// 2. Visual styles can be turned off, so the current owner-drawing implementation wouldn't work
	// in that scenario.
	if (!m_darkModeManager->IsDarkModeSupported())
	{
		return;
	}

	BOOL dark = enableDarkMode;
	DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

	m_windowSubclasses.push_back(std::make_unique<WindowSubclass>(hwnd,
		std::bind_front(&ThemeManager::MainWindowSubclass, this)));

	auto mainMenu = GetMenu(hwnd);
	int numItems = GetMenuItemCount(mainMenu);

	for (int i = 0; i < numItems; i++)
	{
		MENUITEMINFO menuItemInfo = {};
		menuItemInfo.cbSize = sizeof(menuItemInfo);
		menuItemInfo.fMask = MIIM_FTYPE;
		auto res = GetMenuItemInfo(mainMenu, i, true, &menuItemInfo);
		CHECK(res);

		// Removing the MFT_OWNERDRAW style once it's been applied to the menu bar items appears to
		// be problematic. The resulting items aren't spaced correctly. Because of that, the menu
		// bar will always be owner-drawn, regardless of the theme.
		WI_SetFlag(menuItemInfo.fType, MFT_OWNERDRAW);
		WI_SetFlag(menuItemInfo.fMask, MIIM_DATA);
		menuItemInfo.dwItemData = i;
		res = SetMenuItemInfo(mainMenu, i, true, &menuItemInfo);
		CHECK(res);
	}

	// Turning on owner draw for at least one item in the menu bar will disable visual styles in the
	// bar. That's useful here, as (1) the items will be oner-drawn, so the presence or absence of
	// visual styles doesn't matter and (2) disabling visual styles means that the background brush
	// here will be used to paint the empty section of the bar (the section behind each item will be
	// painted when drawing the item).
	//
	// Note that the background should really be drawn by DrawThemeBackground(), however, as noted
	// above, Windows seemingly doesn't provide any documented way of drawing directly into the menu
	// bar DC. The most you can do is provide a background brush. That's fine on Windows 10 and
	// 11, where the background colors are effectively flat and a solid brush works.
	MENUINFO menuInfo = {};
	menuInfo.cbSize = sizeof(menuInfo);
	menuInfo.fMask = MIM_BACKGROUND;
	menuInfo.hbrBack = GetMenuBarBackgroundBrush(enableDarkMode);
	auto res = SetMenuInfo(mainMenu, &menuInfo);
	CHECK(res);
}

void ThemeManager::ApplyThemeToDialog(HWND hwnd, bool enableDarkMode)
{
	BOOL dark = enableDarkMode;
	DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

	if (enableDarkMode)
	{
		m_windowSubclasses.push_back(std::make_unique<WindowSubclass>(hwnd,
			std::bind_front(&ThemeManager::DialogSubclass, this)));
	}
}

void ThemeManager::ApplyThemeToTabControl(HWND hwnd, bool enableDarkMode)
{
	if (enableDarkMode)
	{
		m_windowSubclasses.push_back(std::make_unique<WindowSubclass>(hwnd,
			std::bind_front(&ThemeManager::TabControlSubclass, this)));
	}
}

void ThemeManager::ApplyThemeToListView(HWND hwnd, bool enableDarkMode)
{
	DWORD extendedStyle = ListView_GetExtendedListViewStyle(hwnd);

	if (enableDarkMode)
	{
		SetWindowTheme(hwnd, L"ItemsView", nullptr);
	}
	else
	{
		SetWindowTheme(hwnd, L"Explorer", nullptr);
	}

	if (WI_IsFlagSet(extendedStyle, LVS_EX_TRANSPARENTBKGND))
	{
		// Setting the window theme above will clear the LVS_EX_TRANSPARENTBKGND style. So, if that
		// style was set, it will be restored here.
		ListView_SetExtendedListViewStyle(hwnd, extendedStyle);
	}

	COLORREF backgroundColor;
	COLORREF textColor;

	if (enableDarkMode)
	{
		backgroundColor = m_darkModeColorProvider->GetBackgroundColor();
		textColor = m_darkModeColorProvider->GetTextColor();
	}
	else
	{
		backgroundColor = GetSysColor(COLOR_WINDOW);
		textColor = GetSysColor(COLOR_WINDOWTEXT);
	}

	ListView_SetBkColor(hwnd, backgroundColor);
	ListView_SetTextBkColor(hwnd, backgroundColor);
	ListView_SetTextColor(hwnd, textColor);

	if (enableDarkMode)
	{
		m_windowSubclasses.push_back(std::make_unique<WindowSubclass>(hwnd,
			std::bind_front(&ThemeManager::ListViewSubclass, this)));
	}
}

void ThemeManager::ApplyThemeToHeader(HWND hwnd)
{
	SetWindowTheme(hwnd, L"ItemsView", nullptr);
}

void ThemeManager::ApplyThemeToTreeView(HWND hwnd, bool enableDarkMode)
{
	// When in dark mode, this theme sets the following colors correctly:
	//
	// - the item selection color,
	// - the colors of the arrows that appear to the left of the items,
	// - the color of the scrollbars.
	//
	// It doesn't, however, change the background color, or the text color.
	SetWindowTheme(hwnd, L"Explorer", nullptr);

	COLORREF backgroundColor;
	COLORREF textColor;
	COLORREF insertMarkColor;

	if (enableDarkMode)
	{
		backgroundColor = m_darkModeColorProvider->GetBackgroundColor();
		textColor = m_darkModeColorProvider->GetTextColor();
		insertMarkColor = m_darkModeColorProvider->GetForegroundColor();
	}
	else
	{
		backgroundColor = GetSysColor(COLOR_WINDOW);
		textColor = GetSysColor(COLOR_WINDOWTEXT);
		insertMarkColor = CLR_DEFAULT;
	}

	TreeView_SetBkColor(hwnd, backgroundColor);
	TreeView_SetTextColor(hwnd, textColor);
	TreeView_SetInsertMarkColor(hwnd, insertMarkColor);
}

void ThemeManager::ApplyThemeToRichEdit(HWND hwnd, bool enableDarkMode)
{
	COLORREF backgroundColor;
	COLORREF textColor;

	if (enableDarkMode)
	{
		backgroundColor = m_darkModeColorProvider->GetBackgroundColor();
		textColor = m_darkModeColorProvider->GetTextColor();
	}
	else
	{
		backgroundColor = GetSysColor(COLOR_WINDOW);
		textColor = GetSysColor(COLOR_WINDOWTEXT);
	}

	SendMessage(hwnd, EM_SETBKGNDCOLOR, 0, backgroundColor);

	CHARFORMAT charFormat = {};
	charFormat.cbSize = sizeof(charFormat);
	charFormat.dwMask = CFM_COLOR;
	charFormat.crTextColor = textColor;
	charFormat.dwEffects = 0;
	SendMessage(hwnd, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&charFormat));
}

void ThemeManager::ApplyThemeToRebar(HWND hwnd, bool enableDarkMode)
{
	if (enableDarkMode)
	{
		m_windowSubclasses.push_back(std::make_unique<WindowSubclass>(hwnd,
			std::bind_front(&ThemeManager::RebarSubclass, this)));
	}
}

void ThemeManager::ApplyThemeToToolbar(HWND hwnd, bool enableDarkMode)
{
	// Without this, the DarkMode::Toolbar theme may be applied in dark mode. That theme is
	// problematic (e.g. it results in dropdown buttons being drawn incorrectly, see #519). This
	// call will ensure that the default theme (i.e. Toolbar) will be used in both light mode and
	// dark mode.
	//
	// Using the default theme is fine, since dark mode is implemented in other ways (e.g. by custom
	// drawing).
	SetWindowTheme(hwnd, L"", nullptr);

	COLORREF insertMarkColor;

	if (enableDarkMode)
	{
		insertMarkColor = m_darkModeColorProvider->GetForegroundColor();
	}
	else
	{
		insertMarkColor = CLR_DEFAULT;
	}

	SendMessage(hwnd, TB_SETINSERTMARKCOLOR, 0, insertMarkColor);

	// The tooltips window won't exist until either it's requested using TB_GETTOOLTIPS, or the
	// tooltip needs to be shown. Therefore, calling TB_GETTOOLTIPS will create the tooltip control,
	// if appropriate (the toolbar may not have the TBSTYLE_TOOLTIPS style set, in which case, no
	// tooltip control will be created). The tooltip window will then be themed by the call to
	// EnumThreadWindows() above.
	SendMessage(hwnd, TB_GETTOOLTIPS, 0, 0);

	HWND parent = GetParent(hwnd);
	CHECK(parent);

	if (enableDarkMode)
	{
		// Note that the parent window may end up being subclassed multiple times. That shouldn't
		// have any correctness issues, since when receiving the relevant drawing messages, one of
		// the subclasses (it's not specified which) will perform the appropriate handling. It is
		// inefficient generally, since each subclass will be invoked for other messages as well.
		// That shouldn't be too much of an issue, since there's only a limited number of toolbars,
		// so the number of extraneous subclasses won't be very high.
		m_windowSubclasses.push_back(std::make_unique<WindowSubclass>(parent,
			std::bind_front(&ThemeManager::ToolbarParentSubclass, this)));
	}
}

void ThemeManager::ApplyThemeToComboBoxEx(HWND hwnd, bool enableDarkMode)
{
	if (enableDarkMode)
	{
		m_windowSubclasses.push_back(std::make_unique<WindowSubclass>(hwnd,
			std::bind_front(&ThemeManager::ComboBoxExSubclass, this)));
	}
}

void ThemeManager::ApplyThemeToComboBox(HWND hwnd)
{
	HWND parent = GetParent(hwnd);
	CHECK(parent);

	WCHAR parentClassName[256];
	auto parentClassNameResult = GetClassName(parent, parentClassName, std::size(parentClassName));

	if (parentClassNameResult != 0 && lstrcmp(parentClassName, WC_COMBOBOXEX) == 0)
	{
		SetWindowTheme(hwnd, L"AddressComposited", nullptr);
	}
	else
	{
		SetWindowTheme(hwnd, L"CFD", nullptr);
	}
}

void ThemeManager::ApplyThemeToEditControl(HWND hwnd, bool enableDarkMode)
{
	HWND parent = GetParent(hwnd);
	CHECK(parent);

	WCHAR parentClassName[256];
	auto parentClassNameResult = GetClassName(parent, parentClassName, std::size(parentClassName));

	if (parentClassNameResult != 0 && lstrcmp(parentClassName, WC_COMBOBOX) == 0)
	{
		// The edit control will be themed along with the combobox.
		return;
	}

	if (enableDarkMode)
	{
		SetWindowTheme(hwnd, L"CFD", nullptr);
	}
	else
	{
		SetWindowTheme(hwnd, nullptr, nullptr);
	}
}

void ThemeManager::ApplyThemeToButton(HWND hwnd, bool enableDarkMode)
{
	SetWindowTheme(hwnd, L"Explorer", nullptr);

	auto style = GetWindowLongPtr(hwnd, GWL_STYLE);

	if ((style & BS_TYPEMASK) == BS_GROUPBOX)
	{
		if (enableDarkMode)
		{
			m_windowSubclasses.push_back(std::make_unique<WindowSubclass>(hwnd,
				std::bind_front(&ThemeManager::GroupBoxSubclass, this)));
		}
	}
}

void ThemeManager::ApplyThemeToTooltips(HWND hwnd)
{
	SetWindowTheme(hwnd, L"Explorer", nullptr);
}

void ThemeManager::ApplyThemeToStatusBar(HWND hwnd, bool enableDarkMode)
{
	if (enableDarkMode)
	{
		SetWindowTheme(hwnd, nullptr, L"ExplorerStatusBar");
	}
	else
	{
		// Revert the control back to its default theme (see
		// https://devblogs.microsoft.com/oldnewthing/20181115-00/?p=100225).
		SetWindowTheme(hwnd, nullptr, nullptr);
	}
}

void ThemeManager::ApplyThemeToScrollBar(HWND hwnd, bool enableDarkMode)
{
	auto style = GetWindowLongPtr(hwnd, GWL_STYLE);

	if (WI_IsAnyFlagSet(style, SBS_SIZEGRIP | SBS_SIZEBOX))
	{
		if (enableDarkMode)
		{
			m_windowSubclasses.push_back(std::make_unique<WindowSubclass>(hwnd,
				std::bind_front(&ThemeManager::ScrollBarSubclass, this)));
		}
	}
}

void ThemeManager::ApplyThemeToUpDownControl(HWND hwnd)
{
	// Note that this style is only implemented in Windows 11. That means the control will appear in
	// its normal light style on Windows 10 when dark mode is enabled.
	SetWindowTheme(hwnd, L"Explorer", nullptr);
}

LRESULT ThemeManager::MainWindowSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static wil::unique_htheme theme(OpenThemeData(hwnd, L"Menu"));
	static constexpr DWORD drawFlagsBase = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
	static bool alwaysShowAccessKeys = ShouldAlwaysShowAccessKeys();

	switch (msg)
	{
	case WM_MEASUREITEM:
	{
		auto *measureItem = reinterpret_cast<MEASUREITEMSTRUCT *>(lParam);

		if (measureItem->CtlType != ODT_MENU)
		{
			break;
		}

		auto hdc = wil::GetWindowDC(hwnd);

		// Only items in the menu bar are owner-drawn, so the menu will always be the menu
		// associated with the window.
		auto menu = GetMenu(hwnd);
		CHECK(menu);

		auto text =
			MenuHelper::GetMenuItemString(menu, static_cast<UINT>(measureItem->itemData), true);

		auto logFont = GetSystemFontScaledToWindow(SystemFont::Menu, hwnd);
		wil::unique_hfont font(CreateFontIndirect(&logFont));
		wil::unique_select_object selectFont;

		if (font)
		{
			selectFont = wil::SelectObject(hdc.get(), font.get());
		}

		DCHECK(selectFont);

		RECT textRect;
		HRESULT hr = GetThemeTextExtent(theme.get(), hdc.get(), MENU_BARITEM, MBI_NORMAL,
			text.c_str(), -1, drawFlagsBase, nullptr, &textRect);
		CHECK(SUCCEEDED(hr));

		measureItem->itemWidth = GetRectWidth(&textRect);
		measureItem->itemHeight = GetRectHeight(&textRect);

		return TRUE;
	}
	break;

	// This contains just enough functionality to owner-draw the items in the menu bar; it's not a
	// complete owner-drawn menu implementation. For example, neither checked nor bitmap items have
	// any handling. The code here implicitly assumes that the only menu items that are owner-drawn
	// are the items in the menu bar.
	case WM_DRAWITEM:
	{
		auto *drawItem = reinterpret_cast<DRAWITEMSTRUCT *>(lParam);

		if (drawItem->CtlType != ODT_MENU)
		{
			break;
		}

		int itemState;

		if (WI_IsAnyFlagSet(drawItem->itemState, ODS_INACTIVE | ODS_GRAYED | ODS_DISABLED))
		{
			if (WI_IsFlagSet(drawItem->itemState, ODS_HOTLIGHT))
			{
				itemState = MBI_DISABLEDHOT;
			}
			else if (WI_IsFlagSet(drawItem->itemState, ODS_SELECTED))
			{
				itemState = MBI_DISABLEDPUSHED;
			}
			else
			{
				itemState = MBI_DISABLED;
			}
		}
		else
		{
			if (WI_IsFlagSet(drawItem->itemState, ODS_HOTLIGHT))
			{
				itemState = MBI_HOT;
			}
			else if (WI_IsFlagSet(drawItem->itemState, ODS_SELECTED))
			{
				itemState = MBI_PUSHED;
			}
			else
			{
				itemState = MBI_NORMAL;
			}
		}

		bool darkModeEnabled = m_darkModeManager->IsDarkModeEnabled();
		bool selected = false;
		bool selectionPartiallyTransparent = false;

		if (itemState == MBI_HOT || itemState == MBI_PUSHED || itemState == MBI_DISABLEDHOT
			|| itemState == MBI_DISABLEDPUSHED)
		{
			selected = true;

			if (!darkModeEnabled)
			{
				selectionPartiallyTransparent =
					IsThemeBackgroundPartiallyTransparent(theme.get(), MENU_BARITEM, itemState);
			}
		}

		// The hot item selection rectangle drawn by DrawThemeBackground() may be partially
		// transparent. In that case, the menu bar background will need to be drawn first. In dark
		// mode, the background color and selection color are both opaque, so there's no need to
		// draw both for a single item.
		if (!selected || selectionPartiallyTransparent)
		{
			// In non-dark mode, the background could be drawn by using:
			//
			// DrawThemeBackground(theme.get(), drawItem->hDC, MENU_BARBACKGROUND, MB_ACTIVE,
			//   &drawItem->rcItem, nullptr);
			//
			// However, if that background isn't simply a flat color, the background underneath each
			// item will differ from the background shown in the empty space. In Windows 11, for
			// example, the themed background includes a 1px border at the bottom, so drawing the
			// themed background here would result in a slight difference. It's also possible the
			// theme could change in the future. So, the safest thing to do is to use the same brush
			// to draw the bar background and the item background.
			FillRect(drawItem->hDC, &drawItem->rcItem, GetMenuBarBackgroundBrush(darkModeEnabled));
		}

		if (selected)
		{
			if (darkModeEnabled)
			{
				FillRect(drawItem->hDC, &drawItem->rcItem,
					m_darkModeColorProvider->GetHotItemBackgroundBrush());
			}
			else
			{
				DrawThemeBackground(theme.get(), drawItem->hDC, MENU_BARITEM, itemState,
					&drawItem->rcItem, nullptr);
			}
		}

		// As per the documentation for DRAWITEMSTRUCT, this is the menu handle when a menu is being
		// drawn.
		auto menu = reinterpret_cast<HMENU>(drawItem->hwndItem);
		auto text =
			MenuHelper::GetMenuItemString(menu, static_cast<UINT>(drawItem->itemData), true);

		DWORD drawFlags = drawFlagsBase;

		// It appears that Windows passes in the ODS_NOACCEL flag even when the access keys option
		// is turned on in settings. Therefore, if that setting is on, the ODS_NOACCEL flag will be
		// ignored.
		if (!alwaysShowAccessKeys && WI_IsFlagSet(drawItem->itemState, ODS_NOACCEL))
		{
			WI_SetFlag(drawFlags, DT_HIDEPREFIX);
		}

		DTTOPTS options = {};
		options.dwSize = sizeof(options);

		if (darkModeEnabled)
		{
			WI_SetFlag(options.dwFlags, DTT_TEXTCOLOR);
			COLORREF textColor;

			if (itemState == MBI_DISABLED || itemState == MBI_DISABLEDHOT
				|| itemState == MBI_DISABLEDPUSHED)
			{
				textColor = m_darkModeColorProvider->GetDisabledTextColor();
			}
			else
			{
				textColor = m_darkModeColorProvider->GetTextColor();
			}

			options.crText = textColor;
		}

		HRESULT hr = DrawThemeTextEx(theme.get(), drawItem->hDC, MENU_BARITEM, itemState,
			text.c_str(), -1, drawFlags, &drawItem->rcItem, &options);
		DCHECK(SUCCEEDED(hr));

		return TRUE;
	}
	break;

	case WM_THEMECHANGED:
		theme.reset(OpenThemeData(hwnd, L"Menu"));
		break;

	case WM_SETTINGCHANGE:
		if (wParam == SPI_SETKEYBOARDCUES)
		{
			alwaysShowAccessKeys = ShouldAlwaysShowAccessKeys();

			RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME);
		}
		break;

	// A 1px border will be drawn under the menu bar, in a color specified by the current theme. In
	// dark mode, the application will draw with custom colors, that aren't derived from the theme.
	// That then means that the	border can look out of place (e.g. it might be drawn in a light
	// color when the rest of the application is dark). To fix that, the border will be painted over
	// here.
	//
	// This is done in both WM_NCPAINT and WM_NCACTIVATE, since painting over the border in
	// WM_NCPAINT only will cause it to reappear whenever the window loses focus.
	//
	// Also see https://github.com/notepad-plus-plus/notepad-plus-plus/pull/9985.
	case WM_NCPAINT:
	case WM_NCACTIVATE:
	{
		if (!m_darkModeManager->IsDarkModeEnabled())
		{
			break;
		}

		auto defWindowProcResult = DefWindowProc(hwnd, msg, wParam, lParam);

		RECT windowRect;
		auto res = GetWindowRect(hwnd, &windowRect);
		CHECK(res);

		MENUBARINFO barInfo = {};
		barInfo.cbSize = sizeof(barInfo);
		res = GetMenuBarInfo(hwnd, OBJID_MENU, 0, &barInfo);
		CHECK(res);

		// The border is drawn directly underneath the menu bar.
		RECT menuBarBorderRect = { barInfo.rcBar.left, barInfo.rcBar.bottom, barInfo.rcBar.right,
			barInfo.rcBar.bottom + 1 };
		OffsetRect(&menuBarBorderRect, -windowRect.left, -windowRect.top);

		auto hdc = wil::GetWindowDC(hwnd);
		FillRect(hdc.get(), &menuBarBorderRect, m_darkModeColorProvider->GetBackgroundBrush());

		return defWindowProcResult;
	}
	break;
	}

	return DefSubclassProc(hwnd, msg, wParam, lParam);
}

HBRUSH ThemeManager::GetMenuBarBackgroundBrush(bool enableDarkMode)
{
	if (enableDarkMode)
	{
		return m_darkModeColorProvider->GetBackgroundBrush();
	}
	else
	{
		int systemColorIndex;

		if (DarkModeManager::IsHighContrast())
		{
			systemColorIndex = COLOR_BTNFACE;
		}
		else
		{
			systemColorIndex = COLOR_WINDOW;
		}

		return GetSysColorBrush(systemColorIndex);
	}
}

bool ThemeManager::ShouldAlwaysShowAccessKeys()
{
	BOOL alwaysShow;
	BOOL res = SystemParametersInfo(SPI_GETKEYBOARDCUES, 0, &alwaysShow, 0);

	if (!res)
	{
		DCHECK(false);
		return false;
	}

	return alwaysShow;
}

LRESULT ThemeManager::DialogSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CTLCOLORDLG:
	case WM_CTLCOLORSTATIC:
	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORLISTBOX:
	case WM_CTLCOLORBTN:
	{
		auto hdc = reinterpret_cast<HDC>(wParam);
		SetBkColor(hdc, m_darkModeColorProvider->GetBackgroundColor());
		SetTextColor(hdc, m_darkModeColorProvider->GetTextColor());
		return reinterpret_cast<LRESULT>(m_darkModeColorProvider->GetBackgroundBrush());
	}
	break;

	case WM_NOTIFY:
	{
		auto *nmhdr = reinterpret_cast<NMHDR *>(lParam);

		switch (nmhdr->code)
		{
		case NM_CUSTOMDRAW:
			return OnCustomDraw(reinterpret_cast<NMCUSTOMDRAW *>(lParam));
		}
	}
	break;
	}

	return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT ThemeManager::ToolbarParentSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_NOTIFY:
	{
		auto *nmhdr = reinterpret_cast<NMHDR *>(lParam);

		switch (nmhdr->code)
		{
		case NM_CUSTOMDRAW:
			return OnCustomDraw(reinterpret_cast<NMCUSTOMDRAW *>(lParam));
		}
	}
	break;
	}

	return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT ThemeManager::OnCustomDraw(NMCUSTOMDRAW *customDraw)
{
	WCHAR className[256];
	auto res = GetClassName(customDraw->hdr.hwndFrom, className, std::size(className));

	if (res == 0)
	{
		DCHECK(false);
		return CDRF_DODEFAULT;
	}

	if (lstrcmp(className, WC_BUTTON) == 0)
	{
		return OnButtonCustomDraw(customDraw);
	}
	else if (lstrcmp(className, TOOLBARCLASSNAME) == 0)
	{
		return OnToolbarCustomDraw(reinterpret_cast<NMTBCUSTOMDRAW *>(customDraw));
	}

	return CDRF_DODEFAULT;
}

LRESULT ThemeManager::OnButtonCustomDraw(NMCUSTOMDRAW *customDraw)
{
	switch (customDraw->dwDrawStage)
	{
	case CDDS_PREPAINT:
	{
		auto style = GetWindowLongPtr(customDraw->hdr.hwndFrom, GWL_STYLE);

		// The size of the interactive element of the control (i.e. the check box or radio button
		// part).
		SIZE elementSize;

		// Although the documentation
		// (https://learn.microsoft.com/en-au/windows/win32/controls/button-styles#constants) states
		// that BS_TYPEMASK is out of date and shouldn't be used, it's necessary here, for the
		// reasons discussed in https://stackoverflow.com/a/7293345. That is, the type flags from
		// BS_PUSHBUTTON to BS_OWNERDRAW are mutually exclusive and the values in that range can
		// have multiple bits set. So, checking that a single bit is set isn't going to work.
		if ((style & BS_TYPEMASK) == BS_AUTOCHECKBOX)
		{
			elementSize = GetCheckboxSize(customDraw->hdr.hwndFrom);
		}
		else if ((style & BS_TYPEMASK) == BS_AUTORADIOBUTTON)
		{
			elementSize = GetRadioButtonSize(customDraw->hdr.hwndFrom);
		}
		else
		{
			break;
		}

		// Also applies to radio buttons.
		constexpr int CHECKBOX_TEXT_SPACING_96DPI = 3;

		UINT dpi = DpiCompatibility::GetInstance().GetDpiForWindow(customDraw->hdr.hwndFrom);

		RECT textRect = customDraw->rc;
		textRect.left +=
			elementSize.cx + MulDiv(CHECKBOX_TEXT_SPACING_96DPI, dpi, USER_DEFAULT_SCREEN_DPI);

		std::wstring text = GetWindowString(customDraw->hdr.hwndFrom);
		DCHECK(!text.empty());

		COLORREF textColor;

		if (IsWindowEnabled(customDraw->hdr.hwndFrom))
		{
			textColor = m_darkModeColorProvider->GetTextColor();
		}
		else
		{
			textColor = m_darkModeColorProvider->GetDisabledTextColor();
		}

		SetTextColor(customDraw->hdc, textColor);

		UINT textFormat = DT_LEFT;

		if (!WI_IsFlagSet(customDraw->uItemState, CDIS_SHOWKEYBOARDCUES))
		{
			WI_SetFlag(textFormat, DT_HIDEPREFIX);
		}

		RECT finalTextRect = textRect;
		DrawText(customDraw->hdc, text.c_str(), static_cast<int>(text.size()), &finalTextRect,
			textFormat | DT_CALCRECT);

		if (GetRectHeight(&finalTextRect) < GetRectHeight(&textRect))
		{
			textRect.top += (GetRectHeight(&textRect) - GetRectHeight(&finalTextRect)) / 2;
		}

		DrawText(customDraw->hdc, text.c_str(), static_cast<int>(text.size()), &textRect,
			textFormat);

		if (WI_IsFlagSet(customDraw->uItemState, CDIS_FOCUS))
		{
			DrawFocusRect(customDraw->hdc, &textRect);
		}

		// TODO: May also need to handle CDIS_DISABLED and CDIS_GRAYED.
	}
		return CDRF_SKIPDEFAULT;
	}

	return CDRF_DODEFAULT;
}

LRESULT ThemeManager::OnToolbarCustomDraw(NMTBCUSTOMDRAW *customDraw)
{
	switch (customDraw->nmcd.dwDrawStage)
	{
	case CDDS_PREPAINT:
		return CDRF_NOTIFYITEMDRAW;

	case CDDS_ITEMPREPAINT:
		if (WI_IsFlagSet(customDraw->nmcd.uItemState, CDIS_CHECKED))
		{
			// The color used to draw checked items doesn't work very well in dark mode (the color
			// is too bright and somewhat hard to distinguish from the text). Therefore, checked
			// items will be drawn as if they're hot.
			//
			// Additionally, if the button actually is hot, it will still be drawn in the same
			// color. That matches the behavior of the control in the default theme.
			WI_SetFlag(customDraw->nmcd.uItemState, CDIS_HOT);
			customDraw->clrHighlightHotTrack =
				m_darkModeColorProvider->GetToolbarCheckedBackgroundColor();
		}
		else
		{
			customDraw->clrHighlightHotTrack = m_darkModeColorProvider->GetHotItemBackgroundColor();
		}

		customDraw->clrText = m_darkModeColorProvider->GetTextColor();
		return TBCDRF_USECDCOLORS | TBCDRF_HILITEHOTTRACK;
	}

	return CDRF_DODEFAULT;
}

LRESULT ThemeManager::ComboBoxExSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CTLCOLOREDIT:
	{
		auto hdc = reinterpret_cast<HDC>(wParam);
		SetBkMode(hdc, TRANSPARENT);
		SetTextColor(hdc, m_darkModeColorProvider->GetTextColor());
		return reinterpret_cast<LRESULT>(m_darkModeColorProvider->GetComboBoxExBackgroundBrush());
	}
	break;
	}

	return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT ThemeManager::TabControlSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_ERASEBKGND:
		return 1;

	case WM_PAINT:
	{
		DoubleBufferedPaint doubleBufferedPaint(hwnd);
		HDC memDC = doubleBufferedPaint.GetMemoryDC();
		RECT paintRect = doubleBufferedPaint.GetPaintRect();

		ThemedTabControlPainter painter(hwnd, m_darkModeColorProvider);

		if (auto itr = m_hotTabMap.find(hwnd); itr != m_hotTabMap.end())
		{
			painter.SetHotItem(itr->second);
		}

		painter.Paint(memDC, paintRect);
	}
		return 0;

	case WM_MOUSEMOVE:
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

		TCHITTESTINFO hitTestInfo = {};
		hitTestInfo.pt = pt;
		int index = TabCtrl_HitTest(hwnd, &hitTestInfo);

		if (index != -1)
		{
			m_hotTabMap.insert_or_assign(hwnd, index);
		}
		else
		{
			m_hotTabMap.erase(hwnd);
		}
	}
	break;

	case WM_MOUSELEAVE:
		m_hotTabMap.erase(hwnd);
		break;

	case WM_PARENTNOTIFY:
		switch (LOWORD(wParam))
		{
		case WM_CREATE:
			// When the tab control needs to be scrolled to show all tabs, it will create an up-down
			// control. That control should be themed.
			auto child = reinterpret_cast<HWND>(lParam);
			ApplyThemeToWindowAndChildren(child);
			break;
		}
		break;

	case WM_DESTROY:
		m_hotTabMap.erase(hwnd);
		break;
	}

	return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT ThemeManager::ListViewSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_NOTIFY:
		switch (reinterpret_cast<LPNMHDR>(lParam)->code)
		{
		case NM_CUSTOMDRAW:
		{
			auto *customDraw = reinterpret_cast<NMCUSTOMDRAW *>(lParam);

			switch (customDraw->dwDrawStage)
			{
			case CDDS_PREPAINT:
				return CDRF_NOTIFYITEMDRAW;

			case CDDS_ITEMPREPAINT:
				// This sets the text color in the listview header.
				SetTextColor(customDraw->hdc, m_darkModeColorProvider->GetTextColor());
				return CDRF_NEWFONT;
			}
		}
		break;
		}
		break;
	}

	return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT ThemeManager::RebarSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_ERASEBKGND:
	{
		auto hdc = reinterpret_cast<HDC>(wParam);

		RECT rc;
		GetClientRect(hwnd, &rc);
		FillRect(hdc, &rc, m_darkModeColorProvider->GetBackgroundBrush());

		return 1;
	}
	break;
	}

	return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT ThemeManager::GroupBoxSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);

		RECT rect;
		GetClientRect(hwnd, &rect);

		std::wstring text = GetWindowString(hwnd);
		DCHECK(!text.empty());

		SetBkMode(hdc, TRANSPARENT);
		SetTextColor(hdc, m_darkModeColorProvider->GetTextColor());

		auto font = reinterpret_cast<HFONT>(SendMessage(hwnd, WM_GETFONT, 0, 0));
		wil::unique_select_object selectFont;

		if (font)
		{
			selectFont = wil::SelectObject(hdc, font);
		}

		RECT textRect = rect;
		DrawText(hdc, text.c_str(), static_cast<int>(text.size()), &textRect, DT_CALCRECT);

		// The top border of the group box isn't anchored to the top of the control. Rather, it cuts
		// midway through the caption text. That is, the text is anchored to the top of the control
		// and the border starts at the vertical mid-point of the text.
		RECT groupBoxRect = rect;
		groupBoxRect.top += GetRectHeight(&textRect) / 2;

		wil::unique_htheme theme(OpenThemeData(nullptr, L"BUTTON"));

		// The group box border isn't shown behind the caption; instead, the text appears as if its
		// drawn directly on top of the parent.
		DrawThemeBackground(theme.get(), hdc, BP_GROUPBOX, GBS_NORMAL, &groupBoxRect, &ps.rcPaint);

		// When the system draws the group box, it appears appears that the constant below (8) is
		// embedded within the code, rather than being retrieved dynamically.
		int xBorder = GetSystemMetrics(SM_CXBORDER);
		int xEdge = GetSystemMetrics(SM_CXEDGE);
		OffsetRect(&textRect, 8 - xBorder + xEdge, 0);

		RECT textBackgroundRect = textRect;
		InflateRect(&textBackgroundRect, xEdge, 0);
		DrawThemeParentBackground(hwnd, hdc, &textBackgroundRect);

		DrawText(hdc, text.c_str(), static_cast<int>(text.size()), &textRect, DT_LEFT);

		EndPaint(hwnd, &ps);
	}
		return 0;
	}

	return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT ThemeManager::ScrollBarSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);

		int partId = SBP_SIZEBOX;
		int stateId;

		auto style = GetWindowLongPtr(hwnd, GWL_STYLE);

		if (WI_IsFlagSet(style, SBS_SIZEBOXTOPLEFTALIGN))
		{
			stateId = SZB_TOPLEFTALIGN;
		}
		else
		{
			stateId = SZB_RIGHTALIGN;
		}

		RECT rect;
		GetClientRect(hwnd, &rect);

		wil::unique_htheme theme(OpenThemeData(hwnd, L"SCROLLBAR"));

		if (IsThemeBackgroundPartiallyTransparent(theme.get(), partId, stateId))
		{
			DrawThemeParentBackground(hwnd, hdc, &ps.rcPaint);
		}

		DrawThemeBackground(theme.get(), hdc, partId, stateId, &rect, &ps.rcPaint);

		EndPaint(hwnd, &ps);
	}
		return 0;
	}

	return DefSubclassProc(hwnd, msg, wParam, lParam);
}

// Shell dialog dark mode support.
// Property sheets and other shell-created dialogs run on their own threads. We use a WinEvent hook
// to detect when these dialogs are shown and apply dark mode theming. With WINEVENT_INCONTEXT, the
// callback runs on the dialog's own thread, making SetWindowSubclass safe to call.

void ThemeManager::SetupShellDialogDarkModeHook()
{
	if (m_shellDialogHook)
	{
		return;
	}

	// Try in-context first: callback runs on the dialog's thread (same-thread subclassing is safe).
	m_shellDialogHook = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW,
		GetModuleHandle(nullptr), ShellDialogWinEventProc, GetCurrentProcessId(), 0,
		WINEVENT_INCONTEXT);

	if (m_shellDialogHook)
	{
		s_hookInContext = true;
		return;
	}

	// Fallback: out-of-context (callback on main thread, limited theming - no subclassing).
	m_shellDialogHook = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW, nullptr,
		ShellDialogWinEventProc, GetCurrentProcessId(), 0, WINEVENT_OUTOFCONTEXT);
	s_hookInContext = false;
}

void ThemeManager::TeardownShellDialogDarkModeHook()
{
	if (m_shellDialogHook)
	{
		UnhookWinEvent(m_shellDialogHook);
		m_shellDialogHook = nullptr;
	}
}

void CALLBACK ThemeManager::ShellDialogWinEventProc([[maybe_unused]] HWINEVENTHOOK hWinEventHook,
	[[maybe_unused]] DWORD event, HWND hwnd, LONG idObject, LONG idChild,
	DWORD idEventThread, [[maybe_unused]] DWORD dwmsEventTime)
{
	if (idObject != OBJID_WINDOW || idChild != CHILDID_SELF)
	{
		return;
	}

	if (!IsWindow(hwnd))
	{
		return;
	}

	if (!s_shellDarkModeActive.load(std::memory_order_relaxed))
	{
		return;
	}

	// Skip dialogs on the main thread (themed by normal ThemeManager), unless they are owned by
	// a shell dialog we have themed (e.g. "Remove Properties" sub-dialog from property sheet).
	if (idEventThread == s_mainThreadId)
	{
		HWND owner = GetWindow(hwnd, GW_OWNER);

		if (!owner || !GetProp(owner, L"ExplorerPPDarkThemed"))
		{
			return;
		}
	}

	WCHAR className[16];

	if (GetClassName(hwnd, className, static_cast<int>(std::size(className))) == 0)
	{
		return;
	}

	if (lstrcmp(className, DIALOG_CLASS_NAME) != 0)
	{
		return;
	}

	// Skip already-themed windows.
	if (GetProp(hwnd, L"ExplorerPPDarkThemed"))
	{
		return;
	}

	ApplyDarkModeToShellDialog(hwnd);
}

void ThemeManager::ApplyDarkModeToShellDialog(HWND hwnd)
{
	SetProp(hwnd, L"ExplorerPPDarkThemed", reinterpret_cast<HANDLE>(1));

	// Dark title bar.
	BOOL dark = TRUE;
	DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

	if (s_AllowDarkModeForWindow)
	{
		s_AllowDarkModeForWindow(hwnd, true);
	}

	// Detect file dialogs (Open/Save/Browse) by checking for the shell folder view control. These
	// are typically themed by external tools like Windhawk, so we should not theme their children.
	bool isFileDialog = false;

	EnumChildWindows(
		hwnd,
		[](HWND child, LPARAM lParam) -> BOOL
		{
			WCHAR cls[64];

			if (GetClassName(child, cls, static_cast<int>(std::size(cls))) != 0
				&& lstrcmp(cls, L"SHELLDLL_DefView") == 0)
			{
				*reinterpret_cast<bool *>(lParam) = true;
				return FALSE;
			}

			return TRUE;
		},
		reinterpret_cast<LPARAM>(&isFileDialog));

	if (isFileDialog)
	{
		return;
	}

	// Install a subclass for WM_CTLCOLOR handling (only safe when running in-context on the
	// dialog's own thread).
	if (s_hookInContext)
	{
		SetWindowSubclass(hwnd, ShellDialogSubclassProc, SHELL_DIALOG_SUBCLASS_ID, 0);
	}

	EnumChildWindows(hwnd, ThemeShellDialogChild, 0);
}

BOOL CALLBACK ThemeManager::ThemeShellDialogChild(HWND hwnd, [[maybe_unused]] LPARAM lParam)
{
	// Skip windows already themed to avoid redundant SetWindowTheme repaints.
	if (GetProp(hwnd, L"ExplorerPPDarkThemed"))
	{
		return TRUE;
	}

	if (s_AllowDarkModeForWindow)
	{
		s_AllowDarkModeForWindow(hwnd, true);
	}

	WCHAR className[256];

	if (GetClassName(hwnd, className, static_cast<int>(std::size(className))) == 0)
	{
		return TRUE;
	}

	SetProp(hwnd, L"ExplorerPPDarkThemed", reinterpret_cast<HANDLE>(1));

	if (lstrcmp(className, WC_TABCONTROL) == 0)
	{
		SetWindowTheme(hwnd, L"", L"");

		if (s_hookInContext)
		{
			SetWindowSubclass(hwnd, ShellTabControlSubclassProc, SHELL_TAB_SUBCLASS_ID, 0);
		}
	}
	else if (lstrcmp(className, WC_LISTVIEW) == 0)
	{
		SetWindowTheme(hwnd, L"ItemsView", nullptr);
		ListView_SetBkColor(hwnd, SHELL_DLG_BG_COLOR);
		ListView_SetTextBkColor(hwnd, SHELL_DLG_BG_COLOR);
		ListView_SetTextColor(hwnd, SHELL_DLG_TEXT_COLOR);

		if (s_hookInContext)
		{
			SetWindowSubclass(hwnd, ShellListViewSubclassProc, SHELL_LISTVIEW_SUBCLASS_ID, 0);
		}
	}
	else if (lstrcmp(className, WC_TREEVIEW) == 0)
	{
		SetWindowTheme(hwnd, L"Explorer", nullptr);
		TreeView_SetBkColor(hwnd, SHELL_DLG_BG_COLOR);
		TreeView_SetTextColor(hwnd, SHELL_DLG_TEXT_COLOR);
	}
	else if (lstrcmp(className, WC_BUTTON) == 0)
	{
		auto style = GetWindowLongPtr(hwnd, GWL_STYLE);
		auto type = style & BS_TYPEMASK;

		if (type == BS_GROUPBOX)
		{
			if (s_hookInContext)
			{
				SetWindowSubclass(hwnd, ShellGroupBoxSubclassProc, SHELL_GROUPBOX_SUBCLASS_ID, 0);
			}
		}
		else
		{
			SetWindowTheme(hwnd, L"Explorer", nullptr);
		}
	}
	else if (lstrcmp(className, WC_EDIT) == 0)
	{
		SetWindowTheme(hwnd, L"CFD", nullptr);
	}
	else if (lstrcmp(className, PROGRESS_CLASS) == 0)
	{
		SetWindowTheme(hwnd, L"Explorer", nullptr);
	}
	else if (lstrcmp(className, WC_SCROLLBAR) == 0)
	{
		SetWindowTheme(hwnd, L"Explorer", nullptr);
	}
	else if (lstrcmp(className, STATUSCLASSNAME) == 0)
	{
		SetWindowTheme(hwnd, nullptr, L"ExplorerStatusBar");
	}
	else if (lstrcmp(className, WC_HEADER) == 0)
	{
		SetWindowTheme(hwnd, L"ItemsView", nullptr);
	}
	else if (lstrcmp(className, UPDOWN_CLASS) == 0)
	{
		SetWindowTheme(hwnd, L"Explorer", nullptr);
	}
	else if (lstrcmp(className, WC_COMBOBOX) == 0)
	{
		SetWindowTheme(hwnd, L"CFD", nullptr);
	}
	else if (lstrcmp(className, WC_LINK) == 0)
	{
		// Set each link item to a lighter color for dark background readability.
		LITEM item = {};
		item.mask = LIF_ITEMINDEX | LIF_STATE;
		item.stateMask = LIS_DEFAULTCOLORS;

		for (int i = 0; SendMessage(hwnd, LM_GETITEM, 0, reinterpret_cast<LPARAM>(&item)); i++)
		{
			item.state &= ~LIS_DEFAULTCOLORS;
			SendMessage(hwnd, LM_SETITEM, 0, reinterpret_cast<LPARAM>(&item));
			item.iLink = i + 1;
		}

		if (s_hookInContext)
		{
			SetWindowSubclass(hwnd, ShellSysLinkSubclassProc, SHELL_SYSLINK_SUBCLASS_ID, 0);
		}
	}
	else if (lstrcmp(className, DIALOG_CLASS_NAME) == 0)
	{
		// Child dialog (e.g. property sheet tab pages). ApplyDarkModeToShellDialog will subclass
		// it and theme its children. The top-level GetProp guard already prevents re-entry.
		ApplyDarkModeToShellDialog(hwnd);
	}

	return TRUE;
}

LRESULT ThemeManager::OnShellButtonCustomDraw(NMCUSTOMDRAW *customDraw)
{
	switch (customDraw->dwDrawStage)
	{
	case CDDS_PREPAINT:
	{
		auto style = GetWindowLongPtr(customDraw->hdr.hwndFrom, GWL_STYLE);

		SIZE elementSize;

		switch (style & BS_TYPEMASK)
		{
		case BS_AUTOCHECKBOX:
		case BS_CHECKBOX:
		case BS_AUTO3STATE:
		case BS_3STATE:
			elementSize = GetCheckboxSize(customDraw->hdr.hwndFrom);
			break;

		case BS_AUTORADIOBUTTON:
		case BS_RADIOBUTTON:
			elementSize = GetRadioButtonSize(customDraw->hdr.hwndFrom);
			break;

		default:
			return CDRF_DODEFAULT;
		}

		constexpr int CHECKBOX_TEXT_SPACING_96DPI = 3;
		UINT dpi = DpiCompatibility::GetInstance().GetDpiForWindow(customDraw->hdr.hwndFrom);

		RECT textRect = customDraw->rc;
		textRect.left +=
			elementSize.cx + MulDiv(CHECKBOX_TEXT_SPACING_96DPI, dpi, USER_DEFAULT_SCREEN_DPI);

		std::wstring text = GetWindowString(customDraw->hdr.hwndFrom);

		if (text.empty())
		{
			return CDRF_DODEFAULT;
		}

		COLORREF textColor = IsWindowEnabled(customDraw->hdr.hwndFrom)
			? SHELL_DLG_TEXT_COLOR
			: SHELL_DLG_DISABLED_TEXT_COLOR;
		SetTextColor(customDraw->hdc, textColor);
		SetBkMode(customDraw->hdc, TRANSPARENT);

		UINT textFormat = DT_LEFT;

		if (!WI_IsFlagSet(customDraw->uItemState, CDIS_SHOWKEYBOARDCUES))
		{
			WI_SetFlag(textFormat, DT_HIDEPREFIX);
		}

		RECT finalTextRect = textRect;
		DrawText(customDraw->hdc, text.c_str(), static_cast<int>(text.size()), &finalTextRect,
			textFormat | DT_CALCRECT);

		if (GetRectHeight(&finalTextRect) < GetRectHeight(&textRect))
		{
			textRect.top += (GetRectHeight(&textRect) - GetRectHeight(&finalTextRect)) / 2;
		}

		DrawText(customDraw->hdc, text.c_str(), static_cast<int>(text.size()), &textRect,
			textFormat);

		if (WI_IsFlagSet(customDraw->uItemState, CDIS_FOCUS))
		{
			DrawFocusRect(customDraw->hdc, &textRect);
		}

		return CDRF_SKIPDEFAULT;
	}
	}

	return CDRF_DODEFAULT;
}

LRESULT ThemeManager::OnShellListViewCustomDraw(NMLVCUSTOMDRAW *customDraw)
{
	COLORREF textColor = IsWindowEnabled(customDraw->nmcd.hdr.hwndFrom)
		? SHELL_DLG_TEXT_COLOR
		: SHELL_DLG_DISABLED_TEXT_COLOR;

	switch (customDraw->nmcd.dwDrawStage)
	{
	case CDDS_PREPAINT:
		return CDRF_NOTIFYITEMDRAW;

	case CDDS_ITEMPREPAINT:
		customDraw->clrText = textColor;
		customDraw->clrTextBk = SHELL_DLG_BG_COLOR;
		return CDRF_NOTIFYSUBITEMDRAW;

	case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
		customDraw->clrText = textColor;
		customDraw->clrTextBk = SHELL_DLG_BG_COLOR;
		return CDRF_DODEFAULT;
	}

	return CDRF_DODEFAULT;
}

LRESULT CALLBACK ThemeManager::ShellDialogSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
	LPARAM lParam, UINT_PTR subclassId, [[maybe_unused]] DWORD_PTR refData)
{
	if (!s_shellDarkModeActive.load(std::memory_order_relaxed))
	{
		return DefSubclassProc(hwnd, msg, wParam, lParam);
	}

	switch (msg)
	{
	case WM_CTLCOLORDLG:
	case WM_CTLCOLORSTATIC:
	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORLISTBOX:
	case WM_CTLCOLORBTN:
	{
		auto hdc = reinterpret_cast<HDC>(wParam);
		SetBkColor(hdc, SHELL_DLG_BG_COLOR);
		SetTextColor(hdc, SHELL_DLG_TEXT_COLOR);
		return reinterpret_cast<LRESULT>(s_shellDlgBgBrush);
	}

	case WM_NOTIFY:
	{
		auto *nmhdr = reinterpret_cast<NMHDR *>(lParam);

		if (nmhdr->code == TCN_SELCHANGE)
		{
			// Let the property sheet handle the tab change first (creates/shows the page).
			LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);

			// Theme any newly created child windows (lazy tab pages).
			EnumChildWindows(hwnd, ThemeShellDialogChild, 0);

			return result;
		}

		if (nmhdr->code == NM_CUSTOMDRAW)
		{
			auto *customDraw = reinterpret_cast<NMCUSTOMDRAW *>(lParam);

			WCHAR childClassName[256];

			if (GetClassName(customDraw->hdr.hwndFrom, childClassName,
					static_cast<int>(std::size(childClassName)))
				!= 0)
			{
				if (lstrcmp(childClassName, WC_BUTTON) == 0)
				{
					return OnShellButtonCustomDraw(customDraw);
				}
				else if (lstrcmp(childClassName, WC_LISTVIEW) == 0)
				{
					return OnShellListViewCustomDraw(
						reinterpret_cast<NMLVCUSTOMDRAW *>(customDraw));
				}
			}
		}
	}
	break;

	case WM_NCDESTROY:
		RemoveProp(hwnd, L"ExplorerPPDarkThemed");
		RemoveWindowSubclass(hwnd, ShellDialogSubclassProc, subclassId);
		break;
	}

	return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK ThemeManager::ShellTabControlSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
	LPARAM lParam, UINT_PTR subclassId, [[maybe_unused]] DWORD_PTR refData)
{
	if (!s_shellDarkModeActive.load(std::memory_order_relaxed))
	{
		return DefSubclassProc(hwnd, msg, wParam, lParam);
	}

	switch (msg)
	{
	case WM_ERASEBKGND:
		return 1;

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);
		PaintShellTabControl(hwnd, hdc, ps.rcPaint);
		EndPaint(hwnd, &ps);
	}
		return 0;

	case WM_NCDESTROY:
		RemoveWindowSubclass(hwnd, ShellTabControlSubclassProc, subclassId);
		break;
	}

	return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void ThemeManager::PaintShellTabControl(HWND hwnd, HDC hdc, const RECT &paintRect)
{
	RECT clientRect;
	GetClientRect(hwnd, &clientRect);
	FillRect(hdc, &clientRect, s_shellDlgBgBrush);

	int numTabs = TabCtrl_GetItemCount(hwnd);

	if (numTabs == 0)
	{
		return;
	}

	int selectedTab = TabCtrl_GetCurSel(hwnd);

	// Draw bottom border under the tab row.
	RECT firstTabRect;
	TabCtrl_GetItemRect(hwnd, 0, &firstTabRect);
	RECT bottomEdge = { clientRect.left, firstTabRect.bottom, clientRect.right,
		firstTabRect.bottom + 1 };
	FillRect(hdc, &bottomEdge, s_shellBorderBrush);

	SetBkMode(hdc, TRANSPARENT);

	auto font = reinterpret_cast<HFONT>(SendMessage(hwnd, WM_GETFONT, 0, 0));
	HGDIOBJ oldFont = font ? SelectObject(hdc, font) : nullptr;

	for (int i = 0; i < numTabs; i++)
	{
		RECT tabRect;
		TabCtrl_GetItemRect(hwnd, i, &tabRect);

		RECT testRect;

		if (!IntersectRect(&testRect, &paintRect, &tabRect))
		{
			continue;
		}

		bool isSelected = (i == selectedTab);

		if (isSelected)
		{
			tabRect.top = 0;
		}

		RECT bgRect = tabRect;

		if (isSelected)
		{
			// Cover the bottom border for the selected tab.
			bgRect.bottom += 1;
		}

		FillRect(hdc, &bgRect, isSelected ? s_shellDlgBgBrush : s_shellTabBgBrush);

		// Left border (only for first tab).
		if (i == 0)
		{
			RECT leftBorder = { tabRect.left, tabRect.top, tabRect.left + 1, tabRect.bottom };
			FillRect(hdc, &leftBorder, s_shellBorderBrush);
		}

		// Right border.
		int rightBorderTop = tabRect.top;

		if (i == selectedTab - 1)
		{
			rightBorderTop = 0;
		}

		RECT rightBorder = { tabRect.right - 1, rightBorderTop, tabRect.right, tabRect.bottom };
		FillRect(hdc, &rightBorder, s_shellBorderBrush);

		// Top border.
		RECT topBorder = { tabRect.left, tabRect.top, tabRect.right, tabRect.top + 1 };
		FillRect(hdc, &topBorder, s_shellBorderBrush);

		// Draw tab text.
		WCHAR text[256] = {};
		TCITEM tci = {};
		tci.mask = TCIF_TEXT;
		tci.pszText = text;
		tci.cchTextMax = static_cast<int>(std::size(text));
		TabCtrl_GetItem(hwnd, i, &tci);

		SetTextColor(hdc,
			isSelected ? SHELL_DLG_TEXT_COLOR : SHELL_DLG_BACKGROUND_TEXT_COLOR);
		DrawText(hdc, text, -1, &tabRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	}

	if (oldFont)
	{
		SelectObject(hdc, oldFont);
	}
}

LRESULT CALLBACK ThemeManager::ShellGroupBoxSubclassProc(HWND hwnd, UINT msg,
	WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, [[maybe_unused]] DWORD_PTR refData)
{
	if (!s_shellDarkModeActive.load(std::memory_order_relaxed))
	{
		return DefSubclassProc(hwnd, msg, wParam, lParam);
	}

	switch (msg)
	{
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);

		RECT rect;
		GetClientRect(hwnd, &rect);

		int textLen = GetWindowTextLength(hwnd);
		std::wstring text(static_cast<size_t>(textLen) + 1, L'\0');
		GetWindowText(hwnd, text.data(), static_cast<int>(text.size()));
		text.resize(static_cast<size_t>(textLen));

		SetBkMode(hdc, TRANSPARENT);
		SetTextColor(hdc, SHELL_DLG_TEXT_COLOR);

		auto font = reinterpret_cast<HFONT>(SendMessage(hwnd, WM_GETFONT, 0, 0));
		HGDIOBJ oldFont = font ? SelectObject(hdc, font) : nullptr;

		RECT textRect = rect;
		DrawText(hdc, text.c_str(), static_cast<int>(text.size()), &textRect, DT_CALCRECT);

		RECT groupBoxRect = rect;
		groupBoxRect.top += (textRect.bottom - textRect.top) / 2;

		wil::unique_htheme theme(OpenThemeData(nullptr, L"BUTTON"));
		DrawThemeBackground(theme.get(), hdc, BP_GROUPBOX, GBS_NORMAL, &groupBoxRect, &ps.rcPaint);

		int xBorder = GetSystemMetrics(SM_CXBORDER);
		int xEdge = GetSystemMetrics(SM_CXEDGE);
		OffsetRect(&textRect, 8 - xBorder + xEdge, 0);

		RECT textBackgroundRect = textRect;
		InflateRect(&textBackgroundRect, xEdge, 0);
		DrawThemeParentBackground(hwnd, hdc, &textBackgroundRect);

		DrawText(hdc, text.c_str(), static_cast<int>(text.size()), &textRect, DT_LEFT);

		if (oldFont)
		{
			SelectObject(hdc, oldFont);
		}

		EndPaint(hwnd, &ps);
	}
		return 0;

	case WM_NCDESTROY:
		RemoveWindowSubclass(hwnd, ShellGroupBoxSubclassProc, subclassId);
		break;
	}

	return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK ThemeManager::ShellListViewSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
	LPARAM lParam, UINT_PTR subclassId, [[maybe_unused]] DWORD_PTR refData)
{
	if (!s_shellDarkModeActive.load(std::memory_order_relaxed))
	{
		return DefSubclassProc(hwnd, msg, wParam, lParam);
	}

	switch (msg)
	{
	// Prevent the dialog from resetting ListView colors (e.g. when toggling radio buttons in the
	// "Remove Properties" dialog).
	case LVM_SETBKCOLOR:
	case LVM_SETTEXTBKCOLOR:
		lParam = static_cast<LPARAM>(SHELL_DLG_BG_COLOR);
		break;

	case LVM_SETTEXTCOLOR:
		lParam = static_cast<LPARAM>(SHELL_DLG_TEXT_COLOR);
		break;

	case WM_ENABLE:
	case WM_THEMECHANGED:
	{
		LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
		SetWindowTheme(hwnd, L"ItemsView", nullptr);
		ListView_SetBkColor(hwnd, SHELL_DLG_BG_COLOR);
		ListView_SetTextBkColor(hwnd, SHELL_DLG_BG_COLOR);
		ListView_SetTextColor(hwnd, SHELL_DLG_TEXT_COLOR);
		InvalidateRect(hwnd, nullptr, TRUE);

		HWND header = ListView_GetHeader(hwnd);

		if (header)
		{
			InvalidateRect(header, nullptr, TRUE);
		}

		return result;
	}

	case WM_ERASEBKGND:
	{
		auto hdc = reinterpret_cast<HDC>(wParam);

		RECT rc;
		GetClientRect(hwnd, &rc);
		FillRect(hdc, &rc, s_shellDlgBgBrush);
		return 1;
	}

	case WM_NOTIFY:
	{
		auto *nmhdr = reinterpret_cast<LPNMHDR>(lParam);

		// Only handle NM_CUSTOMDRAW from the header control, not from the ListView itself.
		if (nmhdr->code == NM_CUSTOMDRAW
			&& nmhdr->hwndFrom == ListView_GetHeader(hwnd))
		{
			auto *customDraw = reinterpret_cast<NMCUSTOMDRAW *>(lParam);

			switch (customDraw->dwDrawStage)
			{
			case CDDS_PREPAINT:
				return CDRF_NOTIFYITEMDRAW;

			case CDDS_ITEMPREPAINT:
				SetTextColor(customDraw->hdc, SHELL_DLG_TEXT_COLOR);
				return CDRF_NEWFONT;
			}
		}
	}
	break;

	case WM_NCDESTROY:
		RemoveWindowSubclass(hwnd, ShellListViewSubclassProc, subclassId);
		break;
	}

	return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK ThemeManager::ShellSysLinkSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
	LPARAM lParam, UINT_PTR subclassId, [[maybe_unused]] DWORD_PTR refData)
{
	switch (msg)
	{
	case WM_NCDESTROY:
		RemoveWindowSubclass(hwnd, ShellSysLinkSubclassProc, subclassId);
		break;
	}

	return DefSubclassProc(hwnd, msg, wParam, lParam);
}
