// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include <windows.h>

// Accumulates mouse wheel deltas across messages so that high-precision touchpads (which deliver
// fractional WHEEL_DELTA values per event) eventually trigger a line-step scroll instead of being
// truncated to zero by a `delta / WHEEL_DELTA` division. A real mouse wheel always delivers a
// multiple of WHEEL_DELTA per notch, so its behavior is unchanged.
class WheelAccumulator
{
public:
	// Adds delta to the residual and returns the largest line-aligned portion (a multiple of
	// WHEEL_DELTA). The remainder is kept for the next call. Returns 0 when nothing has crossed a
	// line boundary yet.
	int AddDelta(int delta)
	{
		m_residual += delta;
		int lines = m_residual / WHEEL_DELTA;
		m_residual -= lines * WHEEL_DELTA;
		return lines * WHEEL_DELTA;
	}

	void Reset()
	{
		m_residual = 0;
	}

private:
	int m_residual = 0;
};
