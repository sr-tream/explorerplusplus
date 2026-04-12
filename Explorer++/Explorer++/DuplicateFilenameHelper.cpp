// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "DuplicateFilenameHelper.h"
#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <map>
#include <shobjidl.h>
#include <wil/com.h>

std::optional<std::wstring> StripDuplicateSuffix(const std::wstring &filename)
{
	namespace fs = std::filesystem;
	fs::path filePath(filename);

	auto stem = filePath.stem().wstring();
	auto ext = filePath.extension().wstring();

	if (stem.size() < 4)
	{
		return std::nullopt;
	}

	if (stem.back() != L')')
	{
		return std::nullopt;
	}

	auto openPos = stem.rfind(L" (");

	if (openPos == std::wstring::npos || openPos == 0)
	{
		return std::nullopt;
	}

	auto digitStart = openPos + 2;
	auto digitEnd = stem.size() - 1;

	if (digitStart >= digitEnd)
	{
		return std::nullopt;
	}

	for (size_t i = digitStart; i < digitEnd; i++)
	{
		if (!std::iswdigit(stem[i]))
		{
			return std::nullopt;
		}
	}

	auto baseStem = stem.substr(0, openPos);
	return baseStem + ext;
}

std::vector<DuplicateFileMatch> FindDuplicateMatches(
	const std::vector<std::wstring> &sourceFiles, const std::wstring &destinationDir)
{
	namespace fs = std::filesystem;

	std::vector<DuplicateFileMatch> matches;
	std::map<std::wstring, size_t> targetToIndex;

	for (const auto &sourcePath : sourceFiles)
	{
		fs::path sourceFilePath(sourcePath);

		std::error_code ec;

		if (fs::is_directory(sourceFilePath, ec))
		{
			continue;
		}

		auto filename = sourceFilePath.filename().wstring();
		auto baseName = StripDuplicateSuffix(filename);

		if (!baseName)
		{
			continue;
		}

		fs::path existingPath = fs::path(destinationDir) / *baseName;

		if (!fs::exists(existingPath, ec))
		{
			continue;
		}

		// Detect ambiguous case: multiple sources mapping to the same target.
		std::wstring lowerTarget = *baseName;
		std::transform(lowerTarget.begin(), lowerTarget.end(), lowerTarget.begin(), ::towlower);

		if (targetToIndex.count(lowerTarget))
		{
			return {};
		}

		targetToIndex[lowerTarget] = matches.size();
		matches.push_back({ sourcePath, *baseName, existingPath.wstring() });
	}

	return matches;
}

HRESULT PerformDuplicateReplace(HWND hwnd, const std::vector<std::wstring> &sourceFiles,
	const std::vector<DuplicateFileMatch> &matches, const std::wstring &destinationDir,
	bool isMove)
{
	// Build a lookup of matched source paths (case-insensitive) to their target names.
	std::map<std::wstring, std::wstring> matchedFiles;

	for (const auto &match : matches)
	{
		std::wstring lowerSource = match.sourcePath;
		std::transform(lowerSource.begin(), lowerSource.end(), lowerSource.begin(), ::towlower);
		matchedFiles[lowerSource] = match.targetName;
	}

	wil::com_ptr_nothrow<IShellItem> destFolder;
	RETURN_IF_FAILED(
		SHCreateItemFromParsingName(destinationDir.c_str(), nullptr, IID_PPV_ARGS(&destFolder)));

	// Batch 1: Matched files. Use FOF_NOCONFIRMATION since the user already confirmed replacement.
	if (!matches.empty())
	{
		wil::com_ptr_nothrow<IFileOperation> fo;
		RETURN_IF_FAILED(
			CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&fo)));
		RETURN_IF_FAILED(fo->SetOwnerWindow(hwnd));
		RETURN_IF_FAILED(fo->SetOperationFlags(FOF_ALLOWUNDO | FOF_NOCONFIRMATION));

		for (const auto &match : matches)
		{
			wil::com_ptr_nothrow<IShellItem> sourceItem;
			RETURN_IF_FAILED(SHCreateItemFromParsingName(match.sourcePath.c_str(), nullptr,
				IID_PPV_ARGS(&sourceItem)));

			if (isMove)
			{
				RETURN_IF_FAILED(fo->MoveItem(sourceItem.get(), destFolder.get(),
					match.targetName.c_str(), nullptr));
			}
			else
			{
				RETURN_IF_FAILED(fo->CopyItem(sourceItem.get(), destFolder.get(),
					match.targetName.c_str(), nullptr));
			}
		}

		RETURN_IF_FAILED(fo->PerformOperations());
	}

	// Batch 2: Unmatched files. Normal flags so Windows shows its overwrite dialog if needed.
	std::vector<std::wstring> unmatchedFiles;

	for (const auto &sourcePath : sourceFiles)
	{
		std::wstring lowerSource = sourcePath;
		std::transform(lowerSource.begin(), lowerSource.end(), lowerSource.begin(), ::towlower);

		if (matchedFiles.count(lowerSource) == 0)
		{
			unmatchedFiles.push_back(sourcePath);
		}
	}

	if (!unmatchedFiles.empty())
	{
		wil::com_ptr_nothrow<IFileOperation> fo;
		RETURN_IF_FAILED(
			CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&fo)));
		RETURN_IF_FAILED(fo->SetOwnerWindow(hwnd));
		RETURN_IF_FAILED(fo->SetOperationFlags(FOF_ALLOWUNDO));

		for (const auto &sourcePath : unmatchedFiles)
		{
			wil::com_ptr_nothrow<IShellItem> sourceItem;
			RETURN_IF_FAILED(SHCreateItemFromParsingName(sourcePath.c_str(), nullptr,
				IID_PPV_ARGS(&sourceItem)));

			if (isMove)
			{
				RETURN_IF_FAILED(
					fo->MoveItem(sourceItem.get(), destFolder.get(), nullptr, nullptr));
			}
			else
			{
				RETURN_IF_FAILED(
					fo->CopyItem(sourceItem.get(), destFolder.get(), nullptr, nullptr));
			}
		}

		RETURN_IF_FAILED(fo->PerformOperations());
	}

	return S_OK;
}
