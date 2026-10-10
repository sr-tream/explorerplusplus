#Requires -Version 7.0
<#
.SYNOPSIS
Launches isolated Explorer++ copies and measures time to a visible, idle main window.
.DESCRIPTION
Uses disposable portable settings, leaving existing Explorer++ processes and settings alone.
Reports executable architecture and saves per-launch logs and JSON results. These are fresh
process launches; the script does not flush Windows disk or emulation caches. Folder enumeration
may still be running when the window appears.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string[]] $ExecutablePath,
    [string] $Directory = (Join-Path $PSScriptRoot '..'),
    [string] $ConfigPath,
    [switch] $UseOriginalExecutable,
    [switch] $WaitForNavigation,
    [ValidateRange(1, 20)]
    [int] $Iterations = 3,
    [ValidateRange(1, 100)]
    [int] $RestoredTabs = 1,
    [switch] $SkipQuickAccess,
    [ValidateRange(1, 120)]
    [int] $TimeoutSeconds = 30,
    [ValidateRange(0, 120000)]
    [int] $MaximumStartupMs = 0
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Get-PeArchitecture([string] $Path) {
    $stream = [IO.File]::OpenRead($Path)
    $reader = [IO.BinaryReader]::new($stream)
    try {
        if ($reader.ReadUInt16() -ne 0x5A4D) { throw "Not a Windows executable: $Path" }
        $stream.Position = 0x3C
        $offset = $reader.ReadInt32()
        $stream.Position = $offset
        if ($reader.ReadUInt32() -ne 0x4550) { throw "Invalid PE header: $Path" }
        switch ($reader.ReadUInt16()) {
            0x014C { 'X86' }
            0x8664 { 'X64' }
            0xAA64 { 'Arm64' }
            0xA641 { 'Arm64EC' }
            0xA64E { 'Arm64X' }
            default { throw "Unknown executable architecture: $Path" }
        }
    } finally {
        $reader.Dispose()
    }
}

if ($ConfigPath) {
    if ($PSBoundParameters.ContainsKey('RestoredTabs') -or $PSBoundParameters.ContainsKey('Directory') -or $SkipQuickAccess) {
        throw 'ConfigPath cannot be combined with Directory, RestoredTabs or SkipQuickAccess.'
    }
    $snapshot = [xml] [IO.File]::ReadAllText((Resolve-Path -LiteralPath $ConfigPath).Path)
    if ($snapshot.DocumentElement.Name -ne 'ExplorerPlusPlus') { throw 'Invalid Explorer++ configuration.' }
    $settings = $snapshot.DocumentElement.SelectSingleNode('Settings')
    if ($null -eq $settings) {
        $settings = $snapshot.CreateElement('Settings')
        [void] $snapshot.DocumentElement.AppendChild($settings)
    }
    # Only the disposable copy is changed: prevent handing off to, or prompting to close,
    # the user's existing Explorer++ process.
    foreach ($entry in @{ AllowMultipleInstances = 'yes'; ConfirmCloseTabs = 'no' }.GetEnumerator()) {
        $node = $settings.SelectSingleNode("Setting[@name='$($entry.Key)']")
        if ($null -eq $node) {
            $node = $snapshot.CreateElement('Setting')
            $node.SetAttribute('name', $entry.Key)
            [void] $settings.AppendChild($node)
        }
        $node.InnerText = $entry.Value
    }
    $RestoredTabs = $snapshot.SelectNodes('//Tabs/Tab').Count
    $quickAccessNode = $settings.SelectSingleNode("Setting[@name='ShowQuickAccessInTreeView']")
    $quickAccess = if ($null -eq $quickAccessNode) { 'yes' } else { $quickAccessNode.InnerText }
    $configXml = $snapshot.OuterXml
} else {
    $directoryPath = (Resolve-Path -LiteralPath $Directory).Path
    $escapedDirectory = [Security.SecurityElement]::Escape($directoryPath)
    $quickAccess = if ($SkipQuickAccess) { 'no' } else { 'yes' }
    $tabsXml = '<Tab Directory="' + $escapedDirectory + '" />'
    $configXml = @"
<?xml version="1.0" encoding="utf-8"?>
<ExplorerPlusPlus>
  <Settings>
    <Setting name="AllowMultipleInstances">yes</Setting>
    <Setting name="ConfirmCloseTabs">no</Setting>
    <Setting name="StartupMode">1</Setting>
    <Setting name="SynchronizeTreeview">no</Setting>
    <Setting name="ShowQuickAccessInTreeView">$quickAccess</Setting>
  </Settings>
  <Windows>
    <Window X="50" Y="50" Width="1000" Height="700" SelectedTab="0">
      <Tabs>$($tabsXml * $RestoredTabs)</Tabs>
    </Window>
  </Windows>
</ExplorerPlusPlus>
"@
}

