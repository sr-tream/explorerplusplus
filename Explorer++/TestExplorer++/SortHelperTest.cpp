// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "pch.h"
#include <shlobj.h>
#include "ShellBrowser/SortHelper.h"
#include <gtest/gtest.h>

using namespace testing;

TEST(SortHelperTest, FilesystemFoldersAlwaysSortFirst)
{
	EXPECT_TRUE(ShouldSortFoldersFirst(false, false, false));
	EXPECT_TRUE(ShouldSortFoldersFirst(false, false, true));
}

TEST(SortHelperTest, VirtualFoldersStillHonorMixedOrderSetting)
{
	EXPECT_TRUE(ShouldSortFoldersFirst(true, false, false));
	EXPECT_FALSE(ShouldSortFoldersFirst(true, false, true));
}

TEST(SortHelperTest, RecycleBinDoesNotForceFoldersFirst)
{
	EXPECT_FALSE(ShouldSortFoldersFirst(false, true, false));
	EXPECT_FALSE(ShouldSortFoldersFirst(true, true, false));
	EXPECT_FALSE(ShouldSortFoldersFirst(true, true, true));
}

TEST(SortHelperTest, DescendingSortDoesNotReverseFolderSeparation)
{
	EXPECT_FALSE(
		ShouldReverseSortComparison(SortDirection::Descending, true, true, false));
	EXPECT_FALSE(
		ShouldReverseSortComparison(SortDirection::Descending, true, false, true));
}

TEST(SortHelperTest, DescendingSortStillReversesWithinSameItemType)
{
	EXPECT_TRUE(
		ShouldReverseSortComparison(SortDirection::Descending, true, true, true));
	EXPECT_TRUE(
		ShouldReverseSortComparison(SortDirection::Descending, true, false, false));
}
