// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "ShellBrowserImpl.h"
#include "../AcceleratorHelper.h"
#include "../BackgroundContextMenuDelegate.h"
#include "../ContextMenuAction.h"
#include "../ContextMenuActionModel.h"
#include "../DirectoryOperationsHelper.h"
#include "../DuplicateFilenameHelper.h"
#include "../FileIconModel.h"
#include "../FolderView.h"
#include "../Icon.h"
#include "../MainResource.h"
#include "../NavigationHelper.h"
#include "../NewMenuClient.h"
#include "../OpenItemsContextMenuDelegate.h"
#include "../PopupMenuView.h"
#include "../ResourceIconModel.h"
#include "../ResourceLoader.h"
#include "../ServiceProvider.h"
#include "../ShellIconModel.h"
#include "../ShellView.h"
#include "../SortMenuBuilder.h"
#include "../SortModeMenuMappings.h"
#include "../ViewModeHelper.h"
#include "../ViewsMenuBuilder.h"
#include "../App.h"
#include "../BrowserWindow.h"
#include "LegacyContextMenuPreloader.h"
#include "ShellBrowserContextMenuDelegate.h"
#include "ShellNavigationController.h"
#include "../../Helper/DpiCompatibility.h"
#include "../../Helper/FileOperations.h"
#include "../../Helper/MenuHelper.h"
#include "../../Helper/ShellBackgroundContextMenu.h"
#include "../../Helper/ShellItemContextMenu.h"
#include "../../Helper/ShellHelper.h"
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

