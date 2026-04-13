// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include "../../Helper/ShellContextMenu.h"
#include <functional>
#include <memory>
#include <wil/com.h>

class Runtime;

class LegacyContextMenuPreloader
{
public:
	using PrepareMenuCallback = std::function<ShellContextMenu::PreparedMenu(HWND, IUnknown *)>;
	using ShowPreparedMenuCallback = std::function<void(HWND, const POINT &, IUnknown *,
		ShellContextMenu::PreparedMenu)>;
	using FallbackShowMenuCallback = std::function<void(const POINT &)>;

	struct Callbacks
	{
		PrepareMenuCallback prepareMenu;
		ShowPreparedMenuCallback showPreparedMenu;
		FallbackShowMenuCallback fallbackShowMenu;
	};

	LegacyContextMenuPreloader(HWND ownerWindow, Runtime *runtime, Callbacks callbacks,
		IUnknown *site = nullptr);
	~LegacyContextMenuPreloader();

	bool OnMoreActionsSelected(const POINT &pt);
	void Invalidate();

private:
public:
	struct State;

private:
	std::shared_ptr<State> m_state;
};
