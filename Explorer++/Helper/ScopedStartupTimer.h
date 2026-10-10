// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include <glog/logging.h>
#include <chrono>

// Timings are emitted only when INFO logging is enabled (--enable-logging).
// Use a monotonic clock so clock adjustments don't affect startup measurements.
class ScopedStartupTimer
{
public:
	explicit ScopedStartupTimer(const char *phase,
		std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now()) :
		m_phase(phase),
		m_start(start)
	{
	}

	ScopedStartupTimer(const ScopedStartupTimer &) = delete;
	ScopedStartupTimer &operator=(const ScopedStartupTimer &) = delete;

	~ScopedStartupTimer()
	{
		Stop();
	}

	void Stop()
	{
		if (m_stopped)
		{
			return;
		}

		m_stopped = true;
		auto elapsed = std::chrono::duration<double, std::milli>(Clock::now() - m_start).count();
		LOG(INFO) << "Startup phase: " << m_phase << " elapsed_ms=" << elapsed;
	}

private:
	using Clock = std::chrono::steady_clock;

	const char *const m_phase;
	const Clock::time_point m_start;
	bool m_stopped = false;
};
