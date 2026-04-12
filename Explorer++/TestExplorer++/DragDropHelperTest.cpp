// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "pch.h"
#include "../Helper/DragDropHelper.h"
#include "../Helper/FileOperations.h"
#include "DragDropTestHelper.h"
#include "../Helper/DataObjectImpl.h"
#include "../Helper/WinRTBaseWrapper.h"
#include <gtest/gtest.h>

using namespace testing;

class PreferredDropEffectTestSuite : public TestWithParam<DWORD>
{
};

TEST_P(PreferredDropEffectTestSuite, PreferredDropEffect)
{
	wil::com_ptr_nothrow<IDataObject> dataObject;
	CreateShellDataObject(L"C:\\fake", ShellItemType::File, dataObject);

	DWORD assignedEffect = GetParam();
	EXPECT_HRESULT_SUCCEEDED(SetPreferredDropEffect(dataObject.get(), assignedEffect));

	DWORD retrievedEffect = DROPEFFECT_NONE;
	EXPECT_HRESULT_SUCCEEDED(GetPreferredDropEffect(dataObject.get(), retrievedEffect));

	EXPECT_EQ(retrievedEffect, assignedEffect);
}

INSTANTIATE_TEST_SUITE_P(CopyAndMoveEffects, PreferredDropEffectTestSuite,
	Values(DROPEFFECT_COPY, DROPEFFECT_MOVE));

TEST(FileOperationsTest, TransferOperationFlagsRequireElevation)
{
	EXPECT_EQ(::FileOperations::GetTransferOperationFlags(),
		FOF_ALLOWUNDO | FOFX_REQUIREELEVATION);
}

TEST(FileOperationsTest, DeleteOperationFlagsRequireElevation)
{
	EXPECT_EQ(::FileOperations::GetDeleteOperationFlags(false, false),
		FOF_ALLOWUNDO | FOF_WANTNUKEWARNING | FOFX_REQUIREELEVATION);
	EXPECT_EQ(::FileOperations::GetDeleteOperationFlags(true, false),
		FOF_WANTNUKEWARNING | FOFX_REQUIREELEVATION);
}

TEST(FileOperationsTest, SilentDeleteOperationFlagsStillAllowElevationPrompt)
{
	EXPECT_EQ(::FileOperations::GetDeleteOperationFlags(true, true),
		FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOFX_REQUIREELEVATION
			| FOFX_SHOWELEVATIONPROMPT);
}

TEST(FileOperationsTest, GetTransferActionDefaultsToCopy)
{
	wil::com_ptr_nothrow<IDataObject> dataObject;
	CreateShellDataObject(L"C:\\fake", ShellItemType::File, dataObject);

	EXPECT_EQ(::FileOperations::GetTransferActionForDataObject(dataObject.get()),
		TransferAction::Copy);
}

TEST(FileOperationsTest, GetTransferActionUsesPreferredDropEffect)
{
	wil::com_ptr_nothrow<IDataObject> dataObject;
	CreateShellDataObject(L"C:\\fake", ShellItemType::File, dataObject);
	ASSERT_HRESULT_SUCCEEDED(SetPreferredDropEffect(dataObject.get(), DROPEFFECT_MOVE));

	EXPECT_EQ(::FileOperations::GetTransferActionForDataObject(dataObject.get()),
		TransferAction::Move);
}

TEST(DragDropHelperTest, GetSetTextOnDataObject)
{
	auto dataObject = winrt::make<DataObjectImpl>();

	std::wstring text = L"Test text";
	ASSERT_HRESULT_SUCCEEDED(SetTextOnDataObject(dataObject.get(), text));

	std::wstring retrievedText;
	ASSERT_HRESULT_SUCCEEDED(GetTextFromDataObject(dataObject.get(), retrievedText));
	EXPECT_EQ(retrievedText, text);
}
