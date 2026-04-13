// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include <optional>
#include <string>
#include <unordered_set>
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

// Builds a duplicate filename using the " (N)" suffix pattern.
// E.g., "foo.txt" + 2 -> "foo (2).txt", "Photos" + 3 -> "Photos (3)".
std::wstring BuildDuplicateName(const std::wstring &filename, size_t duplicateIndex);

// Finds the next available duplicate name in the destination directory. Reserved names are treated
// as already taken and updated with the selected result.
std::wstring FindNextAvailableDuplicateName(const std::wstring &filename,
	const std::wstring &destinationDir, std::unordered_set<std::wstring> &reservedNames);

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
