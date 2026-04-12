// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "pch.h"
#include "../Explorer++/DuplicateFilenameHelper.h"
#include <gtest/gtest.h>

using namespace testing;

TEST(StripDuplicateSuffixTest, BasicSuffix)
{
	auto result = StripDuplicateSuffix(L"foo (1).txt");
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, L"foo.txt");
}

TEST(StripDuplicateSuffixTest, MultiDigitSuffix)
{
	auto result = StripDuplicateSuffix(L"document (23).pdf");
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, L"document.pdf");
}

TEST(StripDuplicateSuffixTest, NestedParentheses)
{
	// Only the last " (N)" is stripped.
	auto result = StripDuplicateSuffix(L"foo (1) (2).txt");
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, L"foo (1).txt");
}

TEST(StripDuplicateSuffixTest, NoSuffix)
{
	auto result = StripDuplicateSuffix(L"foo.txt");
	EXPECT_FALSE(result.has_value());
}

TEST(StripDuplicateSuffixTest, NonDigitParens)
{
	auto result = StripDuplicateSuffix(L"foo (bar).txt");
	EXPECT_FALSE(result.has_value());
}

TEST(StripDuplicateSuffixTest, EmptyParens)
{
	auto result = StripDuplicateSuffix(L"foo ().txt");
	EXPECT_FALSE(result.has_value());
}

TEST(StripDuplicateSuffixTest, NoExtension)
{
	auto result = StripDuplicateSuffix(L"readme (1)");
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, L"readme");
}

TEST(StripDuplicateSuffixTest, MultipleDotsInExtension)
{
	auto result = StripDuplicateSuffix(L"archive.tar (1).gz");
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, L"archive.tar.gz");
}

TEST(StripDuplicateSuffixTest, NoSpaceBeforeParen)
{
	auto result = StripDuplicateSuffix(L"foo(1).txt");
	EXPECT_FALSE(result.has_value());
}

TEST(StripDuplicateSuffixTest, OnlySuffix)
{
	// Name consists of only the suffix pattern (no base name).
	auto result = StripDuplicateSuffix(L" (1).txt");
	EXPECT_FALSE(result.has_value());
}

TEST(StripDuplicateSuffixTest, SingleCharName)
{
	auto result = StripDuplicateSuffix(L"a (1).txt");
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, L"a.txt");
}

TEST(StripDuplicateSuffixTest, LargeNumber)
{
	auto result = StripDuplicateSuffix(L"report (999).docx");
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, L"report.docx");
}

TEST(StripDuplicateSuffixTest, SpacesInBaseName)
{
	auto result = StripDuplicateSuffix(L"my document (1).txt");
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, L"my document.txt");
}

TEST(StripDuplicateSuffixTest, MixedContent)
{
	auto result = StripDuplicateSuffix(L"foo (bar) (3).txt");
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, L"foo (bar).txt");
}
