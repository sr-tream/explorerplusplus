// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include <string>
#include <unordered_map>

// Bitmask values for git file statuses, used both for per-item status
// and for ColorRule filtering.
namespace GitStatus
{

inline constexpr DWORD None = 0;
inline constexpr DWORD Modified = 0x1;
inline constexpr DWORD Staged = 0x2;
inline constexpr DWORD Untracked = 0x4;
inline constexpr DWORD Ignored = 0x8;
inline constexpr DWORD Conflicted = 0x10;
inline constexpr DWORD Deleted = 0x20;
inline constexpr DWORD Renamed = 0x40;
inline constexpr DWORD Added = 0x80;

}

struct CaseInsensitiveHash
{
	size_t operator()(const std::wstring &str) const;
};

struct CaseInsensitiveEqual
{
	bool operator()(const std::wstring &a, const std::wstring &b) const;
};

struct CaseInsensitiveLess
{
	bool operator()(const std::wstring &a, const std::wstring &b) const;
};

using GitStatusMap =
	std::unordered_map<std::wstring, DWORD, CaseInsensitiveHash, CaseInsensitiveEqual>;

struct GitStatusResult
{
	std::wstring directory;
	GitStatusMap statusMap;
};

class GitStatusTracker
{
public:
	// Returns a map of filename -> git status bitmask for files in the given directory.
	// Runs `git status` in the directory. Returns empty map if not a git repo or git is not
	// installed.
	static GitStatusMap GetStatusForDirectory(const std::wstring &directoryPath);

private:
	static std::wstring RunGitCommand(const std::wstring &directoryPath, const std::wstring &args);
	static DWORD ParseStatusCodes(char indexStatus, char workTreeStatus);
	static std::wstring ExtractFilename(const std::wstring &repoRelativePath);
};
