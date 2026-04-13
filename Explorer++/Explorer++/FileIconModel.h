// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include "IconModel.h"
#include <string>

class FileIconModel : public IconModel
{
public:
	FileIconModel(const std::wstring &filePath, int size);

	wil::unique_hbitmap GetBitmap(UINT dpi, IconUpdateCallback updateCallback) const override;

private:
	const std::wstring m_filePath;
	const int m_size;
};