namespace
{

constexpr UINT IDM_QUICK_CONTEXT_MENU_OPEN = 61000;
constexpr UINT IDM_QUICK_CONTEXT_MENU_OPEN_IN_NEW_TAB = 61001;
constexpr UINT IDM_QUICK_CONTEXT_MENU_VIEW = 61002;
constexpr UINT IDM_QUICK_CONTEXT_MENU_SORT_BY = 61003;
constexpr UINT IDM_QUICK_CONTEXT_MENU_GROUP_BY = 61004;
constexpr UINT IDM_QUICK_CONTEXT_MENU_MORE_ACTIONS = 61005;
constexpr UINT IDM_QUICK_CONTEXT_MENU_DUPLICATE = 61006;
constexpr UINT IDM_QUICK_CONTEXT_MENU_CUSTOM_ACTION_START = 61100;

std::optional<std::wstring> MaybeGetAcceleratorText(const AcceleratorManager *acceleratorManager,
	UINT command)
{
	auto accelerator = acceleratorManager->GetAcceleratorForCommand(static_cast<WORD>(command));

	if (!accelerator)
	{
		return std::nullopt;
	}

	return BuildAcceleratorString(*accelerator);
}

std::unique_ptr<const IconModel> MaybeBuildIcon(const ResourceLoader *resourceLoader,
	std::optional<Icon> icon)
{
	if (!icon)
	{
		return {};
	}

	int size = DpiCompatibility::GetInstance().GetSystemMetricsForDpi(SM_CXSMICON,
		USER_DEFAULT_SCREEN_DPI);
	return std::make_unique<ResourceIconModel>(*icon, size, resourceLoader);
}

std::unique_ptr<const IconModel> MaybeBuildCustomActionIcon(
	const ContextMenuActions::ContextMenuAction &contextMenuAction)
{
	if (contextMenuAction.GetIconPath().empty())
	{
		return {};
	}

	int size = DpiCompatibility::GetInstance().GetSystemMetricsForDpi(SM_CXSMICON,
		USER_DEFAULT_SCREEN_DPI);
	return std::make_unique<FileIconModel>(contextMenuAction.GetIconPath(), size);
}

void AppendMenuItem(PopupMenuView *menuView, const AcceleratorManager *acceleratorManager,
	const ResourceLoader *resourceLoader, UINT command, const std::wstring &text,
	std::optional<Icon> icon = std::nullopt, const std::wstring &helpText = L"",
	std::unique_ptr<const IconModel> iconModel = {})
{
	auto finalHelpText = helpText;

	if (finalHelpText.empty())
	{
		auto maybeHelpText = resourceLoader->MaybeLoadString(command);

		if (maybeHelpText)
		{
			finalHelpText = *maybeHelpText;
		}
	}

	if (!iconModel)
	{
		iconModel = MaybeBuildIcon(resourceLoader, icon);
	}

	menuView->AppendItem(command, text, std::move(iconModel), finalHelpText,
		MaybeGetAcceleratorText(acceleratorManager, command));
}

std::unique_ptr<const IconModel> MaybeBuildOpenItemIcon(ShellIconLoader *shellIconLoader,
	const Config *config, const std::vector<PidlAbsolute> &selectedItems)
{
	if (!config->showQuickContextMenuOpenItemIcon || !shellIconLoader || selectedItems.size() != 1)
	{
		return {};
	}

	return std::make_unique<ShellIconModel>(shellIconLoader, selectedItems.front().Raw());
}

UINT ShowCommandMenu(HMENU menu, HWND owner, const POINT &pt)
{
	return TrackPopupMenu(menu,
		TPM_LEFTALIGN | TPM_VERTICAL | TPM_RECURSE | TPM_RETURNCMD, pt.x, pt.y, 0, owner, nullptr);
}

HRESULT DuplicateItemsInDirectory(HWND owner, const std::vector<PidlAbsolute> &selectedItems,
	const std::vector<std::wstring> &selectedItemPaths, const std::wstring &directoryPath)
{
	wil::com_ptr_nothrow<IShellItem> destinationFolder;
	HRESULT hr = SHCreateItemFromParsingName(directoryPath.c_str(), nullptr,
		IID_PPV_ARGS(&destinationFolder));

	if (FAILED(hr))
	{
		return hr;
	}

	wil::com_ptr_nothrow<IFileOperation> fileOperation;
	hr = CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&fileOperation));

	if (FAILED(hr))
	{
		return hr;
	}

	hr = fileOperation->SetOwnerWindow(owner);

	if (FAILED(hr))
	{
		return hr;
	}

	hr = fileOperation->SetOperationFlags(::FileOperations::GetTransferOperationFlags());

	if (FAILED(hr))
	{
		return hr;
	}

	std::unordered_set<std::wstring> reservedNames;

	for (size_t i = 0; i < selectedItems.size(); i++)
	{
		auto sourceName = std::filesystem::path(selectedItemPaths[i]).filename().wstring();
		auto duplicateName = FindNextAvailableDuplicateName(sourceName, directoryPath, reservedNames);

		wil::com_ptr_nothrow<IShellItem> sourceItem;
		hr = SHCreateItemFromIDList(selectedItems[i].Raw(), IID_PPV_ARGS(&sourceItem));

		if (FAILED(hr))
		{
			return hr;
		}

		hr = fileOperation->CopyItem(sourceItem.get(), destinationFolder.get(), duplicateName.c_str(),
			nullptr);

		if (FAILED(hr))
		{
			return hr;
		}
	}

	return fileOperation->PerformOperations();
}

ContextMenuActions::InvocationContext BuildItemInvocationContext(const std::wstring &directoryPath,
	const std::vector<std::wstring> &selectedItemPaths, int numSelectedFiles, int numSelectedFolders)
{
	ContextMenuActions::InvocationContext context;
	context.menuType = ContextMenuActions::MenuType::Item;
	context.directory = directoryPath;
	context.selectedItemPaths = selectedItemPaths;
	context.numSelectedFiles = numSelectedFiles;
	context.numSelectedFolders = numSelectedFolders;
	return context;
}

ContextMenuActions::InvocationContext BuildBackgroundInvocationContext(
	const std::wstring &directoryPath)
{
	ContextMenuActions::InvocationContext context;
	context.menuType = ContextMenuActions::MenuType::Background;
	context.directory = directoryPath;
	return context;
}

UINT BuildItemContextMenuFlags(ShellItemContextMenu::Flags flags)
{
	UINT contextMenuFlags = CMF_NORMAL;

	if (WI_IsFlagSet(flags, ShellItemContextMenu::Flags::ExtendedVerbs))
	{
		contextMenuFlags |= CMF_EXTENDEDVERBS;
	}

	if (WI_IsFlagSet(flags, ShellItemContextMenu::Flags::Rename))
	{
		contextMenuFlags |= CMF_CANRENAME;
	}

	return contextMenuFlags;
}

