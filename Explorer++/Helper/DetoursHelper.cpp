// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "DetoursHelper.h"

#pragma comment(lib, "KNSoft.SlimDetours.lib")

HRESULT InlineHook(bool enable, PVOID *ppPointer, PVOID pDetour)
{
	return SlimDetoursInlineHook(enable ? TRUE : FALSE, ppPointer, pDetour);
}
