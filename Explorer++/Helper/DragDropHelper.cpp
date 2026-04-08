// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "DragDropHelper.h"
#include "DataObjectWrapper.h"
#include "Helper.h"
#include "ShellHelper.h"
#include "WinRTBaseWrapper.h"
#include <wil/com.h>

// If any dragged item is a shell link (.lnk shortcut or NTFS symlink), overrides the CF_HDROP
// format on the data object with the resolved target paths so that non-shell apps that read only
// raw file paths (e.g. Telegram) receive the real file instead of the link. Shell-specific formats
// such as CFSTR_SHELLIDLIST are left untouched, preserving correct shell drag semantics.
static void OverrideCfHDropWithResolvedLinks(const std::vector<PCIDLIST_ABSOLUTE> &items,
	IDataObject *dataObject)
{
	bool anyResolved = false;
	std::vector<std::wstring> paths;
	paths.reserve(items.size());

	for (PCIDLIST_ABSOLUTE item : items)
	{
		unique_pidl_absolute targetPidl;

		if (SUCCEEDED(MaybeGetLinkTarget(item, targetPidl)))
		{
			wchar_t targetPath[MAX_PATH];

			if (SHGetPathFromIDList(targetPidl.get(), targetPath))
			{
				paths.emplace_back(targetPath);
				anyResolved = true;
				continue;
			}
		}

		// Not a link or resolution failed — use the original path as-is.
		wchar_t path[MAX_PATH];

		if (SHGetPathFromIDList(item, path))
		{
			paths.emplace_back(path);
		}
	}

	if (!anyResolved || paths.empty())
	{
		return;
	}

	// Build a CF_HDROP HGLOBAL: DROPFILES header followed by wide null-terminated paths and a
	// final double null terminator.
	size_t totalChars = 1; // final null

	for (const auto &p : paths)
	{
		totalChars += p.size() + 1;
	}

	wil::unique_hglobal hDrop(GlobalAlloc(GHND, sizeof(DROPFILES) + totalChars * sizeof(wchar_t)));

	if (!hDrop)
	{
		return;
	}

	auto *df = static_cast<DROPFILES *>(GlobalLock(hDrop.get()));

	if (!df)
	{
		return;
	}

	df->pFiles = sizeof(DROPFILES);
	df->pt = {};
	df->fNC = FALSE;
	df->fWide = TRUE;

	auto *dest = reinterpret_cast<wchar_t *>(df + 1);

	for (const auto &p : paths)
	{
		wmemcpy(dest, p.c_str(), p.size());
		dest[p.size()] = L'\0';
		dest += p.size() + 1;
	}

	*dest = L'\0';
	GlobalUnlock(hDrop.get());

	FORMATETC ftc = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	MoveStorageToObject(dataObject, &ftc, GetStgMediumForGlobal(std::move(hDrop)));
}

wil::unique_stg_medium GetStgMediumForGlobal(wil::unique_hglobal global)
{
	wil::unique_stg_medium storage;
	storage.tymed = TYMED_HGLOBAL;
	storage.hGlobal = global.release();
	storage.pUnkForRelease = nullptr;
	return storage;
}

HRESULT SetPreferredDropEffect(IDataObject *dataObject, DWORD effect)
{
	return SetBlobData(dataObject,
		static_cast<CLIPFORMAT>(RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT)), effect);
}

HRESULT GetPreferredDropEffect(IDataObject *dataObject, DWORD &effect)
{
	return GetBlobData(dataObject,
		static_cast<CLIPFORMAT>(RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT)), effect);
}

HRESULT SetDropDescription(IDataObject *dataObject, DROPIMAGETYPE type, const std::wstring &message,
	const std::wstring &insert)
{
	DROPDESCRIPTION dropDescription;
	dropDescription.type = type;
	StringCchCopy(dropDescription.szMessage, std::size(dropDescription.szMessage), message.c_str());
	StringCchCopy(dropDescription.szInsert, std::size(dropDescription.szInsert), insert.c_str());

	return SetBlobData(dataObject,
		static_cast<CLIPFORMAT>(RegisterClipboardFormat(CFSTR_DROPDESCRIPTION)), dropDescription);
}

HRESULT ClearDropDescription(IDataObject *dataObject)
{
	return SetDropDescription(dataObject, DROPIMAGE_INVALID, L"", L"");
}

