// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "ColorRuleModelFactory.h"
#include "ColorRule.h"
#include "ColorRuleModel.h"
#include "GitStatusTracker.h"

namespace
{

constexpr COLORREF GIT_MODIFIED_COLOR = RGB(255, 140, 0);
constexpr COLORREF GIT_STAGED_COLOR = RGB(0, 128, 0);
constexpr COLORREF GIT_UNTRACKED_COLOR = RGB(220, 20, 60);
constexpr COLORREF GIT_IGNORED_COLOR = RGB(128, 128, 128);

}

std::unique_ptr<ColorRuleModel> ColorRuleModelFactory::Create()
{
	auto colorRuleModel = std::make_unique<ColorRuleModel>();

	colorRuleModel->AddItem(std::make_unique<ColorRule>(
		L"Git modified", L"", false, 0, GIT_MODIFIED_COLOR, GitStatus::Modified));

	colorRuleModel->AddItem(std::make_unique<ColorRule>(
		L"Git staged", L"", false, 0, GIT_STAGED_COLOR, GitStatus::Staged));

	colorRuleModel->AddItem(std::make_unique<ColorRule>(
		L"Git untracked", L"", false, 0, GIT_UNTRACKED_COLOR, GitStatus::Untracked));

	colorRuleModel->AddItem(std::make_unique<ColorRule>(
		L"Git ignored", L"", false, 0, GIT_IGNORED_COLOR, GitStatus::Ignored));

	return colorRuleModel;
}
