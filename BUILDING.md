# Requirements

- Visual Studio 2019/2022 (2022 recommended). Install the "Desktop development with C++" workload.
- Windows 10 SDK (most recent version recommended)
- (Optional) To build for ARM64, the following Visual Studio components should be installed:
    - MSVC v143 - VS 2022 C++ ARM64 build tools (Latest)
- (Optional) To build with Clang (using the Debug-Clang/Release-Clang solution configurations), the following Visual Studio components should be installed:
    - C++ Clang tools for Windows
- (Optional) To build the installer, WiX 3.11 and the relevant Visual Studio extension should be installed. Download links for both of those items can be found on the [WiX website](https://wixtoolset.org/docs/wix3/).

# Setup

[vcpkg](https://vcpkg.io/) is used to manage dependencies. Before you can build Explorer++, you'll first need to initialize vcpkg:

- Clone this repo with submodules enabled:
```
git clone --recurse-submodules https://github.com/derceg/explorerplusplus.git
cd explorerplusplus
```
- Run the vcpkg bootstrapper:
    - cmd/PowerShell: `.\Explorer++\ThirdParty\vcpkg\bootstrap-vcpkg.bat`
    - git bash/posix shell: `./Explorer++/ThirdParty/vcpkg/bootstrap-vcpkg.sh`


The relevant packages should then be automatically installed during the first build.

# Compiling

Open `Explorer++\Explorer++.sln`. From within Visual Studio, select `Debug` > `Start Without Debugging` to compile and run the program.

# Translations

Building the program in release mode will also build all of the translations. The resulting DLLs can then be used with Explorer++.

# Tests

The `TestExplorer++` project contains unit tests for the solution as a whole. The GoogleTest package is installed via vcpkg, so provided vcpkg has been initialized and you've been able to build the solution, you should just need to build the `TestExplorer++` project, then run the tests via the Visual Studio Test Explorer.

Note that the `TestHelper` project is older and is in the process of being removed. It doesn't currently compile and shouldn't be used.

## Startup sanity checks and timing

On ARM64 Windows, build the `Release|ARM64` configuration for native execution. An x64 build
runs under emulation; comparing it with a native ARM64 application also compares the runtime
architectures. The installed executable's architecture can be checked by the timing script below.
When building from a terminal, use the ARM64-hosted MSBuild executable under
`MSBuild\Current\Bin\arm64` as well as `Platform=ARM64` to use native build tools.

Run the deferred-tab sanity tests from the repository root after building `TestExplorer++`:

```powershell
& '.\Explorer++\TestExplorer++\ARM64\Release\TestExplorer++.exe' '--gtest_filter=TabContainerTest.*Deferred*'
```

To measure fresh process launches with isolated portable settings:

```powershell
.\Scripts\Measure-Startup.ps1 -ExecutablePath '.\Explorer++\Explorer++\ARM64\Release\Explorer++.exe'
.\Scripts\Measure-Startup.ps1 -ExecutablePath '.\Explorer++\Explorer++\ARM64\Release\Explorer++.exe' -RestoredTabs 10
.\Scripts\Measure-Startup.ps1 -ExecutablePath '.\Explorer++\Explorer++\ARM64\Release\Explorer++.exe' -SkipQuickAccess
.\Scripts\Measure-Startup.ps1 -ExecutablePath '.\Explorer++\Explorer++\ARM64\Release\Explorer++.exe' -ConfigPath '.\session-snapshot.xml'
```

The script creates copies of the static executable with their own `config.xml`, launches them,
checks that a visible main window reaches input idle, and checks that each process exits normally.
It preserves the existing session. It reports architecture, window readiness time, thread count,
and the location of JSON results and logs. Use `-MaximumStartupMs` to set an optional regression
limit. Copying and hashing the executable warms filesystem caches, so these measurements are
**not disk-cold startup measurements**. The window may be ready while folder enumeration is
still running. For a cold-start trace, run the original executable with `--enable-logging` after
a reboot, without launching it first.

Use `-ConfigPath` to profile an existing XML settings snapshot, including its saved tab paths.
The script changes only the disposable copy to allow multiple instances and suppress the close-tabs
confirmation. Do not combine it with `-Directory`, `-RestoredTabs` or `-SkipQuickAccess`.
Use `-UseOriginalExecutable` to launch the original executable with disposable settings instead of
copying it, which preserves its installation location and nearby DLLs. Use `-WaitForNavigation` with
a build containing navigation phase tracing to also measure the first folder commit and subsequent
input idle; this includes enumeration work that can finish after the window appears.

With `--enable-logging`, INFO logs include `Startup phase:` durations for WinMain initialization,
App construction, settings, process registration, resources, toolbars, folder tree creation,
initial tabs, plugins, showing and painting the window, and initialization through the first paint.
Tab creation also logs folder-path resolution, shell browser construction, the Windows shell service
connection, shell window registration and navigation commits. These phases are useful when a new tab
or restored folder stalls after the main window has appeared.
Logs normally go to the Windows temporary directory. Nested durations overlap and should not be
added together. The WinMain
timer starts after the OS loader and C++ static initialization, so it excludes that earlier work.

The startup path currently creates and expands the Quick Access and Desktop roots on the UI
thread before showing the main window, even when the folder pane is hidden. Restored tabs defer
enumeration, but their constructors still retrieve shell metadata and read `desktop.ini` on the UI
thread. Each restored tab also creates several worker threads and an `IShellWindows` COM object.
The phase timings help distinguish those costs from architecture, loader, and system-cache costs.