UINT BuildBackgroundContextMenuFlags(ShellBackgroundContextMenu::Flags flags)
{
	UINT contextMenuFlags = CMF_NODEFAULT;

	if (WI_IsFlagSet(flags, ShellBackgroundContextMenu::Flags::ExtendedVerbs))
	{
		contextMenuFlags |= CMF_EXTENDEDVERBS;
	}

	return contextMenuFlags;
}

winrt::com_ptr<ServiceProvider> BuildBackgroundContextMenuSite(ShellBrowserImpl *shellBrowser,
	App *app, BrowserWindow *browser, PCIDLIST_ABSOLUTE directory)
{
	auto serviceProvider = winrt::make_self<ServiceProvider>();
	serviceProvider->RegisterService(IID_INewMenuClient, winrt::make<NewMenuClient>(shellBrowser));
	serviceProvider->RegisterService(IID_IFolderView,
		winrt::make<FolderView>(shellBrowser->GetWeakPtr()));
	serviceProvider->RegisterService(SID_DefView,
		winrt::make<ShellView>(shellBrowser->GetWeakPtr(), app->GetBrowserList(), browser->GetId(),
			directory, false));
	return serviceProvider;
}

}

void ShellBrowserImpl::ShowBackgroundContextMenu(const POINT &pt)
{
	if (!m_config->showQuickContextMenus)
	{
		ShowLegacyBackgroundContextMenu(pt);
		return;
	}

	ShowQuickBackgroundContextMenu(pt);
}

void ShellBrowserImpl::ShowItemContextMenu(const POINT &pt)
{
	auto selectedItems = GetSelectedItemPidls();

	if (selectedItems.empty())
	{
		return;
	}

	if (!m_config->showQuickContextMenus)
	{
		ShowLegacyItemContextMenu(pt, selectedItems);
		return;
	}

	ShowQuickItemContextMenu(pt, selectedItems);
}

void ShellBrowserImpl::ShowLegacyBackgroundContextMenu(const POINT &pt)
{
	m_legacyContextMenuPreloader.reset();

	ShellBackgroundContextMenu contextMenu(m_directoryState.pidlDirectory.Raw(), m_browser);

	BackgroundContextMenuDelegate backgroundDelegate(m_browser,
		m_app->GetPlatformContext()->GetClipboardStore(), m_app->GetResourceLoader());
	contextMenu.AddDelegate(&backgroundDelegate);

	auto serviceProvider = winrt::make_self<ServiceProvider>();
	serviceProvider->RegisterService(IID_INewMenuClient, winrt::make<NewMenuClient>(this));
	serviceProvider->RegisterService(IID_IFolderView,
		winrt::make<FolderView>(m_weakPtrFactory.GetWeakPtr()));
	serviceProvider->RegisterService(SID_DefView,
		winrt::make<ShellView>(m_weakPtrFactory.GetWeakPtr(), m_app->GetBrowserList(),
			m_browser->GetId(), m_directoryState.pidlDirectory.Raw(), false));

	ShellBackgroundContextMenu::Flags flags = ShellBackgroundContextMenu::Flags::None;

	if (IsKeyDown(VK_SHIFT))
	{
		WI_SetFlag(flags, ShellBackgroundContextMenu::Flags::ExtendedVerbs);
	}

	contextMenu.ShowMenu(m_listView, &pt, serviceProvider.get(), flags);
}

