// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "ItemInfo.h"
#include "../Helper/ShellHelper.h"
#include <wil/com.h>
#include <propkey.h>
#include <propvarutil.h>

namespace
{

HRESULT ExtractFindDataUsingPropertyStore(IShellFolder *shellFolder, PCITEMID_CHILD pidlChild,
	WIN32_FIND_DATA &output)
{
	wil::com_ptr_nothrow<IPropertyStoreFactory> factory;
	HRESULT hr = shellFolder->BindToObject(pidlChild, nullptr, IID_PPV_ARGS(&factory));

	if (FAILED(hr))
	{
		return hr;
	}

	wil::com_ptr_nothrow<IPropertyStore> store;
	PROPERTYKEY keys[] = { PKEY_FindData };
	hr = factory->GetPropertyStoreForKeys(keys, std::size(keys), GPS_FASTPROPERTIESONLY,
		IID_PPV_ARGS(&store));

	if (FAILED(hr))
	{
		return hr;
	}

	wil::unique_prop_variant findDataProp;
	hr = store->GetValue(PKEY_FindData, &findDataProp);

	if (FAILED(hr))
	{
		return hr;
	}

	if (PropVariantGetElementCount(findDataProp) != sizeof(WIN32_FIND_DATA))
	{
		return hr;
	}

	WIN32_FIND_DATA wfd;
	hr = PropVariantToBuffer(findDataProp, &wfd, sizeof(wfd));

	if (FAILED(hr))
	{
		return hr;
	}

	output = wfd;

	return hr;
}

}

std::optional<ItemInfo_t> RetrieveItemInformation(IShellFolder *shellFolder,
	PCIDLIST_ABSOLUTE pidlDirectory, PCITEMID_CHILD pidlChild)
{
	ItemInfo_t itemInfo;

	itemInfo.pidlComplete = PidlAbsolute(ILCombine(pidlDirectory, pidlChild), Pidl::takeOwnership);
	itemInfo.pridl = pidlChild;

	std::wstring parsingName;
	HRESULT hr = GetDisplayName(shellFolder, pidlChild, SHGDN_FORPARSING, parsingName);

	if (FAILED(hr))
	{
		return std::nullopt;
	}

	itemInfo.parsingName = parsingName;

	ULONG attributes = SFGAO_FOLDER | SFGAO_FILESYSTEM;
	PCITEMID_CHILD items[] = { pidlChild };
	hr = shellFolder->GetAttributesOf(1, items, &attributes);

	if (FAILED(hr))
	{
		return std::nullopt;
	}

	SHGDNF displayNameFlags = SHGDN_INFOLDER;

	unique_pidl_absolute recycleBinPidl;
	hr = SHGetKnownFolderIDList(FOLDERID_RecycleBinFolder, KF_FLAG_DEFAULT, nullptr,
		wil::out_param(recycleBinPidl));

	bool isRecycleBin = SUCCEEDED(hr) && ArePidlsEquivalent(pidlDirectory, recycleBinPidl.get());

	// SHGDN_INFOLDER | SHGDN_FORPARSING is used to ensure that the name retrieved for a filesystem
	// file contains an extension, even if extensions are hidden in Windows Explorer. When using
	// SHGDN_INFOLDER by itself, the resulting name won't contain an extension if extensions are
	// hidden in Windows Explorer.
	// Note that the recycle bin is excluded here, as the parsing names for the items are completely
	// different to their regular display names.
	if (!isRecycleBin && WI_IsFlagSet(attributes, SFGAO_FILESYSTEM)
		&& WI_IsFlagClear(attributes, SFGAO_FOLDER))
	{
		WI_SetFlag(displayNameFlags, SHGDN_FORPARSING);
	}

	std::wstring displayName;
	hr = GetDisplayName(shellFolder, pidlChild, displayNameFlags, displayName);

	if (FAILED(hr))
	{
		return std::nullopt;
	}

	itemInfo.displayName = displayName;

	std::wstring editingName;
	hr = GetDisplayName(shellFolder, pidlChild, SHGDN_INFOLDER | SHGDN_FOREDITING, editingName);

	if (FAILED(hr))
	{
		return std::nullopt;
	}

	itemInfo.editingName = editingName;

	if (PathIsRoot(parsingName.c_str()))
	{
		itemInfo.bDrive = TRUE;
		StringCchCopy(itemInfo.szDrive, std::size(itemInfo.szDrive), parsingName.c_str());
	}
	else
	{
		itemInfo.bDrive = FALSE;
	}

	WIN32_FIND_DATA wfd;
	hr = SHGetDataFromIDList(shellFolder, pidlChild, SHGDFIL_FINDDATA, &wfd, sizeof(wfd));

	if (FAILED(hr))
	{
		hr = ExtractFindDataUsingPropertyStore(shellFolder, pidlChild, wfd);
	}

	if (SUCCEEDED(hr))
	{
		itemInfo.wfd = wfd;
		itemInfo.isFindDataValid = true;
	}
	else
	{
		StringCchCopy(itemInfo.wfd.cFileName, std::size(itemInfo.wfd.cFileName),
			displayName.c_str());

		if (WI_IsFlagSet(attributes, SFGAO_FOLDER))
		{
			WI_SetFlag(itemInfo.wfd.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY);
		}
	}

	return itemInfo;
}

std::vector<ItemInfo_t> RetrieveItemInformationFromPidls(PCIDLIST_ABSOLUTE pidlDirectory,
	const std::vector<PidlChild> &itemPidls, std::stop_token stopToken)
{
	if (itemPidls.empty())
	{
		return {};
	}

	wil::com_ptr_nothrow<IShellFolder> shellFolder;
	HRESULT hr =
		SHBindToObject(nullptr, pidlDirectory, nullptr, IID_PPV_ARGS(&shellFolder));

	if (FAILED(hr))
	{
		return {};
	}

	std::vector<ItemInfo_t> items;
	items.reserve(itemPidls.size());

	for (const auto &pidl : itemPidls)
	{
		if (stopToken.stop_requested())
		{
			break;
		}

		auto item = RetrieveItemInformation(shellFolder.get(), pidlDirectory, pidl.Raw());

		if (item)
		{
			items.push_back(std::move(*item));
		}
	}

	return items;
}
