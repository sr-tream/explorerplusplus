// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "pch.h"
#include "ContextMenuActionModel.h"
#include "ContextMenuActionStorage.h"
#include "ContextMenuActionStorageTestHelper.h"
#include "MovableModelHelper.h"
#include "XmlStorageTestHelper.h"
#include <gtest/gtest.h>

using namespace ContextMenuActions;
using namespace testing;

class ContextMenuActionXmlStorageTest : public XmlStorageTest
{
};

TEST_F(ContextMenuActionXmlStorageTest, SaveLoad)
{
	ContextMenuActionModel referenceModel;
	BuildLoadSaveReferenceModel(&referenceModel);

	auto xmlDocumentData = CreateXmlDocument();

	Storage::SaveToXml(xmlDocumentData.xmlDocument.get(), xmlDocumentData.rootNode.get(),
		&referenceModel);

	ContextMenuActionModel loadedModel;
	Storage::LoadFromXml(xmlDocumentData.rootNode.get(), &loadedModel);

	EXPECT_EQ(loadedModel, referenceModel);
}
