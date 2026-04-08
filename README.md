[![Build status](https://ci.appveyor.com/api/projects/status/ggvn28se8xhes3x3/branch/master?svg=true)](https://ci.appveyor.com/project/derceg/explorerplusplus/branch/master)
[![Documentation Status](https://readthedocs.org/projects/explorerplusplus/badge/?version=latest)](https://explorerplusplus.readthedocs.io/en/latest/?badge=latest)
[![Crowdin](https://d322cqt584bo4o.cloudfront.net/explorerplusplus/localized.svg)](https://crowdin.com/project/explorerplusplus)

# Explorer++

Explorer++ is a lightweight and fast file manager for Windows.

## Fork status

This fork contains personal changes that are **not PR-ready** yet. The code hasn't been reviewed by a human, some features still have small bugs, and parts were dropped in the current state because they're already useful for my own workflow.

That means these changes may not be accepted upstream and may not be useful for every Explorer++ user.

### Fork-only changes

* Git status coloring in the list view for files and folders, with color rule filters for modified, staged, untracked, deleted, conflicted, added and ignored items
* Search dialog support for file content search plus indexed search through Windows Search
* Tree view navigation fix when clicking an already-selected folder after switching tabs
* Git ignored folder handling fix so folders with non-ignored content are no longer shown as fully ignored
* Replacement of Microsoft Detours with KNSoft.SlimDetours to improve ARM64 dark mode hook support
* Dark mode theming for shell-created dialogs and property sheets, plus better git status refresh after directory changes
* Startup time optimization: repo-level git status cache (shared across tabs in the same repo) and deferred navigation for non-selected restored tabs
* Fix for race condition between directory monitoring and initial item enumeration that caused assertion failures when copying files into a freshly-navigated folder
* Skip git status for UNC/network paths (including WSL via `\\wsl$\`) to avoid multi-second hangs when browsing Linux filesystems
* Background shell metadata retrieval: per-item shell calls (display name, attributes, find data) are now computed on the background COM STA thread instead of the UI thread, keeping Explorer++ responsive while navigating slow network or WSL folders

These items are based on the latest commits in this fork by `SR_team` (some co-authored with Copilot).

## Features

* With the option to save to the registry or a configuration file, Explorer++ is __completely portable__
* __Tabbed browsing__ for easy management of multiple folders
* Display window shows previews of files as they are selected
* __Easy-to-remember keyboard shortcuts__ for quick navigation
* Customizable user interface
* Full drag-and-drop support with other applications, including Windows Explorer
* Advanced file operations such as merging and splitting supported
* Change file dates and attributes
* Save a directory listing
* Bookmark tabs
* __Search__ for files using their name and attributes
* Switch between icon, list, detail, thumbnail and tile view
* Filter files

## Latest Builds

### 32-bit

[explorerpp_x86.zip](https://download.explorerplusplus.com/dev/latest/explorerpp_x86.zip)

### 64-bit

[explorerpp_x64.zip](https://download.explorerplusplus.com/dev/latest/explorerpp_x64.zip)

### ARM64

[explorerpp_arm64.zip](https://download.explorerplusplus.com/dev/latest/explorerpp_arm64.zip)

### Translations

[explorerpp_translations.zip](https://download.explorerplusplus.com/dev/latest/explorerpp_translations.zip)

For a full list of builds, see https://explorerplusplus.com/builds.

## Building Explorer++

For instructions on how to build Explorer++, see [BUILDING.md](BUILDING.md).

## Documentation

Documentation is available online at [Read the Docs](https://explorerplusplus.readthedocs.io/en/latest/).

## Translations

Translations are managed with [Crowdin](https://crowdin.com/project/explorerplusplus). To contribute to a translation, sign up with Crowdin, then edit the file corresponding to your language. If your language isn't listed, use the contact link shown on the project page to request it.
