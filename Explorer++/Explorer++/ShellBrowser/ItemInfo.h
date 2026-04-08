// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include "../Helper/Pidl.h"
#include <stop_token>
#include <string>
#include <vector>

struct ItemInfo_t
{
	PidlAbsolute pidlComplete;
	PidlChild pridl;
	WIN32_FIND_DATA wfd;
	bool isFindDataValid;
	std::wstring parsingName;
	std::wstring displayName;
	std::wstring editingName;

	/* These are only used for drives. They are
	needed for when a drive is removed from the
	system, in which case the drive name is needed
	so that the removed drive can be found. */
	BOOL bDrive;
	TCHAR szDrive[4];

	/* Used for temporary sorting in details mode (i.e.
	when items need to be rearranged). */
	int iRelativeSort;

	ItemInfo_t() : wfd({}), isFindDataValid(false), bDrive(FALSE)
	{
	}
};

// Retrieves display information for a single shell item. Safe to call from any COM STA thread.
std::optional<ItemInfo_t> RetrieveItemInformation(IShellFolder *shellFolder,
	PCIDLIST_ABSOLUTE pidlDirectory, PCITEMID_CHILD pidlChild);

// Retrieves display information for a batch of shell items. Performs SHBindToObject internally.
// Safe to call from any COM STA thread. Checks stopToken between items for cancellation.
std::vector<ItemInfo_t> RetrieveItemInformationFromPidls(PCIDLIST_ABSOLUTE pidlDirectory,
	const std::vector<PidlChild> &itemPidls, std::stop_token stopToken);
