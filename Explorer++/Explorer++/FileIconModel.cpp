// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "FileIconModel.h"
#include "../Helper/ImageHelper.h"

FileIconModel::FileIconModel(const std::wstring &filePath, int size) :
	m_filePath(filePath),
	m_size(std::max(size, 1))
{
}

wil::unique_hbitmap FileIconModel::GetBitmap(UINT dpi, IconUpdateCallback updateCallback) const
{
	UNREFERENCED_PARAMETER(updateCallback);

	auto bitmap = std::make_unique<Gdiplus::Bitmap>(m_filePath.c_str(), FALSE);

	if (bitmap->GetLastStatus() != Gdiplus::Status::Ok)
	{
		return nullptr;
	}

	int scaledSize = MulDiv(m_size, dpi, USER_DEFAULT_SCREEN_DPI);
	auto scaledBitmap = std::make_unique<Gdiplus::Bitmap>(scaledSize, scaledSize,
		PixelFormat32bppARGB);

	if (scaledBitmap->GetLastStatus() != Gdiplus::Status::Ok)
	{
		return nullptr;
	}

	Gdiplus::Graphics graphics(scaledBitmap.get());

	if (graphics.GetLastStatus() != Gdiplus::Status::Ok)
	{
		return nullptr;
	}

	graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
	graphics.DrawImage(bitmap.get(), Gdiplus::Rect(0, 0, scaledSize, scaledSize), 0, 0,
		bitmap->GetWidth(), bitmap->GetHeight(), Gdiplus::UnitPixel);

	if (graphics.GetLastStatus() != Gdiplus::Status::Ok)
	{
		return nullptr;
	}

	return ImageHelper::GdiplusBitmapToBitmap(scaledBitmap.get());
}
