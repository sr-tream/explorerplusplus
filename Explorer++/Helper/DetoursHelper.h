// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include <KNSoft/SlimDetours/SlimDetours.h>

HRESULT InlineHook(bool enable, PVOID *ppPointer, PVOID pDetour);
