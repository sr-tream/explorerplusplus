// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include <optional>
#include <string>
#include <vector>
#include <windows.h>

struct DuplicateFileMatch
{
	std::wstring sourcePath;
	std::wstring targetName;
	std::wstring existingPath;
};

// Strips a trailing " (N)" suffix from a filename, where N is one or more digits.
// E.g., "foo (1).txt" -> "foo.txt", "doc (23).pdf" -> "doc.pdf".
// Returns nullopt if no such suffix is found.
std::optional<std::wstring> StripDuplicateSuffix(const std::wstring &filename);

// Checks source files for a " (N)" suffix pattern and finds matches where the base name
// already exists in the destination directory. Skips directories. Returns an empty list if
// multiple source files would map to the same target (ambiguous case).
std::vector<DuplicateFileMatch> FindDuplicateMatches(
	const std::vector<std::wstring> &sourceFiles, const std::wstring &destinationDir);

// Copies or moves all source files to the destination directory using IFileOperation.
// Matched files are renamed to their base names (overwriting the existing files).
// Unmatched files are copied/moved with their original names.
HRESULT PerformDuplicateReplace(HWND hwnd, const std::vector<std::wstring> &sourceFiles,
	const std::vector<DuplicateFileMatch> &matches, const std::wstring &destinationDir,
	bool isMove);
