// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

struct IXMLDOMDocument;
struct IXMLDOMNode;

namespace ContextMenuActions
{

class ContextMenuActionModel;

namespace Storage
{

void LoadFromXml(IXMLDOMNode *rootNode, ContextMenuActionModel *model);
void SaveToXml(IXMLDOMDocument *xmlDocument, IXMLDOMNode *rootNode,
	const ContextMenuActionModel *model);

void LoadFromRegistry(HKEY applicationKey, ContextMenuActionModel *model);
void SaveToRegistry(HKEY applicationKey, const ContextMenuActionModel *model);

}

}
