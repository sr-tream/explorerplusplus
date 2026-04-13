// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "ContextMenuActionStorage.h"
#include "ContextMenuAction.h"
#include "ContextMenuActionModel.h"
#include "../Helper/RegistrySettings.h"
#include "../Helper/XMLSettings.h"
#include <wil/com.h>

namespace ContextMenuActions
{

namespace Storage
{

namespace
{

const wchar_t CONTEXT_MENU_ACTIONS_KEY_PATH[] = L"ContextMenuActions";
const wchar_t CONTEXT_MENU_ACTIONS_NODE_NAME[] = L"ContextMenuActions";

const wchar_t SETTING_NAME[] = L"name";
const wchar_t SETTING_COMMAND[] = L"Command";
const wchar_t SETTING_ENABLED[] = L"Enabled";
const wchar_t SETTING_SHOW_IN_ITEM_MENU[] = L"ShowInItemMenu";
const wchar_t SETTING_SHOW_IN_BACKGROUND_MENU[] = L"ShowInBackgroundMenu";
const wchar_t SETTING_SHOW_FOR_FILES[] = L"ShowForFiles";
const wchar_t SETTING_SHOW_FOR_FOLDERS[] = L"ShowForFolders";
const wchar_t SETTING_ALLOW_MULTIPLE_SELECTION[] = L"AllowMultipleSelection";
const wchar_t SETTING_FILE_EXTENSIONS[] = L"FileExtensions";
const wchar_t SETTING_ICON_PATH[] = L"IconPath";

std::unique_ptr<ContextMenuAction> BuildContextMenuAction(const std::wstring &name,
	const std::wstring &command, bool enabled, bool showInItemMenu, bool showInBackgroundMenu,
	bool showForFiles, bool showForFolders, bool allowMultipleSelection,
	const std::wstring &fileExtensions, const std::wstring &iconPath)
{
	return std::make_unique<ContextMenuAction>(name, command, enabled, showInItemMenu,
		showInBackgroundMenu, showForFiles, showForFolders, allowMultipleSelection, fileExtensions,
		iconPath);
}

std::unique_ptr<ContextMenuAction> LoadContextMenuActionFromXmlNode(IXMLDOMNode *node)
{
	wil::com_ptr_nothrow<IXMLDOMNamedNodeMap> attributeMap;
	HRESULT hr = node->get_attributes(&attributeMap);

	if (FAILED(hr))
	{
		return nullptr;
	}

	std::wstring name;
	hr = XMLSettings::GetStringFromMap(attributeMap.get(), SETTING_NAME, name);

	if (FAILED(hr))
	{
		return nullptr;
	}

	std::wstring command;
	hr = XMLSettings::GetStringFromMap(attributeMap.get(), SETTING_COMMAND, command);

	if (FAILED(hr))
	{
		return nullptr;
	}

	bool enabled;
	hr = XMLSettings::GetBoolFromMap(attributeMap.get(), SETTING_ENABLED, enabled);

	if (FAILED(hr))
	{
		return nullptr;
	}

	bool showInItemMenu;
	hr = XMLSettings::GetBoolFromMap(attributeMap.get(), SETTING_SHOW_IN_ITEM_MENU, showInItemMenu);

	if (FAILED(hr))
	{
		return nullptr;
	}

	bool showInBackgroundMenu;
	hr = XMLSettings::GetBoolFromMap(attributeMap.get(), SETTING_SHOW_IN_BACKGROUND_MENU,
		showInBackgroundMenu);

	if (FAILED(hr))
	{
		return nullptr;
	}

	bool showForFiles;
	hr = XMLSettings::GetBoolFromMap(attributeMap.get(), SETTING_SHOW_FOR_FILES, showForFiles);

	if (FAILED(hr))
	{
		return nullptr;
	}

	bool showForFolders;
	hr = XMLSettings::GetBoolFromMap(attributeMap.get(), SETTING_SHOW_FOR_FOLDERS,
		showForFolders);

	if (FAILED(hr))
	{
		return nullptr;
	}

	bool allowMultipleSelection;
	hr = XMLSettings::GetBoolFromMap(attributeMap.get(), SETTING_ALLOW_MULTIPLE_SELECTION,
		allowMultipleSelection);

	if (FAILED(hr))
	{
		return nullptr;
	}

	std::wstring fileExtensions;
	XMLSettings::GetStringFromMap(attributeMap.get(), SETTING_FILE_EXTENSIONS, fileExtensions);
	std::wstring iconPath;
	XMLSettings::GetStringFromMap(attributeMap.get(), SETTING_ICON_PATH, iconPath);

	return BuildContextMenuAction(name, command, enabled, showInItemMenu, showInBackgroundMenu,
		showForFiles, showForFolders, allowMultipleSelection, fileExtensions, iconPath);
}

void LoadFromXmlNode(IXMLDOMNode *parentNode, ContextMenuActionModel *model)
{
	wil::com_ptr_nothrow<IXMLDOMNode> childNode;
	auto queryString = wil::make_bstr_nothrow(L"./ContextMenuAction");

	wil::com_ptr_nothrow<IXMLDOMNodeList> childNodes;
	HRESULT hr = parentNode->selectNodes(queryString.get(), &childNodes);

	if (FAILED(hr))
	{
		return;
	}

	while (childNodes->nextNode(&childNode) == S_OK)
	{
		auto contextMenuAction = LoadContextMenuActionFromXmlNode(childNode.get());

		if (!contextMenuAction)
		{
			continue;
		}

		model->AddItem(std::move(contextMenuAction));
	}
}

void SaveContextMenuActionToXml(IXMLDOMDocument *xmlDocument, IXMLDOMElement *parentNode,
	const ContextMenuAction *contextMenuAction)
{
	wil::com_ptr_nothrow<IXMLDOMElement> actionNode;
	XMLSettings::CreateElementNode(xmlDocument, &actionNode, parentNode, _T("ContextMenuAction"),
		contextMenuAction->GetName().c_str());
	XMLSettings::AddAttributeToNode(xmlDocument, actionNode.get(), SETTING_COMMAND,
		contextMenuAction->GetCommand().c_str());
	XMLSettings::AddAttributeToNode(xmlDocument, actionNode.get(), SETTING_ENABLED,
		XMLSettings::EncodeBoolValue(contextMenuAction->GetEnabled()));
	XMLSettings::AddAttributeToNode(xmlDocument, actionNode.get(), SETTING_SHOW_IN_ITEM_MENU,
		XMLSettings::EncodeBoolValue(contextMenuAction->GetShowInItemMenu()));
	XMLSettings::AddAttributeToNode(xmlDocument, actionNode.get(),
		SETTING_SHOW_IN_BACKGROUND_MENU,
		XMLSettings::EncodeBoolValue(contextMenuAction->GetShowInBackgroundMenu()));
	XMLSettings::AddAttributeToNode(xmlDocument, actionNode.get(), SETTING_SHOW_FOR_FILES,
		XMLSettings::EncodeBoolValue(contextMenuAction->GetShowForFiles()));
	XMLSettings::AddAttributeToNode(xmlDocument, actionNode.get(), SETTING_SHOW_FOR_FOLDERS,
		XMLSettings::EncodeBoolValue(contextMenuAction->GetShowForFolders()));
	XMLSettings::AddAttributeToNode(xmlDocument, actionNode.get(),
		SETTING_ALLOW_MULTIPLE_SELECTION,
		XMLSettings::EncodeBoolValue(contextMenuAction->GetAllowMultipleSelection()));

	if (!contextMenuAction->GetFileExtensions().empty())
	{
		XMLSettings::AddAttributeToNode(xmlDocument, actionNode.get(), SETTING_FILE_EXTENSIONS,
			contextMenuAction->GetFileExtensions().c_str());
	}

	if (!contextMenuAction->GetIconPath().empty())
	{
		XMLSettings::AddAttributeToNode(xmlDocument, actionNode.get(), SETTING_ICON_PATH,
			contextMenuAction->GetIconPath().c_str());
	}
}

void SaveToXmlNode(IXMLDOMDocument *xmlDocument, IXMLDOMElement *parentNode,
	const ContextMenuActionModel *model)
{
	for (const auto &contextMenuAction : model->GetItems())
	{
		SaveContextMenuActionToXml(xmlDocument, parentNode, contextMenuAction.get());
	}
}

std::unique_ptr<ContextMenuAction> LoadContextMenuActionFromRegistryKey(HKEY key)
{
	std::wstring name;
	LSTATUS result = RegistrySettings::ReadString(key, SETTING_NAME, name);

	if (result != ERROR_SUCCESS)
	{
		return nullptr;
	}

	std::wstring command;
	result = RegistrySettings::ReadString(key, SETTING_COMMAND, command);

	if (result != ERROR_SUCCESS)
	{
		return nullptr;
	}

	bool enabled;
	result = RegistrySettings::Read32BitValueFromRegistry(key, SETTING_ENABLED, enabled);

	if (result != ERROR_SUCCESS)
	{
		return nullptr;
	}

	bool showInItemMenu;
	result =
		RegistrySettings::Read32BitValueFromRegistry(key, SETTING_SHOW_IN_ITEM_MENU, showInItemMenu);

	if (result != ERROR_SUCCESS)
	{
		return nullptr;
	}

	bool showInBackgroundMenu;
	result = RegistrySettings::Read32BitValueFromRegistry(key, SETTING_SHOW_IN_BACKGROUND_MENU,
		showInBackgroundMenu);

	if (result != ERROR_SUCCESS)
	{
		return nullptr;
	}

	bool showForFiles;
	result = RegistrySettings::Read32BitValueFromRegistry(key, SETTING_SHOW_FOR_FILES,
		showForFiles);

	if (result != ERROR_SUCCESS)
	{
		return nullptr;
	}

	bool showForFolders;
	result = RegistrySettings::Read32BitValueFromRegistry(key, SETTING_SHOW_FOR_FOLDERS,
		showForFolders);

	if (result != ERROR_SUCCESS)
	{
		return nullptr;
	}

	bool allowMultipleSelection;
	result = RegistrySettings::Read32BitValueFromRegistry(key, SETTING_ALLOW_MULTIPLE_SELECTION,
		allowMultipleSelection);

	if (result != ERROR_SUCCESS)
	{
		return nullptr;
	}

	std::wstring fileExtensions;
	RegistrySettings::ReadString(key, SETTING_FILE_EXTENSIONS, fileExtensions);
	std::wstring iconPath;
	RegistrySettings::ReadString(key, SETTING_ICON_PATH, iconPath);

	return BuildContextMenuAction(name, command, enabled, showInItemMenu, showInBackgroundMenu,
		showForFiles, showForFolders, allowMultipleSelection, fileExtensions, iconPath);
}

void LoadFromRegistryKey(HKEY parentKey, ContextMenuActionModel *model)
{
	wil::unique_hkey childKey;
	size_t index = 0;

	while (RegOpenKeyEx(parentKey, std::to_wstring(index).c_str(), 0, KEY_READ, &childKey)
		== ERROR_SUCCESS)
	{
		auto contextMenuAction = LoadContextMenuActionFromRegistryKey(childKey.get());

		if (contextMenuAction)
		{
			model->AddItem(std::move(contextMenuAction));
		}

		childKey.reset();
		index++;
	}
}

void SaveContextMenuActionToRegistryKey(HKEY key, const ContextMenuAction *contextMenuAction)
{
	RegistrySettings::SaveString(key, SETTING_NAME, contextMenuAction->GetName());
	RegistrySettings::SaveString(key, SETTING_COMMAND, contextMenuAction->GetCommand());
	RegistrySettings::SaveDword(key, SETTING_ENABLED, contextMenuAction->GetEnabled());
	RegistrySettings::SaveDword(key, SETTING_SHOW_IN_ITEM_MENU,
		contextMenuAction->GetShowInItemMenu());
	RegistrySettings::SaveDword(key, SETTING_SHOW_IN_BACKGROUND_MENU,
		contextMenuAction->GetShowInBackgroundMenu());
	RegistrySettings::SaveDword(key, SETTING_SHOW_FOR_FILES, contextMenuAction->GetShowForFiles());
	RegistrySettings::SaveDword(key, SETTING_SHOW_FOR_FOLDERS,
		contextMenuAction->GetShowForFolders());
	RegistrySettings::SaveDword(key, SETTING_ALLOW_MULTIPLE_SELECTION,
		contextMenuAction->GetAllowMultipleSelection());
	RegistrySettings::SaveString(key, SETTING_FILE_EXTENSIONS,
		contextMenuAction->GetFileExtensions());

	if (!contextMenuAction->GetIconPath().empty())
	{
		RegistrySettings::SaveString(key, SETTING_ICON_PATH, contextMenuAction->GetIconPath());
	}
}

void SaveToRegistryKey(HKEY parentKey, const ContextMenuActionModel *model)
{
	size_t index = 0;

	for (const auto &contextMenuAction : model->GetItems())
	{
		wil::unique_hkey childKey;
		LSTATUS result = RegCreateKeyEx(parentKey, std::to_wstring(index).c_str(), 0, nullptr,
			REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &childKey, nullptr);

		if (result == ERROR_SUCCESS)
		{
			SaveContextMenuActionToRegistryKey(childKey.get(), contextMenuAction.get());
			index++;
		}
	}
}

}

void LoadFromXml(IXMLDOMNode *rootNode, ContextMenuActionModel *model)
{
	wil::com_ptr_nothrow<IXMLDOMNode> contextMenuActionsNode;
	auto queryString = wil::make_bstr_nothrow(CONTEXT_MENU_ACTIONS_NODE_NAME);
	HRESULT hr = rootNode->selectSingleNode(queryString.get(), &contextMenuActionsNode);

	if (hr != S_OK)
	{
		return;
	}

	model->RemoveAllItems();
	LoadFromXmlNode(contextMenuActionsNode.get(), model);
}

void SaveToXml(IXMLDOMDocument *xmlDocument, IXMLDOMNode *rootNode,
	const ContextMenuActionModel *model)
{
	wil::com_ptr_nothrow<IXMLDOMElement> contextMenuActionsNode;
	auto nodeName = wil::make_bstr_nothrow(CONTEXT_MENU_ACTIONS_NODE_NAME);
	HRESULT hr = xmlDocument->createElement(nodeName.get(), &contextMenuActionsNode);

	if (FAILED(hr))
	{
		return;
	}

	SaveToXmlNode(xmlDocument, contextMenuActionsNode.get(), model);
	XMLSettings::AppendChildToParent(contextMenuActionsNode.get(), rootNode);
}

void LoadFromRegistry(HKEY applicationKey, ContextMenuActionModel *model)
{
	wil::unique_hkey contextMenuActionsKey;
	LSTATUS result =
		RegOpenKeyEx(applicationKey, CONTEXT_MENU_ACTIONS_KEY_PATH, 0, KEY_READ, &contextMenuActionsKey);

	if (result != ERROR_SUCCESS)
	{
		return;
	}

	model->RemoveAllItems();
	LoadFromRegistryKey(contextMenuActionsKey.get(), model);
}

void SaveToRegistry(HKEY applicationKey, const ContextMenuActionModel *model)
{
	LSTATUS deleteResult = RegDeleteTree(applicationKey, CONTEXT_MENU_ACTIONS_KEY_PATH);

	if (deleteResult != ERROR_SUCCESS && deleteResult != ERROR_FILE_NOT_FOUND)
	{
		return;
	}

	wil::unique_hkey contextMenuActionsKey;
	LSTATUS result = RegCreateKeyEx(applicationKey, CONTEXT_MENU_ACTIONS_KEY_PATH, 0, nullptr,
		REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &contextMenuActionsKey, nullptr);

	if (result != ERROR_SUCCESS)
	{
		return;
	}

	SaveToRegistryKey(contextMenuActionsKey.get(), model);
}

}

}