$resultDirectory = Join-Path ([IO.Path]::GetTempPath()) ('ExplorerPP-startup-' + [guid]::NewGuid())
[void] [IO.Directory]::CreateDirectory($resultDirectory)
$osArchitecture = [Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
$results = [Collections.Generic.List[object]]::new()

try {
    foreach ($executable in $ExecutablePath) {
        $sourcePath = (Resolve-Path -LiteralPath $executable).Path
        $architecture = Get-PeArchitecture $sourcePath
        $hash = (Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash

        for ($iteration = 1; $iteration -le $Iterations; $iteration++) {
            $launchDirectory = Join-Path $resultDirectory ('launch-' + $results.Count)
            [void] [IO.Directory]::CreateDirectory($launchDirectory)
            $copyPath = $sourcePath
            if (-not $UseOriginalExecutable) {
                $copyPath = Join-Path $launchDirectory 'Explorer++.exe'
                Copy-Item -LiteralPath $sourcePath -Destination $copyPath
            }
            [IO.File]::WriteAllText((Join-Path $launchDirectory 'config.xml'), $configXml)

            $startInfo = [Diagnostics.ProcessStartInfo]::new($copyPath)
            $startInfo.UseShellExecute = $false
            $startInfo.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
            $startInfo.WorkingDirectory = $launchDirectory
            $startInfo.ArgumentList.Add('--enable-logging')
            $startInfo.Environment['EXPLORERPP_CONFIG'] = Join-Path $launchDirectory 'config.xml'
            $startInfo.Environment['TEMP'] = $launchDirectory
            $startInfo.Environment['TMP'] = $launchDirectory
            # Unbuffered INFO logs let the optional navigation check observe a completed commit.
            $startInfo.Environment['GLOG_logbufsecs'] = '0'
            $process = [Diagnostics.Process]::new()
            $process.StartInfo = $startInfo
            $started = $false
            $result = $null

            try {
                $timer = [Diagnostics.Stopwatch]::StartNew()
                [void] $process.Start()
                $processStartMs = [math]::Round($timer.Elapsed.TotalMilliseconds, 1)
                $started = $true
                while ($true) {
                    if ($process.HasExited) {
                        throw "Explorer++ exited before showing its window (exit $($process.ExitCode))."
                    }
                    $process.Refresh()
                    if ($process.MainWindowHandle -ne [IntPtr]::Zero -and $process.WaitForInputIdle(0)) {
                        break
                    }
                    if ($timer.Elapsed.TotalSeconds -ge $TimeoutSeconds) {
                        throw "Explorer++ did not show an idle window within $TimeoutSeconds seconds."
                    }
                    Start-Sleep -Milliseconds 10
                }
                $elapsedMs = [math]::Round($timer.Elapsed.TotalMilliseconds, 1)
                $result = [pscustomobject]@{
                    Executable = $sourcePath
                    SHA256 = $hash
                    OSArchitecture = $osArchitecture
                    BinaryArchitecture = $architecture
                    Iteration = $iteration
                    RestoredTabs = $RestoredTabs
                    ConfigPath = $ConfigPath
                    OriginalExecutable = [bool] $UseOriginalExecutable
                    QuickAccess = $quickAccess -eq 'yes'
                    WindowReadyMs = $elapsedMs
                    ProcessStartMs = $processStartMs
                    ProcessStartUtc = $process.StartTime.ToUniversalTime().ToString('o')
                    FirstNavigationCommitMs = $null
                    ThreadsAtWindowReady = $process.Threads.Count
                    WindowTitle = $process.MainWindowTitle
                    ShutdownPassed = $false
                    PhaseTimings = @()
                    Logs = $launchDirectory
                }
                $results.Add($result)

                if ($WaitForNavigation) {
                    while ($true) {
                        if ($process.HasExited) { throw 'Explorer++ exited before the first navigation commit.' }
                        $committed = Get-ChildItem -LiteralPath $launchDirectory -Filter '*.INFO.*' |
                            Select-String -Pattern 'Startup phase: tab navigation commit elapsed_ms=' -Quiet
                        if ($committed -contains $true -and $process.WaitForInputIdle(0)) {
                            $result.FirstNavigationCommitMs = [math]::Round($timer.Elapsed.TotalMilliseconds, 1)
                            break
                        }
                        if ($timer.Elapsed.TotalSeconds -ge $TimeoutSeconds) {
                            throw "The first navigation commit was not observed within $TimeoutSeconds seconds. This check requires a build with navigation phase tracing."
                        }
                        Start-Sleep -Milliseconds 50
                    }
                }
                $timer.Stop()

                if (-not $process.CloseMainWindow() -or -not $process.WaitForExit($TimeoutSeconds * 1000)) {
                    throw 'The isolated Explorer++ process did not close normally.'
                }
                if ($process.ExitCode -ne 0) { throw "Explorer++ exited with code $($process.ExitCode)." }
                $result.ShutdownPassed = $true
                if ($MaximumStartupMs -gt 0 -and $elapsedMs -gt $MaximumStartupMs) {
                    throw "Window readiness took $elapsedMs ms, exceeding $MaximumStartupMs ms."
                }
            } finally {
                # Only the disposable process created for this launch is terminated on failure.
                if ($started -and -not $process.HasExited) { $process.Kill($true); $process.WaitForExit() }
                $process.Dispose()
                if ($null -ne $result) {
                    $result.PhaseTimings = @(Get-ChildItem -LiteralPath $launchDirectory -Filter '*.INFO.*' |
                        Get-Content | ForEach-Object {
                            if ($_ -match 'Startup phase: (.+) elapsed_ms=([0-9.eE+-]+)$') {
                                [pscustomobject]@{
                                    Phase = $Matches[1]
                                    ElapsedMs = [double]::Parse($Matches[2], [Globalization.CultureInfo]::InvariantCulture)
                                }
                            }
                        })
                }
            }
        }
    }
} finally {
    $results | ConvertTo-Json -Depth 4 -AsArray | Set-Content -LiteralPath (Join-Path $resultDirectory 'results.json')
    Write-Host "Startup results and logs: $resultDirectory"
}

$results | Select-Object Executable, BinaryArchitecture, Iteration, RestoredTabs, WindowReadyMs, FirstNavigationCommitMs, ThreadsAtWindowReady
