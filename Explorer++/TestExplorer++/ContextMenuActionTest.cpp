// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "pch.h"
#include "ContextMenuAction.h"
#include <gtest/gtest.h>
#include <shellapi.h>

using namespace ContextMenuActions;

namespace
{

InvocationContext BuildItemContext(const std::vector<std::wstring> &selectedItemPaths,
	int numSelectedFiles, int numSelectedFolders)
{
	InvocationContext context;
	context.menuType = MenuType::Item;
	context.directory = L"C:\\Temp";
	context.selectedItemPaths = selectedItemPaths;
	context.numSelectedFiles = numSelectedFiles;
	context.numSelectedFolders = numSelectedFolders;
	return context;
}

InvocationContext BuildBackgroundContext()
{
	InvocationContext context;
	context.menuType = MenuType::Background;
	context.directory = L"C:\\Temp";
	return context;
}

}

TEST(ContextMenuActionTest, BackgroundActionUsesBackgroundVisibility)
{
	ContextMenuAction action(L"Open terminal here", L"cmd.exe /K cd /d %directory%", true, false,
		true, false, false, true);

	EXPECT_TRUE(IsApplicable(action, BuildBackgroundContext()));

	action.SetShowInBackgroundMenu(false);

	EXPECT_FALSE(IsApplicable(action, BuildBackgroundContext()));
}

TEST(ContextMenuActionTest, MultipleSelectionRestrictionIsHonored)
{
	ContextMenuAction action(L"Open in VS Code", L"code.exe %path%", true, true, false, true, false,
		false);

	auto context =
		BuildItemContext({ L"C:\\Temp\\readme.txt", L"C:\\Temp\\notes.txt" }, 2, 0);

	EXPECT_FALSE(IsApplicable(action, context));
}

TEST(ContextMenuActionTest, FolderVisibilityIsHonoredForMixedSelection)
{
	ContextMenuAction action(L"Open in VS Code", L"code.exe %path%", true, true, false, true, false,
		true);

	auto context =
		BuildItemContext({ L"C:\\Temp\\readme.txt", L"C:\\Temp\\Docs" }, 1, 1);

	EXPECT_FALSE(IsApplicable(action, context));
}

TEST(ContextMenuActionTest, FileExtensionsAreNormalizedBeforeMatching)
{
	ContextMenuAction action(L"Open in VS Code", L"code.exe %path%", true, true, false, true, false,
		true, L"txt; Md");

	auto matchingContext =
		BuildItemContext({ L"C:\\Temp\\README.TXT", L"C:\\Temp\\notes.md" }, 2, 0);
	auto nonMatchingContext =
		BuildItemContext({ L"C:\\Temp\\README.TXT", L"C:\\Temp\\image.png" }, 2, 0);

	EXPECT_TRUE(IsApplicable(action, matchingContext));
	EXPECT_FALSE(IsApplicable(action, nonMatchingContext));
}

TEST(ContextMenuActionTest, FileExtensionsCanBeSeparatedByWhitespace)
{
	ContextMenuAction action(L"Open in IDA", L"ida.exe %path%", true, true, false, true, false,
		true, L".dll .dbg .so .exe");

	auto matchingContext = BuildItemContext(
		{ L"C:\\Temp\\plugin.dll", L"C:\\Temp\\symbols.DBG", L"C:\\Temp\\lib.so",
			L"C:\\Temp\\tool.EXE" },
		4, 0);
	auto nonMatchingContext =
		BuildItemContext({ L"C:\\Temp\\plugin.dll", L"C:\\Temp\\notes.txt" }, 2, 0);

	EXPECT_TRUE(IsApplicable(action, matchingContext));
	EXPECT_FALSE(IsApplicable(action, nonMatchingContext));
}

