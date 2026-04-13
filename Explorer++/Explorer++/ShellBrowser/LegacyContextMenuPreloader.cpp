// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "LegacyContextMenuPreloader.h"
#include "../Runtime.h"
#include "../RuntimeHelper.h"
#include <optional>
#include <wil/resource.h>

namespace
{

constexpr int PRELOAD_THREAD_PRIORITY = THREAD_PRIORITY_LOWEST;
constexpr int INTERACTIVE_THREAD_PRIORITY = THREAD_PRIORITY_ABOVE_NORMAL;

template <typename T>
wil::com_ptr_nothrow<IStream> MarshalInterfaceToStream(T *interfacePointer)
{
	if (!interfacePointer)
	{
		return nullptr;
	}

	wil::com_ptr_nothrow<IStream> stream;
	HRESULT hr = CoMarshalInterThreadInterfaceInStream(__uuidof(T), interfacePointer, &stream);

	if (FAILED(hr))
	{
		return nullptr;
	}

	return stream;
}

template <typename T>
wil::com_ptr_nothrow<T> UnmarshalInterfaceFromStream(wil::com_ptr_nothrow<IStream> stream)
{
	if (!stream)
	{
		return nullptr;
	}

	wil::com_ptr_nothrow<T> interfacePointer;
	HRESULT hr =
		CoGetInterfaceAndReleaseStream(stream.detach(), __uuidof(T), interfacePointer.put_void());

	if (FAILED(hr))
	{
		return nullptr;
	}

	return interfacePointer;
}

}

struct LegacyContextMenuPreloader::State
{
	struct PreparedMenuResult
	{
		wil::unique_hmenu menu;
		wil::com_ptr_nothrow<IStream> contextMenuStream;
		bool buildSucceeded = false;
		bool hasShellContextMenu = false;
	};

	enum class Status
	{
		Pending,
		Ready,
		Failed,
		Consumed
	};

	State(HWND ownerWindow, Runtime *runtime, Callbacks callbacks, IUnknown *site) :
		ownerWindow(ownerWindow),
		runtime(runtime),
		callbacks(std::move(callbacks))
	{
		if (site)
		{
			site->AddRef();
			uiSite.attach(site);
		}
	}

	void RegisterWorkerThread()
	{
		std::scoped_lock lock(priorityMutex);

		HANDLE duplicatedHandle = nullptr;
		auto currentProcess = GetCurrentProcess();
		if (DuplicateHandle(currentProcess, GetCurrentThread(), currentProcess, &duplicatedHandle,
				THREAD_QUERY_LIMITED_INFORMATION | THREAD_SET_INFORMATION, FALSE, 0))
		{
			workerThread.reset(duplicatedHandle);
		}

		originalThreadPriority = GetThreadPriority(GetCurrentThread());
		ApplyCurrentPriorityPreference(GetCurrentThread());
	}

	void RestoreWorkerThreadPriority()
	{
		std::scoped_lock lock(priorityMutex);

		if (originalThreadPriority != THREAD_PRIORITY_ERROR_RETURN)
		{
			SetThreadPriority(GetCurrentThread(), originalThreadPriority);
		}

		workerThread.reset();
	}

	void BoostWorkerThreadPriority()
	{
		interactivePriorityRequested = true;

		std::scoped_lock lock(priorityMutex);

		if (workerThread)
		{
			SetThreadPriority(workerThread.get(), INTERACTIVE_THREAD_PRIORITY);
		}
	}

	void OnBuildCompleted(PreparedMenuResult preparedMenuResult)
	{
		if (!active)
		{
			return;
		}

		preparedMenuResultOpt.emplace(std::move(preparedMenuResult));
		status = preparedMenuResultOpt->buildSucceeded ? Status::Ready : Status::Failed;

		if (requestedShowPoint)
		{
			auto pt = *requestedShowPoint;
			requestedShowPoint.reset();
			ShowPreparedOrFallback(pt);
		}
	}

	bool OnMoreActionsSelected(const POINT &pt)
	{
		if (!active)
		{
			return false;
		}

		switch (status)
		{
		case Status::Ready:
			ShowPreparedOrFallback(pt);
			return true;

		case Status::Pending:
			requestedShowPoint = pt;
			BoostWorkerThreadPriority();
			return true;

		case Status::Failed:
			callbacks.fallbackShowMenu(pt);
			status = Status::Consumed;
			active = false;
			return true;

		case Status::Consumed:
			return false;
		}

		return false;
	}

	void Invalidate()
	{
		active = false;
		preparedMenuResultOpt.reset();

		std::scoped_lock lock(priorityMutex);
		workerThread.reset();
	}

