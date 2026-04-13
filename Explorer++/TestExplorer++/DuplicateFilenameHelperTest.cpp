// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "pch.h"
#include "../Explorer++/DuplicateFilenameHelper.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <unordered_set>

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

TEST(BuildDuplicateNameTest, InsertsSuffixBeforeExtension)
{
	EXPECT_EQ(BuildDuplicateName(L"foo.txt", 2), L"foo (2).txt");
}

TEST(BuildDuplicateNameTest, ReusesBaseNameForExistingDuplicate)
{
	EXPECT_EQ(BuildDuplicateName(L"foo (1).txt", 2), L"foo (2).txt");
}

TEST(BuildDuplicateNameTest, HandlesFolderNames)
{
	EXPECT_EQ(BuildDuplicateName(L"Photos", 3), L"Photos (3)");
}

TEST(FindNextAvailableDuplicateNameTest, SkipsExistingAndReservedNames)
{
	namespace fs = std::filesystem;

	auto testDirectory = fs::temp_directory_path()
		/ (std::wstring(L"explorerpp-duplicate-helper-")
			+ std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
	fs::create_directories(testDirectory);

	try
	{
		std::ofstream(testDirectory / "foo.txt").put('\n');
		std::ofstream(testDirectory / "foo (1).txt").put('\n');

		std::unordered_set<std::wstring> reservedNames = { L"foo (2).txt" };
		auto duplicateName =
			FindNextAvailableDuplicateName(L"foo.txt", testDirectory.wstring(), reservedNames);

		EXPECT_EQ(duplicateName, L"foo (3).txt");
		EXPECT_TRUE(reservedNames.contains(L"foo (3).txt"));
	}
	catch (...)
	{
		fs::remove_all(testDirectory);
		throw;
	}

	fs::remove_all(testDirectory);
}