void ShellBrowserImpl::ShowLegacyItemContextMenu(const POINT &pt,
	const std::vector<PidlAbsolute> &selectedItems)
{
	m_legacyContextMenuPreloader.reset();

	std::vector<PCITEMID_CHILD> childPidls;

	for (const auto &item : selectedItems)
	{
		childPidls.push_back(ILFindLastID(item.Raw()));
	}

	ShellItemContextMenu contextMenu(m_directoryState.pidlDirectory.Raw(), childPidls, m_browser);

	OpenItemsContextMenuDelegate openItemsDelegate(m_browser, m_app->GetResourceLoader());
	contextMenu.AddDelegate(&openItemsDelegate);

	ShellBrowserContextMenuDelegate shellBrowserDelegate(m_weakPtrFactory.GetWeakPtr());
	contextMenu.AddDelegate(&shellBrowserDelegate);

	ShellItemContextMenu::Flags flags = ShellItemContextMenu::Flags::Rename;

	if (IsKeyDown(VK_SHIFT))
	{
		WI_SetFlag(flags, ShellItemContextMenu::Flags::ExtendedVerbs);
	}

	contextMenu.ShowMenu(m_listView, &pt, nullptr, flags);
}

void ShellBrowserImpl::ShowQuickBackgroundContextMenu(const POINT &pt)
{
	m_legacyContextMenuPreloader.reset();

	auto *resourceLoader = m_app->GetResourceLoader();
	auto *acceleratorManager = m_app->GetAcceleratorManager();

	PopupMenuView menuView(m_browser);
	AppendMenuItem(&menuView, acceleratorManager, resourceLoader, IDM_QUICK_CONTEXT_MENU_VIEW,
		resourceLoader->LoadString(IDS_BACKGROUND_CONTEXT_MENU_VIEW) + L"...", Icon::Views);
	AppendMenuItem(&menuView, acceleratorManager, resourceLoader, IDM_QUICK_CONTEXT_MENU_SORT_BY,
		resourceLoader->LoadString(IDS_BACKGROUND_CONTEXT_MENU_SORT_BY) + L"...");
	AppendMenuItem(&menuView, acceleratorManager, resourceLoader, IDM_QUICK_CONTEXT_MENU_GROUP_BY,
		resourceLoader->LoadString(IDS_BACKGROUND_CONTEXT_MENU_GROUP_BY) + L"...");
	menuView.AppendSeparator();
	AppendMenuItem(&menuView, acceleratorManager, resourceLoader, IDM_VIEW_REFRESH,
		resourceLoader->LoadString(IDS_BACKGROUND_CONTEXT_MENU_REFRESH), Icon::Refresh);
	AppendMenuItem(&menuView, acceleratorManager, resourceLoader, IDM_BACKGROUND_CONTEXT_MENU_PASTE,
		resourceLoader->LoadString(IDS_BACKGROUND_CONTEXT_MENU_PASTE), Icon::Paste);
	AppendMenuItem(&menuView, acceleratorManager, resourceLoader,
		IDM_BACKGROUND_CONTEXT_MENU_PASTE_SHORTCUT,
		resourceLoader->LoadString(IDS_BACKGROUND_CONTEXT_MENU_PASTE_SHORTCUT), Icon::PasteShortcut);

	menuView.EnableItem(IDM_BACKGROUND_CONTEXT_MENU_PASTE,
		CanPasteInDirectory(m_app->GetPlatformContext()->GetClipboardStore(),
			m_directoryState.pidlDirectory.Raw(), PasteType::Normal));
	menuView.EnableItem(IDM_BACKGROUND_CONTEXT_MENU_PASTE_SHORTCUT,
		CanPasteInDirectory(m_app->GetPlatformContext()->GetClipboardStore(),
			m_directoryState.pidlDirectory.Raw(), PasteType::Shortcut));

	if (CanCustomizeDirectory(m_directoryState.pidlDirectory.Raw()))
	{
		AppendMenuItem(&menuView, acceleratorManager, resourceLoader,
			IDM_BACKGROUND_CONTEXT_MENU_CUSTOMIZE,
			resourceLoader->LoadString(IDS_BACKGROUND_CONTEXT_MENU_CUSTOMIZE));
	}

	std::unordered_map<UINT, const ContextMenuActions::ContextMenuAction *> customActions;

	if (m_config->showQuickContextMenuCustomActions)
	{
		auto actionContext = BuildBackgroundInvocationContext(GetDirectoryPath());
		UINT customActionId = IDM_QUICK_CONTEXT_MENU_CUSTOM_ACTION_START;

		for (const auto &contextMenuAction : m_app->GetContextMenuActionModel()->GetItems())
		{
			if (!ContextMenuActions::IsApplicable(*contextMenuAction, actionContext))
			{
				continue;
			}

			menuView.AppendSeparator();
			break;
		}

		for (const auto &contextMenuAction : m_app->GetContextMenuActionModel()->GetItems())
		{
			if (!ContextMenuActions::IsApplicable(*contextMenuAction, actionContext))
			{
				continue;
			}

			menuView.AppendItem(customActionId, contextMenuAction->GetName(),
				MaybeBuildCustomActionIcon(*contextMenuAction),
				contextMenuAction->GetCommand());
			customActions.insert({ customActionId, contextMenuAction.get() });
			customActionId++;
		}
	}

	menuView.AppendSeparator();
	AppendMenuItem(&menuView, acceleratorManager, resourceLoader, IDM_QUICK_CONTEXT_MENU_MORE_ACTIONS,
		L"More actions...");
	menuView.RemoveTrailingSeparators();

	auto directoryPidl = PidlAbsolute(m_directoryState.pidlDirectory.Raw());
	auto backgroundFlags = ShellBackgroundContextMenu::Flags::None;

	if (IsKeyDown(VK_SHIFT))
	{
		WI_SetFlag(backgroundFlags, ShellBackgroundContextMenu::Flags::ExtendedVerbs);
	}

	auto backgroundContextMenu =
		std::make_shared<ShellBackgroundContextMenu>(directoryPidl.Raw(), m_browser);
	auto serviceProvider =
		BuildBackgroundContextMenuSite(this, m_app, m_browser, directoryPidl.Raw());
	m_legacyContextMenuPreloader = std::make_unique<LegacyContextMenuPreloader>(m_listView,
		m_app->GetRuntime(),
		LegacyContextMenuPreloader::Callbacks{
			[backgroundContextMenu, backgroundFlags](HWND hwnd, IUnknown *site)
			{
				return backgroundContextMenu->BuildShellMenu(hwnd, site,
					BuildBackgroundContextMenuFlags(backgroundFlags));
			},
			[backgroundContextMenu, browser = m_browser,
				clipboardStore = m_app->GetPlatformContext()->GetClipboardStore(),
				resourceLoader](HWND hwnd, const POINT &menuPt, IUnknown *site,
				ShellContextMenu::PreparedMenu preparedMenu)
			{
				BackgroundContextMenuDelegate backgroundDelegate(browser, clipboardStore,
					resourceLoader);
				backgroundContextMenu->AddDelegate(&backgroundDelegate);
				backgroundContextMenu->ShowPreparedMenu(hwnd, &menuPt, site, std::move(preparedMenu));
			},
			[this](const POINT &menuPt) { ShowLegacyBackgroundContextMenu(menuPt); } },
		serviceProvider.get());

	auto selectedObserver = menuView.AddItemSelectedObserver(
		[this, pt, resourceLoader, &customActions](UINT menuItemId, bool isCtrlKeyDown,
			bool isShiftKeyDown)
		{
			UNREFERENCED_PARAMETER(isCtrlKeyDown);
			UNREFERENCED_PARAMETER(isShiftKeyDown);

			auto customActionContext = BuildBackgroundInvocationContext(GetDirectoryPath());
			auto customActionItr = customActions.find(menuItemId);

			if (customActionItr != customActions.end())
			{
				ContextMenuActions::ExecuteAction(m_browser->GetHWND(), *customActionItr->second,
					customActionContext);
				return;
			}

			switch (menuItemId)
			{
			case IDM_QUICK_CONTEXT_MENU_VIEW:
			{
				ViewsMenuBuilder viewsMenuBuilder(resourceLoader);
				auto menu = viewsMenuBuilder.BuildMenu(m_browser);
				UINT command = ShowCommandMenu(menu.get(), m_listView, pt);

				for (auto viewMode : VIEW_MODES)
				{
					if (GetViewModeMenuId(viewMode) == command)
					{
						SetViewMode(viewMode);
						break;
					}
				}
			}
			break;

			case IDM_QUICK_CONTEXT_MENU_SORT_BY:
			{
				SortMenuBuilder sortMenuBuilder(resourceLoader);
				auto sortMenus = sortMenuBuilder.BuildMenus(*GetTab());
				UINT command = ShowCommandMenu(sortMenus.sortByMenu.get(), m_listView, pt);

				if (command == IDM_SORT_ASCENDING)
				{
					SetSortDirection(SortDirection::Ascending);
				}
				else if (command == IDM_SORT_DESCENDING)
				{
					SetSortDirection(SortDirection::Descending);
				}
				else if (IsSortModeMenuItemId(command))
				{
					SetSortMode(GetSortModeForMenuItemId(command));
				}
			}
			break;

			case IDM_QUICK_CONTEXT_MENU_GROUP_BY:
			{
				SortMenuBuilder sortMenuBuilder(resourceLoader);
				auto sortMenus = sortMenuBuilder.BuildMenus(*GetTab());
				UINT command = ShowCommandMenu(sortMenus.groupByMenu.get(), m_listView, pt);

				if (command == IDM_GROUP_BY_NONE)
				{
					SetShowInGroups(false);
				}
				else if (command == IDM_GROUP_SORT_ASCENDING)
				{
					SetGroupSortDirection(SortDirection::Ascending);
				}
				else if (command == IDM_GROUP_SORT_DESCENDING)
				{
					SetGroupSortDirection(SortDirection::Descending);
				}
				else if (IsGroupModeMenuItemId(command))
				{
					SetShowInGroups(true);
					SetGroupMode(GetSortModeForGroupMenuItemId(command));
				}
			}
			break;

			case IDM_VIEW_REFRESH:
				GetNavigationController()->Refresh();
				break;

			case IDM_BACKGROUND_CONTEXT_MENU_PASTE:
				SetFocus(m_listView);
				SendMessage(m_browser->GetHWND(), WM_COMMAND, MAKEWPARAM(IDM_EDIT_PASTE, 0), 0);
				break;

			case IDM_BACKGROUND_CONTEXT_MENU_PASTE_SHORTCUT:
				SetFocus(m_listView);
				SendMessage(m_browser->GetHWND(), WM_COMMAND, MAKEWPARAM(IDM_EDIT_PASTESHORTCUT, 0), 0);
				break;

			case IDM_BACKGROUND_CONTEXT_MENU_CUSTOMIZE:
				ExecuteFileAction(m_browser->GetHWND(), m_directoryState.pidlDirectory.Raw(),
					L"properties", L"customize", L"");
				break;

			case IDM_QUICK_CONTEXT_MENU_MORE_ACTIONS:
				if (!m_legacyContextMenuPreloader
					|| !m_legacyContextMenuPreloader->OnMoreActionsSelected(pt))
				{
					ShowLegacyBackgroundContextMenu(pt);
				}
				break;
			}
		});

	UNREFERENCED_PARAMETER(selectedObserver);
	menuView.Show(m_listView, pt);
}

