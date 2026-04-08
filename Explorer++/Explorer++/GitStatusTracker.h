// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include <chrono>
#include <future>
#include <memory>
#include <mutex>
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
	// Uses a per-repo cache so that multiple tabs in the same repository share a single
	// 'git status' invocation instead of each spawning their own.
	static GitStatusMap GetStatusForDirectory(const std::wstring &directoryPath);

	// Clears the repo-level git status cache. Useful for tests or forced refresh.
	static void ClearCache();

private:
	struct CachedRepoStatus
	{
		std::shared_future<std::wstring> statusFuture;
		std::chrono::steady_clock::time_point createdAt;
	};

	static std::wstring RunGitCommand(const std::wstring &directoryPath, const std::wstring &args);
	static DWORD ParseStatusCodes(char indexStatus, char workTreeStatus);
	static std::wstring ExtractFilename(const std::wstring &repoRelativePath);
	static std::wstring GetRepoRoot(const std::wstring &directoryPath);
	static std::wstring NormalizePath(const std::wstring &path);
	static std::wstring ComputeDirectoryPrefix(const std::wstring &directoryPath,
		const std::wstring &repoRoot);

	static std::mutex s_cacheMutex;
	static std::unordered_map<std::wstring, CachedRepoStatus, CaseInsensitiveHash,
		CaseInsensitiveEqual>
		s_repoCache;
	static constexpr auto CACHE_TTL = std::chrono::seconds(5);
};
