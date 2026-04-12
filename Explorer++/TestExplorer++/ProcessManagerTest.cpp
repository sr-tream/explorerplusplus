// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "pch.h"
#include "ProcessManager.h"
#include "BrowserTestBase.h"
#include "BrowserWindowFake.h"
#include "BrowserList.h"
#include "BrowserWindowMock.h"
#include "CommandLine.h"
#include "Config.h"
#include "PidlTestHelper.h"
#include "ShellView.h"
#include "ShellBrowser/ShellBrowser.h"
#include "../Helper/Helper.h"
#include <fstream>
#include <gtest/gtest.h>

using namespace testing;

class ProcessManagerTest : public Test
{
protected:
	struct ProcessData
	{
		BrowserList browserList;
		ProcessManager processManager;
		CommandLine::Settings settings;
		Config config;

		ProcessData() : processManager(&browserList)
		{
		}

		bool Initialize(const std::wstring &windowName = CreateUniqueWindowName())
		{
			return processManager.InitializeCurrentProcess(&settings, &config, windowName);
		}
	};

	static std::wstring CreateUniqueWindowName()
	{
		// ProcessManager will create a message-only window. To find an existing process, it will
		// search for an existing message-only window that has the appropriate class name and window
		// name. A random value will be returned here to ensure that each test runs correctly, even
		// if Explorer++, or another test is currently running.
		return CreateGUID();
	}
};

TEST_F(ProcessManagerTest, NoExistingProcess)
{
	// allowMultipleInstances isn't set, but there are no other processes, so this initialization
	// should succeed.
	ProcessData processData;
	processData.config.allowMultipleInstances = false;
	auto res = processData.Initialize();
	EXPECT_TRUE(res);
}

TEST_F(ProcessManagerTest, ExistingProcess)
{
	auto windowName = CreateGUID();

	ProcessData processData1;
	auto res = processData1.Initialize(windowName);
	ASSERT_TRUE(res);

	ProcessData processData2;
	processData2.config.allowMultipleInstances = false;
	res = processData2.Initialize(windowName);
	EXPECT_FALSE(res);
}

TEST_F(ProcessManagerTest, ExistingProcessOpenDefaultDirectory)
{
	auto windowName = CreateGUID();

	ProcessData processData1;
	auto res = processData1.Initialize(windowName);
	ASSERT_TRUE(res);

	BrowserWindowMock browser;
	processData1.browserList.AddBrowser(&browser);

	// There are no directories specified in the command line settings for the second process, so
	// the first process should attempt to open a tab in the default directory.
	EXPECT_CALL(browser, OpenDefaultItem(OpenFolderDisposition::ForegroundTab));
	EXPECT_CALL(browser, Activate());
	EXPECT_CALL(browser, FocusActiveTab());

	ProcessData processData2;
	processData2.config.allowMultipleInstances = false;
	res = processData2.Initialize(windowName);
	EXPECT_FALSE(res);
}

TEST_F(ProcessManagerTest, ExistingProcessOpenDirectories)
{
	auto windowName = CreateGUID();

	ProcessData processData1;
	auto res = processData1.Initialize(windowName);
	ASSERT_TRUE(res);

	std::vector<std::wstring> directories = { L"c:\\", L"c:\\windows", L"c:\\users\\default" };

	BrowserWindowMock browser;
	processData1.browserList.AddBrowser(&browser);

	InSequence sequence;

	for (const auto &directory : directories)
	{
		EXPECT_CALL(browser, SelectTabByPath(directory)).WillOnce(Return(false));
		EXPECT_CALL(browser, OpenItem(directory, OpenFolderDisposition::ForegroundTab));
		EXPECT_CALL(browser, Activate());
		EXPECT_CALL(browser, FocusActiveTab());
	}

	ProcessData processData2;
	processData2.settings.directories = directories;
	processData2.config.allowMultipleInstances = false;
	res = processData2.Initialize(windowName);
	EXPECT_FALSE(res);
}

TEST_F(ProcessManagerTest, ExistingProcessSelectsExistingTabForDirectory)
{
	auto windowName = CreateGUID();

	ProcessData processData1;
	auto res = processData1.Initialize(windowName);
	ASSERT_TRUE(res);

	std::wstring directory = L"shell:Downloads";

	BrowserWindowMock browser;
	processData1.browserList.AddBrowser(&browser);

	{
		InSequence sequence;
		EXPECT_CALL(browser, SelectTabByPath(directory)).WillOnce(Return(true));
		EXPECT_CALL(browser, Activate());
		EXPECT_CALL(browser, FocusActiveTab());
	}

	EXPECT_CALL(browser, OpenItem(A<const std::wstring &>(), _)).Times(0);

	ProcessData processData2;
	processData2.settings.directories = { directory };
	processData2.config.allowMultipleInstances = false;
	res = processData2.Initialize(windowName);
	EXPECT_FALSE(res);
}

