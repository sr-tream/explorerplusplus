// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include <chrono>

class TabLoadingIndicatorState
{
public:
	bool Start(std::chrono::milliseconds delay)
	{
		bool wasVisible = m_visible;

		m_navigationInProgress = true;
		m_visible = delay <= std::chrono::milliseconds::zero();

		return wasVisible != m_visible;
	}

	bool ShowAfterDelay()
	{
		if (!m_navigationInProgress || m_visible)
		{
			return false;
		}

		m_visible = true;
		return true;
	}

	bool Finish()
	{
		bool wasVisible = m_visible;

		m_navigationInProgress = false;
		m_visible = false;

		return wasVisible;
	}

	bool IsVisible() const
	{
		return m_visible;
	}

private:
	bool m_navigationInProgress = false;
	bool m_visible = false;
};