void ShellBrowserImpl::ShowQuickItemContextMenu(const POINT &pt,
	const std::vector<PidlAbsolute> &selectedItems)
{
	m_legacyContextMenuPreloader.reset();

	auto *resourceLoader = m_app->GetResourceLoader();
	auto *acceleratorManager = m_app->GetAcceleratorManager();

	std::vector<std::wstring> selectedItemPaths;
	selectedItemPaths.reserve(selectedItems.size());

	for (const auto &selectedItem : selectedItems)
	{
		std::wstring path;
		HRESULT hr = GetDisplayName(selectedItem.Raw(), SHGDN_FORPARSING, path);

		if (SUCCEEDED(hr))
		{
			selectedItemPaths.push_back(path);
		}
	}

	auto actionContext = BuildItemInvocationContext(GetDirectoryPath(), selectedItemPaths,
		GetNumSelectedFiles(), GetNumSelectedFolders());

	PopupMenuView menuView(m_browser);
	AppendMenuItem(&menuView, acceleratorManager, resourceLoader, IDM_QUICK_CONTEXT_MENU_OPEN,
		resourceLoader->LoadString(IDS_APPLICATION_CONTEXT_MENU_OPEN), std::nullopt, L"",
		MaybeBuildOpenItemIcon(m_shellIconLoader.get(), m_config, selectedItems));

	bool showOpenInNewTab =
		selectedItems.size() == 1 && m_browser->CanOpenItemInNewTab(selectedItems[0].Raw());
	bool canDuplicate = !InVirtualFolder() && selectedItemPaths.size() == selectedItems.size();

	if (showOpenInNewTab)
	{
		AppendMenuItem(&menuView, acceleratorManager, resourceLoader,
			IDM_QUICK_CONTEXT_MENU_OPEN_IN_NEW_TAB,
			resourceLoader->LoadString(IDS_GENERAL_OPEN_IN_NEW_TAB), Icon::NewTab,
			resourceLoader->LoadString(IDS_GENERAL_OPEN_IN_NEW_TAB_HELP_TEXT));
	}

	menuView.AppendSeparator();
	AppendMenuItem(&menuView, acceleratorManager, resourceLoader, IDM_EDIT_CUT,
		resourceLoader->LoadString(IDS_TOOLBAR_CUT), Icon::Cut);
	AppendMenuItem(&menuView, acceleratorManager, resourceLoader, IDM_EDIT_COPY,
		resourceLoader->LoadString(IDS_TOOLBAR_COPY), Icon::Copy);
	AppendMenuItem(&menuView, acceleratorManager, resourceLoader, IDM_QUICK_CONTEXT_MENU_DUPLICATE,
		L"Duplicate", Icon::Copy);
	AppendMenuItem(&menuView, acceleratorManager, resourceLoader, IDM_FILE_RENAME,
		resourceLoader->LoadString(IDS_BOOKMARK_TREEVIEW_CONTEXT_MENU_RENAME), Icon::Rename);
	AppendMenuItem(&menuView, acceleratorManager, resourceLoader, IDM_FILE_DELETE,
		resourceLoader->LoadString(IDS_TOOLBAR_DELETE), Icon::Delete);
	AppendMenuItem(&menuView, acceleratorManager, resourceLoader, IDM_FILE_PROPERTIES,
		resourceLoader->LoadString(IDS_APPLICATION_CONTEXT_MENU_PROPERTIES), Icon::Properties);

	menuView.EnableItem(IDM_EDIT_CUT, IsCommandEnabled(IDM_EDIT_CUT));
	menuView.EnableItem(IDM_EDIT_COPY, IsCommandEnabled(IDM_EDIT_COPY));
	menuView.EnableItem(IDM_QUICK_CONTEXT_MENU_DUPLICATE, canDuplicate);
	menuView.EnableItem(IDM_FILE_RENAME, IsCommandEnabled(IDM_FILE_RENAME));
	menuView.EnableItem(IDM_FILE_DELETE, IsCommandEnabled(IDM_FILE_DELETE));
	menuView.EnableItem(IDM_FILE_PROPERTIES, IsCommandEnabled(IDM_FILE_PROPERTIES));

	std::unordered_map<UINT, const ContextMenuActions::ContextMenuAction *> customActions;

	if (m_config->showQuickContextMenuCustomActions)
	{
		UINT customActionId = IDM_QUICK_CONTEXT_MENU_CUSTOM_ACTION_START;
		bool addedSeparator = false;

		for (const auto &contextMenuAction : m_app->GetContextMenuActionModel()->GetItems())
		{
			if (!ContextMenuActions::IsApplicable(*contextMenuAction, actionContext))
			{
				continue;
			}

			if (!addedSeparator)
			{
				menuView.AppendSeparator();
				addedSeparator = true;
			}

			menuView.AppendItem(customActionId, contextMenuAction->GetName(),
				MaybeBuildCustomActionIcon(*contextMenuAction),
				contextMenuAction->GetCommand());
			customActions.insert({ customActionId, contextMenuAction.get() });
			customActionId++;
		}
	}

	menuView.AppendSeparator();
	AppendMenuItem(&menuView, acceleratorManager, resourceLoader, IDM_QUICK_CONTEXT_MENU_MORE_ACTIONS,
		L"More actions...");
	menuView.RemoveTrailingSeparators();

	auto directoryPidl = PidlAbsolute(m_directoryState.pidlDirectory.Raw());
	std::vector<PCITEMID_CHILD> childPidls;
	childPidls.reserve(selectedItems.size());

	for (const auto &selectedItem : selectedItems)
	{
		childPidls.push_back(ILFindLastID(selectedItem.Raw()));
	}

	auto itemFlags = ShellItemContextMenu::Flags::Rename;

	if (IsKeyDown(VK_SHIFT))
	{
		WI_SetFlag(itemFlags, ShellItemContextMenu::Flags::ExtendedVerbs);
	}

	auto itemContextMenu =
		std::make_shared<ShellItemContextMenu>(directoryPidl.Raw(), childPidls, m_browser);
	auto selectedItemsCopy = selectedItems;
	m_legacyContextMenuPreloader = std::make_unique<LegacyContextMenuPreloader>(m_listView,
		m_app->GetRuntime(),
		LegacyContextMenuPreloader::Callbacks{
			[itemContextMenu, itemFlags](HWND hwnd, IUnknown *site)
			{
				return itemContextMenu->BuildShellMenu(hwnd, site,
					BuildItemContextMenuFlags(itemFlags));
			},
			[itemContextMenu, browser = m_browser, resourceLoader,
				shellBrowserWeak = m_weakPtrFactory.GetWeakPtr()](HWND hwnd, const POINT &menuPt,
				IUnknown *site, ShellContextMenu::PreparedMenu preparedMenu)
			{
				OpenItemsContextMenuDelegate openItemsDelegate(browser, resourceLoader);
				itemContextMenu->AddDelegate(&openItemsDelegate);

				ShellBrowserContextMenuDelegate shellBrowserDelegate(shellBrowserWeak);
				itemContextMenu->AddDelegate(&shellBrowserDelegate);
				itemContextMenu->ShowPreparedMenu(hwnd, &menuPt, site, std::move(preparedMenu));
			},
			[this, selectedItemsCopy](const POINT &menuPt)
			{
				ShowLegacyItemContextMenu(menuPt, selectedItemsCopy);
			} });

	auto selectedObserver = menuView.AddItemSelectedObserver(
		[this, pt, &selectedItems, selectedItemPaths, actionContext,
			&customActions](UINT menuItemId, bool isCtrlKeyDown, bool isShiftKeyDown)
		{
			auto customActionItr = customActions.find(menuItemId);

			if (customActionItr != customActions.end())
			{
				ContextMenuActions::ExecuteAction(m_browser->GetHWND(), *customActionItr->second,
					actionContext);
				return;
			}

			switch (menuItemId)
			{
			case IDM_QUICK_CONTEXT_MENU_OPEN:
				if (selectedItems.size() == 1)
				{
					m_browser->OpenItem(selectedItems[0].Raw(),
						DetermineOpenDisposition(false, isCtrlKeyDown, isShiftKeyDown));
				}
				else
				{
					OpenSelectedItems();
				}
				break;

			case IDM_QUICK_CONTEXT_MENU_OPEN_IN_NEW_TAB:
				m_browser->OpenItem(selectedItems[0].Raw(),
					DetermineOpenDisposition(false, true, isShiftKeyDown));
				break;

			case IDM_EDIT_CUT:
			case IDM_EDIT_COPY:
			case IDM_FILE_RENAME:
			case IDM_FILE_PROPERTIES:
				ExecuteCommand(menuItemId);
				break;

			case IDM_QUICK_CONTEXT_MENU_DUPLICATE:
				DuplicateItemsInDirectory(m_browser->GetHWND(), selectedItems, selectedItemPaths,
					GetDirectoryPath());
				break;

			case IDM_FILE_DELETE:
				ExecuteCommand(isShiftKeyDown ? IDM_FILE_DELETEPERMANENTLY : IDM_FILE_DELETE);
				break;

			case IDM_QUICK_CONTEXT_MENU_MORE_ACTIONS:
				if (!m_legacyContextMenuPreloader
					|| !m_legacyContextMenuPreloader->OnMoreActionsSelected(pt))
				{
					ShowLegacyItemContextMenu(pt, selectedItems);
				}
				break;
			}
		});

	UNREFERENCED_PARAMETER(selectedObserver);
	menuView.Show(m_listView, pt);
}
