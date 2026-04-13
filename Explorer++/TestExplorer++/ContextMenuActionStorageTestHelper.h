// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

namespace ContextMenuActions
{

class ContextMenuAction;
class ContextMenuActionModel;

bool operator==(const ContextMenuAction &first, const ContextMenuAction &second);

}

void BuildLoadSaveReferenceModel(ContextMenuActions::ContextMenuActionModel *model);