TEST(ContextMenuActionTest, ComplexFileExtensionsMatchFullSuffixes)
{
	ContextMenuAction action(L"Extract", L"extract.exe %path%", true, true, false, true, false,
		true, L".tar.gz");

	auto matchingContext = BuildItemContext({ L"C:\\Temp\\archive.TAR.GZ" }, 1, 0);
	auto nonMatchingContext = BuildItemContext({ L"C:\\Temp\\archive.gz" }, 1, 0);

	EXPECT_TRUE(IsApplicable(action, matchingContext));
	EXPECT_FALSE(IsApplicable(action, nonMatchingContext));
}

TEST(ContextMenuActionTest, ExcludedFileExtensionsHideMatchingFiles)
{
	ContextMenuAction action(L"Compress", L"compress.exe %path%", true, true, false, true, false,
		true, L"!.zip !.7z !.tar.gz");

	auto matchingContext = BuildItemContext({ L"C:\\Temp\\readme.txt" }, 1, 0);
	auto nonMatchingContext = BuildItemContext({ L"C:\\Temp\\archive.TAR.GZ" }, 1, 0);

	EXPECT_TRUE(IsApplicable(action, matchingContext));
	EXPECT_FALSE(IsApplicable(action, nonMatchingContext));
}

TEST(ContextMenuActionTest, ExcludedFileExtensionsOverrideIncludedExtensions)
{
	ContextMenuAction action(L"Process archive", L"tool.exe %path%", true, true, false, true, false,
		true, L".zip !.part.zip");

	auto matchingContext = BuildItemContext({ L"C:\\Temp\\archive.zip" }, 1, 0);
	auto nonMatchingContext = BuildItemContext({ L"C:\\Temp\\archive.part.zip" }, 1, 0);

	EXPECT_TRUE(IsApplicable(action, matchingContext));
	EXPECT_FALSE(IsApplicable(action, nonMatchingContext));
}

TEST(ContextMenuActionTest, BuildCommandLineAutomaticallyPassesSelectedPaths)
{
	ContextMenuAction action(L"Open in IDA",
		L"\"C:\\Users\\SR_team\\Projects\\dotfiles\\utils\\Projects\\_soft\\IDA Pro 8.3\\ida64.exe\"");

	auto context = BuildItemContext(
		{ L"C:\\Temp\\My File.exe", L"C:\\Temp\\Second File.dll" }, 2, 0);

	EXPECT_EQ(BuildCommandLine(action, context),
		L"\"C:\\Users\\SR_team\\Projects\\dotfiles\\utils\\Projects\\_soft\\IDA Pro 8.3\\ida64.exe\" "
		L"\"C:\\Temp\\My File.exe\" \"C:\\Temp\\Second File.dll\"");
}

TEST(ContextMenuActionTest, BuildCommandLineDoesNotAppendAutomaticPathsWhenParametersExist)
{
	ContextMenuAction action(L"Open in IDA",
		L"\"C:\\Users\\SR_team\\Projects\\dotfiles\\utils\\Projects\\_soft\\IDA Pro 8.3\\ida64.exe\" -A");

	auto context = BuildItemContext({ L"C:\\Temp\\My File.exe" }, 1, 0);

	EXPECT_EQ(BuildCommandLine(action, context), action.GetCommand());
}

TEST(ContextMenuActionTest, BuildCommandLineEscapesAutomaticPathArguments)
{
	ContextMenuAction action(L"Open terminal", L"\"C:\\Tools\\terminal.exe\"", true, false, true,
		false, false, true);

	auto context = BuildBackgroundContext();
	context.directory = L"C:\\Temp\\Folder With Space\\";

	auto commandLine = BuildCommandLine(action, context);
	int numArguments = 0;
	auto *arguments = CommandLineToArgvW(commandLine.c_str(), &numArguments);

	ASSERT_NE(arguments, nullptr);
	ASSERT_EQ(numArguments, 2);
	EXPECT_EQ(arguments[0], std::wstring(L"C:\\Tools\\terminal.exe"));
	EXPECT_EQ(arguments[1], context.directory);

	LocalFree(arguments);
}
