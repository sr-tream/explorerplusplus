// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "DataObjectWrapper.h"

DataObjectWrapper::DataObjectWrapper(IDataObject *dataObject) : m_dataObject(dataObject)
{
}

// IDataObject
IFACEMETHODIMP DataObjectWrapper::GetData(FORMATETC *formatEtc, STGMEDIUM *medium)
{
	auto it = m_localFormats.find(formatEtc->cfFormat);

	if (it != m_localFormats.end() && WI_IsAnyFlagSet(it->second.tymed, formatEtc->tymed))
	{
		return CopyStgMedium(&it->second, medium);
	}

	return m_dataObject->GetData(formatEtc, medium);
}

IFACEMETHODIMP DataObjectWrapper::GetDataHere(FORMATETC *formatEtc, STGMEDIUM *medium)
{
	return m_dataObject->GetDataHere(formatEtc, medium);
}

IFACEMETHODIMP DataObjectWrapper::QueryGetData(FORMATETC *formatEtc)
{
	auto it = m_localFormats.find(formatEtc->cfFormat);

	if (it != m_localFormats.end() && WI_IsAnyFlagSet(it->second.tymed, formatEtc->tymed))
	{
		return S_OK;
	}

	return m_dataObject->QueryGetData(formatEtc);
}

IFACEMETHODIMP DataObjectWrapper::GetCanonicalFormatEtc(FORMATETC *formatEtc,
	FORMATETC *formatEtcResult)
{
	return m_dataObject->GetCanonicalFormatEtc(formatEtc, formatEtcResult);
}

IFACEMETHODIMP DataObjectWrapper::SetData(FORMATETC *formatEtc, STGMEDIUM *medium,
	BOOL shouldRelease)
{
	// Store HGLOBAL-based formats locally so that GetData() returns the overridden data. The shell
	// IDataObject may not honour a SetData() call for formats it already provides internally.
	if (medium->tymed == TYMED_HGLOBAL)
	{
		wil::unique_stg_medium local;

		if (shouldRelease)
		{
			// Transfer ownership: take the HGLOBAL from the caller so it is freed when local is
			// destroyed. The caller must not release medium after SetData() returns.
			local.tymed = medium->tymed;
			local.hGlobal = medium->hGlobal;
			local.pUnkForRelease = medium->pUnkForRelease;
			medium->tymed = TYMED_NULL;
			medium->hGlobal = nullptr;
			medium->pUnkForRelease = nullptr;
		}
		else
		{
			RETURN_IF_FAILED(CopyStgMedium(medium, &local));
		}

		m_localFormats.erase(formatEtc->cfFormat);
		m_localFormats.emplace(formatEtc->cfFormat, std::move(local));
		return S_OK;
	}

	return m_dataObject->SetData(formatEtc, medium, shouldRelease);
}

IFACEMETHODIMP DataObjectWrapper::EnumFormatEtc(DWORD direction, IEnumFORMATETC **enumerator)
{
	return m_dataObject->EnumFormatEtc(direction, enumerator);
}

IFACEMETHODIMP DataObjectWrapper::DAdvise(FORMATETC *formatEtc, DWORD advf, IAdviseSink *sink,
	DWORD *connection)
{
	return m_dataObject->DAdvise(formatEtc, advf, sink, connection);
}

IFACEMETHODIMP DataObjectWrapper::DUnadvise(DWORD connection)
{
	return m_dataObject->DUnadvise(connection);
}

IFACEMETHODIMP DataObjectWrapper::EnumDAdvise(IEnumSTATDATA **enumerator)
{
	return m_dataObject->EnumDAdvise(enumerator);
}

// IDataObjectAsyncCapability
IFACEMETHODIMP DataObjectWrapper::GetAsyncMode(BOOL *isOpAsync)
{
	*isOpAsync = m_isOpAsync ? VARIANT_TRUE : VARIANT_FALSE;

	return S_OK;
}

IFACEMETHODIMP DataObjectWrapper::SetAsyncMode(BOOL doOpAsync)
{
	m_isOpAsync = !!doOpAsync;

	return S_OK;
}

IFACEMETHODIMP DataObjectWrapper::InOperation(BOOL *inAsyncOp)
{
	*inAsyncOp = m_inOperation ? VARIANT_TRUE : VARIANT_FALSE;

	return S_OK;
}

IFACEMETHODIMP DataObjectWrapper::StartOperation(IBindCtx *reserved)
{
	UNREFERENCED_PARAMETER(reserved);

	m_inOperation = true;

	return S_OK;
}

IFACEMETHODIMP DataObjectWrapper::EndOperation(HRESULT result, IBindCtx *reserved, DWORD effects)
{
	UNREFERENCED_PARAMETER(result);
	UNREFERENCED_PARAMETER(reserved);
	UNREFERENCED_PARAMETER(effects);

	m_inOperation = false;
	return S_OK;
}