TEST_F(ProcessManagerTest, ExistingProcessShowsItemsInFolder)
{
	auto windowName = CreateGUID();

	ProcessData processData1;
	auto res = processData1.Initialize(windowName);
	ASSERT_TRUE(res);

	std::wstring itemPath = L"c:\\windows\\notepad.exe";

	BrowserWindowMock browser;
	processData1.browserList.AddBrowser(&browser);

	{
		InSequence sequence;
		EXPECT_CALL(browser, ShowItemInFolder(itemPath, OpenFolderDisposition::ForegroundTab))
			.WillOnce(Return(true));
		EXPECT_CALL(browser, Activate());
		EXPECT_CALL(browser, FocusActiveTab());
	}

	EXPECT_CALL(browser, OpenItem(A<const std::wstring &>(), _)).Times(0);
	EXPECT_CALL(browser, OpenDefaultItem(_)).Times(0);

	ProcessData processData2;
	processData2.settings.filesToSelect = { itemPath };
	processData2.config.allowMultipleInstances = false;
	res = processData2.Initialize(windowName);
	EXPECT_FALSE(res);
}

class BrowserWindowSelectionTest : public BrowserTestBase
{
};

TEST_F(BrowserWindowSelectionTest, ShellViewFallsBackWhenShellBrowserIsDestroyed)
{
	auto *browser = AddBrowser();

	auto tempDirectory = std::filesystem::temp_directory_path() / std::filesystem::path(CreateGUID());
	std::filesystem::create_directories(tempDirectory);

	auto cleanup = wil::scope_exit(
		[&tempDirectory]
		{
			std::error_code error;
			std::filesystem::remove_all(tempDirectory, error);
		});

	auto tempFile = tempDirectory / L"selected-item.txt";
	std::ofstream tempFileStream(tempFile);
	tempFileStream << "test";
	tempFileStream.close();

	browser->AddTab(tempDirectory.wstring(), { .selected = true });
	browser->AddTab(L"c:\\windows", { .selected = false });
	browser->GetActiveTabContainer()->SelectTabAtIndex(1);

	auto directoryPidl = CreateSimplePidlForTest(tempDirectory.wstring());
	auto itemPidl = CreateSimplePidlForTest(tempFile.wstring());
	auto shellView = winrt::make_self<ShellView>(WeakPtr<ShellBrowserImpl>(), &m_browserList,
		browser->GetId(), directoryPidl.Raw(), true);

	HRESULT hr = shellView->SelectItem(ILFindLastID(itemPidl.Raw()),
		static_cast<SVSIF>(SVSI_SELECT | SVSI_FOCUSED | SVSI_ENSUREVISIBLE));
	ASSERT_HRESULT_SUCCEEDED(hr);

	auto selectedPath = GetDisplayNameWithFallback(
		browser->GetActiveTabContainer()->GetSelectedTab().GetShellBrowser()->GetDirectory().Raw(),
		SHGDN_FORPARSING);
	EXPECT_THAT(selectedPath, StrCaseEq(tempDirectory.wstring()));
}

TEST_F(BrowserWindowSelectionTest, SelectTabByPathResolvesShellKnownFolder)
{
	auto *browser = AddBrowser();

	wil::unique_cotaskmem_string downloadsPath;
	HRESULT hr = SHGetKnownFolderPath(FOLDERID_Downloads, KF_FLAG_DEFAULT, nullptr, &downloadsPath);
	ASSERT_HRESULT_SUCCEEDED(hr);

	browser->AddTab(downloadsPath.get(), { .selected = true });
	browser->AddTab(L"c:\\windows", { .selected = false });
	browser->GetActiveTabContainer()->SelectTabAtIndex(1);

	EXPECT_TRUE(browser->SelectTabByPath(L"shell:Downloads"));

	auto selectedPath = GetDisplayNameWithFallback(
		browser->GetActiveTabContainer()->GetSelectedTab().GetShellBrowser()->GetDirectory().Raw(),
		SHGDN_FORPARSING);
	EXPECT_THAT(selectedPath, StrCaseEq(downloadsPath.get()));
}

TEST_F(BrowserWindowSelectionTest, ShowItemInFolderReusesExistingTab)
{
	auto *browser = AddBrowser();

	auto tempDirectory = std::filesystem::temp_directory_path() / std::filesystem::path(CreateGUID());
	std::filesystem::create_directories(tempDirectory);

	auto cleanup = wil::scope_exit(
		[&tempDirectory]
		{
			std::error_code error;
			std::filesystem::remove_all(tempDirectory, error);
		});

	auto tempFile = tempDirectory / L"selected-item.txt";
	std::ofstream tempFileStream(tempFile);
	tempFileStream << "test";
	tempFileStream.close();

	browser->AddTab(tempDirectory.wstring(), { .selected = true });
	browser->AddTab(L"c:\\windows", { .selected = false });
	browser->GetActiveTabContainer()->SelectTabAtIndex(1);

	EXPECT_TRUE(browser->ShowItemInFolder(tempFile.wstring(), OpenFolderDisposition::ForegroundTab));
	EXPECT_EQ(browser->GetActiveTabContainer()->GetNumTabs(), 2);

	auto selectedPath = GetDisplayNameWithFallback(
		browser->GetActiveTabContainer()->GetSelectedTab().GetShellBrowser()->GetDirectory().Raw(),
		SHGDN_FORPARSING);
	EXPECT_THAT(selectedPath, StrCaseEq(tempDirectory.wstring()));
}
