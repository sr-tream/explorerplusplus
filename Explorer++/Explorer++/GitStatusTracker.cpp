// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "GitStatusTracker.h"
#include <algorithm>
#include <set>
#include <sstream>

// Static member definitions
std::mutex GitStatusTracker::s_cacheMutex;
std::unordered_map<std::wstring, GitStatusTracker::CachedRepoStatus, CaseInsensitiveHash,
	CaseInsensitiveEqual>
	GitStatusTracker::s_repoCache;

size_t CaseInsensitiveHash::operator()(const std::wstring &str) const
{
	std::wstring lower = str;
	std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
	return std::hash<std::wstring>{}(lower);
}

bool CaseInsensitiveEqual::operator()(const std::wstring &a, const std::wstring &b) const
{
	return _wcsicmp(a.c_str(), b.c_str()) == 0;
}

bool CaseInsensitiveLess::operator()(const std::wstring &a, const std::wstring &b) const
{
	return _wcsicmp(a.c_str(), b.c_str()) < 0;
}

std::wstring GitStatusTracker::RunGitCommand(const std::wstring &directoryPath,
	const std::wstring &args)
{
	SECURITY_ATTRIBUTES sa = {};
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;

	HANDLE hReadPipe = nullptr;
	HANDLE hWritePipe = nullptr;

	if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
	{
		return L"";
	}

	SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFO si = {};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.hStdOutput = hWritePipe;
	si.hStdError = hWritePipe;
	si.hStdInput = nullptr;
	si.wShowWindow = SW_HIDE;

	PROCESS_INFORMATION pi = {};

	std::wstring commandLine = L"git " + args;
	std::vector<wchar_t> cmdBuf(commandLine.begin(), commandLine.end());
	cmdBuf.push_back(L'\0');

	BOOL success = CreateProcess(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE,
		CREATE_NO_WINDOW, nullptr, directoryPath.c_str(), &si, &pi);

	CloseHandle(hWritePipe);

	if (!success)
	{
		CloseHandle(hReadPipe);
		return L"";
	}

	std::string output;
	char buffer[4096];
	DWORD bytesRead;

	while (ReadFile(hReadPipe, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0)
	{
		output.append(buffer, bytesRead);
	}

	WaitForSingleObject(pi.hProcess, 5000);

	DWORD exitCode = 1;
	GetExitCodeProcess(pi.hProcess, &exitCode);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	CloseHandle(hReadPipe);

	if (exitCode != 0)
	{
		return L"";
	}

	// Convert UTF-8 output to wstring
	if (output.empty())
	{
		return L"";
	}

	int wideLen =
		MultiByteToWideChar(CP_UTF8, 0, output.c_str(), static_cast<int>(output.size()), nullptr, 0);

	if (wideLen <= 0)
	{
		return L"";
	}

	std::wstring wideOutput(wideLen, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, output.c_str(), static_cast<int>(output.size()),
		wideOutput.data(), wideLen);

	return wideOutput;
}

DWORD GitStatusTracker::ParseStatusCodes(char indexStatus, char workTreeStatus)
{
	DWORD status = GitStatus::None;

	// Check for conflicts first
	if (indexStatus == 'U' || workTreeStatus == 'U'
		|| (indexStatus == 'A' && workTreeStatus == 'A')
		|| (indexStatus == 'D' && workTreeStatus == 'D'))
	{
		return GitStatus::Conflicted;
	}

	// Index (staged) statuses
	switch (indexStatus)
	{
	case 'A':
		status |= GitStatus::Added | GitStatus::Staged;
		break;
	case 'M':
		status |= GitStatus::Staged;
		break;
	case 'D':
		status |= GitStatus::Deleted | GitStatus::Staged;
		break;
	case 'R':
		status |= GitStatus::Renamed | GitStatus::Staged;
		break;
	}

	// Work tree statuses
	switch (workTreeStatus)
	{
	case 'M':
		status |= GitStatus::Modified;
		break;
	case 'D':
		status |= GitStatus::Deleted;
		break;
	}

	return status;
}

std::wstring GitStatusTracker::ExtractFilename(const std::wstring &repoRelativePath)
{
	// Git uses forward slashes. Extract the last component.
	auto pos = repoRelativePath.find_last_of(L'/');

	if (pos != std::wstring::npos)
	{
		return repoRelativePath.substr(pos + 1);
	}

	// Also check for backslash in case git on Windows uses it
	pos = repoRelativePath.find_last_of(L'\\');

	if (pos != std::wstring::npos)
	{
		return repoRelativePath.substr(pos + 1);
	}

	return repoRelativePath;
}

std::wstring GitStatusTracker::NormalizePath(const std::wstring &path)
{
	std::wstring normalized = path;
	std::replace(normalized.begin(), normalized.end(), L'\\', L'/');

	while (!normalized.empty() && normalized.back() == L'/')
	{
		normalized.pop_back();
	}

	return normalized;
}

std::wstring GitStatusTracker::GetRepoRoot(const std::wstring &directoryPath)
{
	std::wstring output = RunGitCommand(directoryPath, L"rev-parse --show-toplevel");

	if (output.empty())
	{
		return L"";
	}

	while (!output.empty() && (output.back() == L'\n' || output.back() == L'\r'))
	{
		output.pop_back();
	}

	return output;
}

std::wstring GitStatusTracker::ComputeDirectoryPrefix(const std::wstring &directoryPath,
	const std::wstring &repoRoot)
{
	std::wstring normalizedDir = NormalizePath(directoryPath);
	std::wstring normalizedRoot = NormalizePath(repoRoot);

	if (_wcsicmp(normalizedDir.c_str(), normalizedRoot.c_str()) == 0)
	{
		return L"";
	}

	if (normalizedDir.size() <= normalizedRoot.size()
		|| normalizedDir[normalizedRoot.size()] != L'/'
		|| _wcsnicmp(normalizedDir.c_str(), normalizedRoot.c_str(), normalizedRoot.size()) != 0)
	{
		return L"";
	}

	std::wstring prefix = normalizedDir.substr(normalizedRoot.size() + 1);

	if (!prefix.empty())
	{
		prefix += L'/';
	}

	return prefix;
}

void GitStatusTracker::ClearCache()
{
	std::lock_guard<std::mutex> lock(s_cacheMutex);
	s_repoCache.clear();
}

GitStatusMap GitStatusTracker::GetStatusForDirectory(const std::wstring &directoryPath)
{
	GitStatusMap statusMap;

	// Skip UNC/network paths (including WSL paths like \\wsl$\ and \\wsl.localhost\).
	// Running Windows git.exe against these filesystems is extremely slow due to network
	// traversal looking for .git directories.
	if (directoryPath.size() >= 2 && directoryPath[0] == L'\\' && directoryPath[1] == L'\\')
	{
		return statusMap;
	}

	// Get the repo root — also serves as the cache key. This is fast (~5ms).
	std::wstring repoRoot = GetRepoRoot(directoryPath);

	if (repoRoot.empty())
	{
		return statusMap;
	}

	std::wstring normalizedRoot = NormalizePath(repoRoot);

	// Try to get git status output from the per-repo cache. Multiple tabs in the
	// same repository will share a single 'git status' invocation via shared_future,
	// avoiding redundant process spawns during multi-tab startup.
	std::shared_future<std::wstring> statusFuture;
	std::shared_ptr<std::promise<std::wstring>> myPromise;

	{
		std::lock_guard<std::mutex> lock(s_cacheMutex);

		auto it = s_repoCache.find(normalizedRoot);

		if (it != s_repoCache.end())
		{
			auto now = std::chrono::steady_clock::now();

			if (now - it->second.createdAt <= CACHE_TTL)
			{
				statusFuture = it->second.statusFuture;
			}
			else
			{
				// Expired — replace with new fetch
				myPromise = std::make_shared<std::promise<std::wstring>>();
				statusFuture = myPromise->get_future().share();
				it->second = { statusFuture, now };
			}
		}
		else
		{
			myPromise = std::make_shared<std::promise<std::wstring>>();
			statusFuture = myPromise->get_future().share();
			s_repoCache[normalizedRoot] = { statusFuture,
				std::chrono::steady_clock::now() };
		}
	}

	if (myPromise)
	{
		try
		{
			std::wstring result =
				RunGitCommand(directoryPath, L"status --porcelain -u --ignored");
			myPromise->set_value(std::move(result));
		}
		catch (...)
		{
			myPromise->set_value(L"");
		}
	}

	std::wstring output = statusFuture.get();

	if (output.empty())
	{
		return statusMap;
	}

	// Compute the repo-relative directory prefix via path arithmetic instead of
	// spawning a separate 'git rev-parse --show-prefix' process.
	std::wstring directoryPrefix = ComputeDirectoryPrefix(directoryPath, repoRoot);

	std::set<std::wstring, CaseInsensitiveLess> foldersWithNonIgnoredContent;
	// Use --full-name so paths are relative to the repo root, matching the prefix
	// format used for filtering.
	std::wstring nonIgnoredOutput =
		RunGitCommand(directoryPath, L"ls-files --full-name --cached --others --exclude-standard");

	if (!nonIgnoredOutput.empty())
	{
		std::wistringstream nonIgnoredStream(nonIgnoredOutput);
		std::wstring nonIgnoredPath;

		while (std::getline(nonIgnoredStream, nonIgnoredPath))
		{
			if (!nonIgnoredPath.empty() && nonIgnoredPath.back() == L'\r')
			{
				nonIgnoredPath.pop_back();
			}

			if (!directoryPrefix.empty())
			{
				if (_wcsnicmp(nonIgnoredPath.c_str(), directoryPrefix.c_str(), directoryPrefix.size())
					!= 0)
				{
					continue;
				}

				nonIgnoredPath = nonIgnoredPath.substr(directoryPrefix.size());
			}

			auto slashPos = nonIgnoredPath.find(L'/');

			if (slashPos == std::wstring::npos)
			{
				continue;
			}

			std::wstring subDir = nonIgnoredPath.substr(0, slashPos);

			if (!subDir.empty())
			{
				foldersWithNonIgnoredContent.insert(subDir);
			}
		}
	}

	// Parse porcelain output line by line.
	// Format: XY filename
	// where X is index status, Y is work tree status
	std::wistringstream stream(output);
	std::wstring line;
	std::set<std::wstring, CaseInsensitiveLess> folderEntries;

	while (std::getline(stream, line))
	{
		if (line.size() < 4)
		{
			continue;
		}

		char indexStatus = static_cast<char>(line[0]);
		char workTreeStatus = static_cast<char>(line[1]);

		// Skip the space at position 2
		std::wstring filePath = line.substr(3);

		// Handle quoted paths (git quotes paths with special chars)
		if (filePath.size() >= 2 && filePath.front() == L'"' && filePath.back() == L'"')
		{
			filePath = filePath.substr(1, filePath.size() - 2);
		}

		// Handle rename entries: "R  old -> new"
		if (indexStatus == 'R' || workTreeStatus == 'R')
		{
			auto arrowPos = filePath.find(L" -> ");
			if (arrowPos != std::wstring::npos)
			{
				filePath = filePath.substr(arrowPos + 4);
			}
		}

		// Untracked entries
		if (indexStatus == '?' && workTreeStatus == '?')
		{
			// Filter to current directory prefix
			if (!directoryPrefix.empty())
			{
				if (_wcsnicmp(filePath.c_str(), directoryPrefix.c_str(), directoryPrefix.size()) != 0)
				{
					continue;
				}

				filePath = filePath.substr(directoryPrefix.size());
			}

			// If it's in a subdirectory, aggregate status onto the subdirectory name
			auto slashPos = filePath.find(L'/');

			if (slashPos != std::wstring::npos)
			{
				std::wstring subDir = filePath.substr(0, slashPos);
				statusMap[subDir] |= GitStatus::Untracked;
				folderEntries.insert(subDir);
				continue;
			}

			statusMap[filePath] |= GitStatus::Untracked;
			continue;
		}

		// Ignored entries
		if (indexStatus == '!' && workTreeStatus == '!')
		{
			if (!directoryPrefix.empty())
			{
				if (_wcsnicmp(filePath.c_str(), directoryPrefix.c_str(), directoryPrefix.size())
					!= 0)
				{
					continue;
				}

				filePath = filePath.substr(directoryPrefix.size());
			}

			auto slashPos = filePath.find(L'/');

			if (slashPos != std::wstring::npos)
			{
				std::wstring subDir = filePath.substr(0, slashPos);
				statusMap[subDir] |= GitStatus::Ignored;
				folderEntries.insert(subDir);
				continue;
			}

			statusMap[filePath] |= GitStatus::Ignored;
			continue;
		}

		// For tracked files, filter to current directory prefix
		if (!directoryPrefix.empty())
		{
			if (_wcsnicmp(filePath.c_str(), directoryPrefix.c_str(), directoryPrefix.size()) != 0)
			{
				continue;
			}

			filePath = filePath.substr(directoryPrefix.size());
		}

		DWORD status = ParseStatusCodes(indexStatus, workTreeStatus);

		if (status == GitStatus::None)
		{
			continue;
		}

		// If it's in a subdirectory, aggregate status onto the subdirectory name
		auto slashPos = filePath.find(L'/');

		if (slashPos != std::wstring::npos)
		{
			std::wstring subDir = filePath.substr(0, slashPos);
			statusMap[subDir] |= status;
			folderEntries.insert(subDir);
			continue;
		}

		// Merge with existing status (a file can be both staged and modified)
		statusMap[filePath] |= status;
	}

	// For folder entries, ensure Conflicted takes priority and that Ignored is only set when
	// there isn't any non-ignored content in the folder.
	for (const auto &folderName : folderEntries)
	{
		auto it = statusMap.find(folderName);

		if (it == statusMap.end())
		{
			continue;
		}

		if (WI_IsFlagSet(it->second, GitStatus::Conflicted))
		{
			it->second = GitStatus::Conflicted;
			continue;
		}

		if (WI_IsFlagSet(it->second, GitStatus::Ignored)
			&& (((it->second & ~GitStatus::Ignored) != GitStatus::None)
				|| foldersWithNonIgnoredContent.find(folderName)
					!= foldersWithNonIgnoredContent.end()))
		{
			it->second &= ~GitStatus::Ignored;

			if (it->second == GitStatus::None)
			{
				statusMap.erase(it);
			}
		}
	}

	return statusMap;
}