	HWND ownerWindow;
	Runtime *runtime;
	Callbacks callbacks;
	wil::com_ptr_nothrow<IUnknown> uiSite;
	std::atomic_bool active = true;
	std::atomic_bool interactivePriorityRequested = false;
	Status status = Status::Pending;
	std::optional<POINT> requestedShowPoint;
	std::optional<PreparedMenuResult> preparedMenuResultOpt;
	std::mutex priorityMutex;
	wil::unique_handle workerThread;
	int originalThreadPriority = THREAD_PRIORITY_ERROR_RETURN;

private:
	void ApplyCurrentPriorityPreference(HANDLE threadHandle)
	{
		int targetPriority = interactivePriorityRequested ? INTERACTIVE_THREAD_PRIORITY
														  : PRELOAD_THREAD_PRIORITY;
		SetThreadPriority(threadHandle, targetPriority);
	}

	void ShowPreparedOrFallback(const POINT &pt)
	{
		CHECK(preparedMenuResultOpt);

		auto preparedMenuResult = std::move(*preparedMenuResultOpt);
		preparedMenuResultOpt.reset();
		status = Status::Consumed;
		active = false;

		if (!preparedMenuResult.buildSucceeded)
		{
			callbacks.fallbackShowMenu(pt);
			return;
		}

		ShellContextMenu::PreparedMenu preparedMenu;
		preparedMenu.menu = std::move(preparedMenuResult.menu);

		if (preparedMenuResult.hasShellContextMenu)
		{
			preparedMenu.contextMenu =
				UnmarshalInterfaceFromStream<IContextMenu>(std::move(preparedMenuResult.contextMenuStream));

			if (!preparedMenu.contextMenu)
			{
				callbacks.fallbackShowMenu(pt);
				return;
			}
		}

		callbacks.showPreparedMenu(ownerWindow, pt, uiSite.get(), std::move(preparedMenu));
	}
};

class WorkerThreadPriorityScope
{
public:
	explicit WorkerThreadPriorityScope(const std::shared_ptr<LegacyContextMenuPreloader::State> &state) :
		m_state(state)
	{
		m_state->RegisterWorkerThread();
	}

	~WorkerThreadPriorityScope()
	{
		m_state->RestoreWorkerThreadPriority();
	}

private:
	const std::shared_ptr<LegacyContextMenuPreloader::State> m_state;
};

concurrencpp::null_result BuildLegacyContextMenuAsync(
	const std::shared_ptr<LegacyContextMenuPreloader::State> &state)
{
	auto marshaledSite = MarshalInterfaceToStream(state->uiSite.get());

	if (state->uiSite && !marshaledSite)
	{
		co_await ResumeOnUiThread(state->runtime);

		if (state->active)
		{
			state->OnBuildCompleted({});
		}

		co_return;
	}

	co_await ResumeOnComStaThread(state->runtime);

	if (!state->active)
	{
		co_return;
	}

	LegacyContextMenuPreloader::State::PreparedMenuResult preparedMenuResult;
	bool siteUnmarshalFailed = false;

	{
		WorkerThreadPriorityScope priorityScope(state);

		auto backgroundSite = UnmarshalInterfaceFromStream<IUnknown>(std::move(marshaledSite));

		if (state->uiSite && !backgroundSite)
		{
			siteUnmarshalFailed = true;
		}
		else
		{
			auto preparedMenu = state->callbacks.prepareMenu(state->ownerWindow, backgroundSite.get());
			preparedMenuResult.menu = std::move(preparedMenu.menu);
			preparedMenuResult.hasShellContextMenu = static_cast<bool>(preparedMenu.contextMenu);
			preparedMenuResult.buildSucceeded = static_cast<bool>(preparedMenuResult.menu);

			if (preparedMenu.contextMenu)
			{
				preparedMenuResult.contextMenuStream =
					MarshalInterfaceToStream(preparedMenu.contextMenu.get());
				preparedMenuResult.buildSucceeded = preparedMenuResult.buildSucceeded
					&& static_cast<bool>(preparedMenuResult.contextMenuStream);
			}
		}
	}

	co_await ResumeOnUiThread(state->runtime);

	if (!state->active)
	{
		co_return;
	}

	if (siteUnmarshalFailed)
	{
		state->OnBuildCompleted({});
		co_return;
	}

	state->OnBuildCompleted(std::move(preparedMenuResult));
}

LegacyContextMenuPreloader::LegacyContextMenuPreloader(HWND ownerWindow, Runtime *runtime,
	Callbacks callbacks, IUnknown *site) :
	m_state(std::make_shared<State>(ownerWindow, runtime, std::move(callbacks), site))
{
	BuildLegacyContextMenuAsync(m_state);
}

LegacyContextMenuPreloader::~LegacyContextMenuPreloader()
{
	Invalidate();
}

bool LegacyContextMenuPreloader::OnMoreActionsSelected(const POINT &pt)
{
	return m_state && m_state->OnMoreActionsSelected(pt);
}

void LegacyContextMenuPreloader::Invalidate()
{
	if (m_state)
	{
		m_state->Invalidate();
		m_state.reset();
	}
}
