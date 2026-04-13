// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "pch.h"
#include "ContextMenuActionModel.h"
#include "ContextMenuActionStorage.h"
#include "ContextMenuActionStorageTestHelper.h"
#include "MovableModelHelper.h"
#include "RegistryStorageTestHelper.h"
#include <gtest/gtest.h>

using namespace ContextMenuActions;
using namespace testing;

class ContextMenuActionRegistryStorageTest : public RegistryStorageTest
{
};

TEST_F(ContextMenuActionRegistryStorageTest, SaveLoad)
{
	ContextMenuActionModel referenceModel;
	BuildLoadSaveReferenceModel(&referenceModel);

	Storage::SaveToRegistry(m_applicationTestKey.get(), &referenceModel);

	ContextMenuActionModel loadedModel;
	Storage::LoadFromRegistry(m_applicationTestKey.get(), &loadedModel);

	EXPECT_EQ(loadedModel, referenceModel);
}

TEST_F(ContextMenuActionRegistryStorageTest, SaveRemovesDeletedActions)
{
	ContextMenuActionModel referenceModel;
	BuildLoadSaveReferenceModel(&referenceModel);
	Storage::SaveToRegistry(m_applicationTestKey.get(), &referenceModel);

	ContextMenuActionModel updatedModel;
	updatedModel.AddItem(std::make_unique<ContextMenuAction>(L"Open terminal here",
		L"cmd.exe /K cd /d %directory%", true, false, true, false, false, true));

	Storage::SaveToRegistry(m_applicationTestKey.get(), &updatedModel);

	ContextMenuActionModel loadedModel;
	Storage::LoadFromRegistry(m_applicationTestKey.get(), &loadedModel);

	EXPECT_EQ(loadedModel, updatedModel);
}