HRESULT StartDragForShellItems(const std::vector<PCIDLIST_ABSOLUTE> &items,
	std::optional<DWORD> preferredDropEffect)
{
	wil::com_ptr_nothrow<IShellItemArray> shellItemArray;
	RETURN_IF_FAILED(SHCreateShellItemArrayFromIDLists(static_cast<UINT>(items.size()),
		items.data(), &shellItemArray));

	SFGAOF attributes = SFGAO_CANCOPY | SFGAO_CANMOVE | SFGAO_CANLINK;
	RETURN_IF_FAILED(shellItemArray->GetAttributes(SIATTRIBFLAGS_AND, attributes, &attributes));

	if (WI_AreAllFlagsClear(attributes, SFGAO_CANCOPY | SFGAO_CANMOVE | SFGAO_CANLINK))
	{
		// The root desktop folder is at least one item that can't be copied/moved/linked. In a
		// situation like that, it's not possible to start a drag at all.
		return DRAGDROP_S_CANCEL;
	}

	wil::com_ptr_nothrow<IDataObject> dataObject;
	RETURN_IF_FAILED(CreateDataObjectForShellTransfer(items, &dataObject));

	DWORD allowedEffects = 0;

	if (WI_IsFlagSet(attributes, SFGAO_CANCOPY))
	{
		WI_SetFlag(allowedEffects, DROPEFFECT_COPY);
	}

	if (WI_IsFlagSet(attributes, SFGAO_CANMOVE))
	{
		WI_SetFlag(allowedEffects, DROPEFFECT_MOVE);
	}

	if (WI_IsFlagSet(attributes, SFGAO_CANLINK))
	{
		WI_SetFlag(allowedEffects, DROPEFFECT_LINK);
	}

	if (preferredDropEffect && WI_AreAllFlagsSet(allowedEffects, *preferredDropEffect))
	{
		RETURN_IF_FAILED(SetPreferredDropEffect(dataObject.get(), *preferredDropEffect));
	}

	// For link items (.lnk shortcuts, NTFS symlinks), override CF_HDROP with resolved target
	// paths so non-shell apps that consume only raw file paths receive the real file.
	OverrideCfHDropWithResolvedLinks(items, dataObject.get());

	DWORD effect;
	return SHDoDragDrop(nullptr, dataObject.get(), nullptr, allowedEffects, &effect);
}

HRESULT CreateDataObjectForShellTransfer(const std::vector<PidlAbsolute> &items,
	IDataObject **dataObjectOut)
{
	std::vector<PCIDLIST_ABSOLUTE> rawItems;

	for (const auto &pidl : items)
	{
		rawItems.push_back(pidl.Raw());
	}

	return CreateDataObjectForShellTransfer(rawItems, dataObjectOut);
}

// Returns an IDataObject instance that can be used for clipboard operations and drag and drop.
HRESULT CreateDataObjectForShellTransfer(const std::vector<PCIDLIST_ABSOLUTE> &items,
	IDataObject **dataObjectOut)
{
	wil::com_ptr_nothrow<IShellItemArray> shellItemArray;
	RETURN_IF_FAILED(SHCreateShellItemArrayFromIDLists(static_cast<UINT>(items.size()),
		items.data(), &shellItemArray));

	wil::com_ptr_nothrow<IDataObject> shellDataObject;
	RETURN_IF_FAILED(
		shellItemArray->BindToHandler(nullptr, BHID_DataObject, IID_PPV_ARGS(&shellDataObject)));

	// Although it's possible to retrieve the IDataObjectAsyncCapability interface from the shell
	// IDataObject instance and call SetAsyncMode(), it appears that doesn't actually enable
	// asynchronous transfer. That's the reason the shell IDataObject instance is wrapped here.
	auto dataObject = winrt::make<DataObjectWrapper>(shellDataObject.get());

	// Attempting an asynchronous transfer on Windows PE will fail, so the transfer should be left
	// as synchronous (the default) in that case.
	if (!IsWindowsPE())
	{
		wil::com_ptr_nothrow<IDataObjectAsyncCapability> asyncCapability;
		RETURN_IF_FAILED(dataObject->QueryInterface(IID_PPV_ARGS(&asyncCapability)));
		RETURN_IF_FAILED(asyncCapability->SetAsyncMode(VARIANT_TRUE));
	}

	*dataObjectOut = dataObject.detach();

	return S_OK;
}

HRESULT GetTextFromDataObject(IDataObject *dataObject, std::wstring &outputText)
{
	FORMATETC ftc = { CF_UNICODETEXT, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	wil::unique_stg_medium stg;
	RETURN_IF_FAILED(dataObject->GetData(&ftc, &stg));

	auto text = ReadStringFromGlobal(stg.hGlobal);

	if (!text)
	{
		return E_FAIL;
	}

	outputText = *text;

	return S_OK;
}

HRESULT SetTextOnDataObject(IDataObject *dataObject, const std::wstring &text)
{
	auto global = WriteStringToGlobal(text);

	if (!global)
	{
		return E_FAIL;
	}

	FORMATETC ftc = { CF_UNICODETEXT, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	auto stg = GetStgMediumForGlobal(std::move(global));
	return MoveStorageToObject(dataObject, &ftc, std::move(stg));
}

HRESULT SetBlobData(IDataObject *dataObject, CLIPFORMAT format, const void *data, size_t size)
{
	FORMATETC ftc = { format, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	return SetBlobData(dataObject, &ftc, data, size);
}

HRESULT SetBlobData(IDataObject *dataObject, FORMATETC *ftc, const void *data, size_t size)
{
	auto global = WriteDataToGlobal(data, size);

	if (!global)
	{
		return E_FAIL;
	}

	auto stg = GetStgMediumForGlobal(std::move(global));
	return MoveStorageToObject(dataObject, ftc, std::move(stg));
}

HRESULT GetBlobData(IDataObject *dataObject, CLIPFORMAT format, std::string &outputData)
{
	FORMATETC ftc = { format, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	return GetBlobData(dataObject, &ftc, outputData);
}

HRESULT GetBlobData(IDataObject *dataObject, FORMATETC *ftc, std::string &outputData)
{
	wil::unique_stg_medium stg;
	RETURN_IF_FAILED(dataObject->GetData(ftc, &stg));

	auto data = ReadBinaryDataFromGlobal(stg.hGlobal);

	if (!data)
	{
		return E_FAIL;
	}

	outputData = *data;

	return S_OK;
}
