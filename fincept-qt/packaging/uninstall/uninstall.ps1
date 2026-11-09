<#
.SYNOPSIS
    Remove Fincept Terminal and everything it created (Windows).

.DESCRIPTION
    Removes, for the current user:
      - the application: installer (IFW) installs in Program Files or the user
        profile, and portable copies that are running or sit next to this script
      - Start Menu / Desktop / taskbar shortcuts and the Apps & Features entry
      - all data: every profile, databases (incl. the encrypted API keys), logs,
        layouts, caches, crash dumps, the Python runtime, venvs and uv cache
      - settings (HKCU\Software\Fincept\FinceptTerminal), legacy Credential
        Manager entries from versions <= 4.0.2, temp files, and data the bundled
        Python scripts keep in your profile (.fincept, .oecd_cache, ...)

    Per-machine installs (Program Files) are removed in a separate elevated
    process (one UAC prompt); user data is always cleaned for the account that
    started the script. MCP client configs (Claude Desktop, Codex, Cursor...) are
    never edited - the script lists the ones that still mention Fincept.

    Easiest way to run it: double-click uninstall.cmd, which needs no change to
    the PowerShell execution policy.

    Keep in step with: packaging/uninstall/uninstall.sh (macOS / Linux) and
    packaging/installer/packages/com.fincept.terminal.core/meta/installscript.qs.
    Must stay ASCII-only and Windows PowerShell 5.1 compatible (no &&, ??, ?:).

.PARAMETER Yes
    Do not ask for confirmation.
.PARAMETER DryRun
    Only list what would be removed; change nothing.
.PARAMETER KeepData
    Remove the application only; keep data and settings.
.PARAMETER DataOnly
    Remove data and settings only; keep the application.
.PARAMETER SharedCaches
    Also remove caches other Python software shares (py-yfinance, matplotlib,
    yt-dlp, nltk_data, .qlib, .edgar). Off by default.
.PARAMETER DataDir
    Extra data folder(s) moved with FINCEPT_DATA_DIR (the variable itself is
    picked up automatically).

.EXAMPLE
    uninstall.cmd
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File uninstall.ps1 -DryRun
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File uninstall.ps1 -Yes -KeepData

.NOTES
    Exit status: 0 done (or nothing to do), 1 something could not be removed,
    2 cancelled.
#>
[CmdletBinding()]
param(
    [switch]$Yes,
    [switch]$DryRun,
    [switch]$KeepData,
    [switch]$DataOnly,
    [switch]$SharedCaches,
    [string[]]$DataDir = @(),
    # Set by uninstall.cmd: the folder it was started from, and whether to wait
    # for Enter before the window closes (double-click).
    [string]$FromDir = '',
    [switch]$PauseAtEnd,
    # Internal: the elevated child that removes per-machine installs.
    [string]$ElevatedInstallDirs = '',
    [string]$ResultFile = ''
)

Set-StrictMode -Version 2
$ErrorActionPreference = 'Continue'

$AppId = 'com.fincept.terminal'
$QtOrg = 'Fincept'
$QtApp = 'FinceptTerminal'
$ExeName = 'FinceptTerminal.exe'
$ToolName = 'FinceptMaintenanceTool.exe'

$script:Actions = 0
$script:Failures = New-Object System.Collections.Generic.List[string]
$script:DryRunNow = [bool]$DryRun

# Never leave the user's working folder inside a folder we are about to delete.
try {
    Set-Location -LiteralPath $env:TEMP
    [System.IO.Directory]::SetCurrentDirectory($env:TEMP)
} catch { }

# -- Output -------------------------------------------------------------------
function Write-Section([string]$Text) { Write-Host ''; Write-Host $Text -ForegroundColor White }
function Write-Info([string]$Text) { Write-Host "  $Text" }
function Write-Plan([string]$Verb, [string]$Text) { Write-Host "  $Verb " -NoNewline -ForegroundColor DarkGray; Write-Host $Text }
function Write-Done([string]$Verb, [string]$Text) { Write-Host "  $Verb " -NoNewline -ForegroundColor Green; Write-Host $Text }
function Write-Warn([string]$Text) { Write-Host "  ! $Text" -ForegroundColor Yellow }
function Add-Failure([string]$Text) {
    $script:Failures.Add($Text)
    Write-Host "  x $Text" -ForegroundColor Red
}

function Exit-Script([int]$Code) {
    if ($PauseAtEnd) {
        Write-Host ''
        [void](Read-Host 'Press Enter to close')
    }
    exit $Code
}

function Test-IsAdmin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    return (New-Object Security.Principal.WindowsPrincipal($id)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

# -- Paths --------------------------------------------------------------------
function Get-FullPath([string]$Path) {
    if (-not $Path) { return '' }
    try { return [System.IO.Path]::GetFullPath($Path).TrimEnd('\') } catch { return '' }
}

# Folders that are never removed, whatever a caller passes.
$script:Protected = @()
foreach ($p in @(
        $env:SystemDrive, "$env:SystemDrive\", $env:windir, $env:ProgramFiles, ${env:ProgramFiles(x86)},
        $env:ProgramData, $env:USERPROFILE, $env:LOCALAPPDATA, $env:APPDATA, $env:TEMP, $env:PUBLIC,
        "$env:LOCALAPPDATA\Programs", "$env:USERPROFILE\.cache", "$env:USERPROFILE\.claude",
        "$env:USERPROFILE\.claude\projects", "$env:USERPROFILE\Downloads", "$env:USERPROFILE\Documents",
        [Environment]::GetFolderPath('Desktop'), [Environment]::GetFolderPath('CommonDesktopDirectory'),
        [Environment]::GetFolderPath('Programs'), [Environment]::GetFolderPath('CommonPrograms'),
        [Environment]::GetFolderPath('StartMenu'), [Environment]::GetFolderPath('CommonStartMenu'))) {
    $f = Get-FullPath $p
    if ($f) { $script:Protected += $f }
}

function Test-Protected([string]$Full) {
    if (-not $Full) { return $true }
    if ($Full.Length -le 3) { return $true } # drive roots
    foreach ($p in $script:Protected) {
        if ([string]::Equals($Full, $p, [StringComparison]::OrdinalIgnoreCase)) { return $true }
    }
    return $false
}

function Test-Under([string]$Path, [string]$Root) {
    if (-not $Path -or -not $Root) { return $false }
    return $Path.StartsWith($Root.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)
}

# Remove a file or folder tree. Folders go through `rmdir /s /q` on the \\?\
# path: it handles paths over 260 characters (deep venvs) and, unlike
# Remove-Item in PowerShell 5.1, never follows junctions out of the tree.
function Remove-Target([string]$Path, [string]$Note = '') {
    if (-not $Path) { return }
    if (-not (Test-Path -LiteralPath $Path)) { return }
    $full = Get-FullPath $Path
    if (Test-Protected $full) { Write-Warn "refusing to remove protected path: $full"; return }
    $script:Actions++
    if ($script:DryRunNow) { Write-Plan 'would remove' ($full + $Note); return }
    $isDir = Test-Path -LiteralPath $full -PathType Container
    if ($full.StartsWith('\\')) { $long = '\\?\UNC\' + $full.Substring(2) } else { $long = '\\?\' + $full }
    for ($i = 0; $i -lt 6; $i++) {
        if ($isDir) {
            & cmd.exe /d /c "rmdir /s /q `"$long`" 2>nul" | Out-Null
        } else {
            try { Remove-Item -LiteralPath $full -Force -ErrorAction Stop } catch { }
        }
        if (-not (Test-Path -LiteralPath $full)) { Write-Done 'removed' $full; return }
        Start-Sleep -Milliseconds 800 # a process that just exited may still hold a handle
    }
    Add-Failure "could not remove $full (a file is in use?)"
}

function Remove-IfEmpty([string]$Path) {
    if ($script:DryRunNow) { return }
    if (-not (Test-Path -LiteralPath $Path -PathType Container)) { return }
    if (@(Get-ChildItem -LiteralPath $Path -Force -ErrorAction SilentlyContinue).Count -eq 0) {
        try { Remove-Item -LiteralPath $Path -Force -ErrorAction Stop; Write-Done 'removed' "$Path (empty)" } catch { }
    }
}

# -- Processes ----------------------------------------------------------------

# The app, its maintenance tool, and helpers (Python workers, uv, yt-dlp,
# QtWebEngineProcess) running from an install or data folder of this session.
# Matched by path, not by name, so a developer's build of FinceptTerminal.exe
# elsewhere is left alone (it is reported instead).
function Get-FinceptProcesses([string[]]$Roots) {
    $found = @()
    $session = [System.Diagnostics.Process]::GetCurrentProcess().SessionId
    foreach ($proc in @(Get-Process -ErrorAction SilentlyContinue)) {
        if ($proc.Id -eq $PID -or $proc.SessionId -ne $session) { continue }
        if ($proc.ProcessName -like 'FinceptMaintenanceTool*') { $found += $proc; continue }
        $path = $null
        try { $path = $proc.Path } catch { }
        if (-not $path) { continue }
        foreach ($r in $Roots) {
            if (Test-Under $path $r) { $found += $proc; break }
        }
    }
    return $found
}

function Show-OtherAppProcesses([string[]]$Roots) {
    foreach ($proc in @(Get-Process -Name 'FinceptTerminal' -ErrorAction SilentlyContinue)) {
        $path = $null
        try { $path = $proc.Path } catch { }
        $ours = $false
        foreach ($r in $Roots) { if (Test-Under $path $r) { $ours = $true } }
        if (-not $ours) {
            Write-Warn "FinceptTerminal.exe is running from $path (pid $($proc.Id)); close it so its data can be removed"
        }
    }
}

function Stop-FinceptProcesses([string[]]$Roots) {
    $procs = @(Get-FinceptProcesses $Roots)
    if ($procs.Count -eq 0) { return }
    $list = ($procs | ForEach-Object { "$($_.ProcessName) ($($_.Id))" }) -join ', '
    $script:Actions++
    if ($script:DryRunNow) { Write-Plan 'would stop' $list; return }
    Write-Info "stopping $list"
    foreach ($proc in $procs) {
        try { [void]$proc.CloseMainWindow() } catch { }
    }
    Start-Sleep -Seconds 2
    foreach ($proc in $procs) {
        try { if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force -ErrorAction Stop } } catch { }
    }
    Start-Sleep -Milliseconds 800
    foreach ($proc in $procs) {
        try {
            $proc.Refresh()
            if (-not $proc.HasExited) { Add-Failure "could not stop $($proc.ProcessName) ($($proc.Id))" }
        } catch { }
    }
}

# -- Application --------------------------------------------------------------

function Get-UninstallEntries {
    $entries = @()
    foreach ($k in @('HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*',
            'HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*',
            'HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*')) {
        foreach ($e in @(Get-ItemProperty -Path $k -ErrorAction SilentlyContinue)) {
            $name = $e.PSObject.Properties['DisplayName']
            if ($name -and ([string]$name.Value) -match '^Fincept\s*Terminal') { $entries += $e }
        }
    }
    return $entries
}

function Get-EntryDir($Entry) {
    $loc = $Entry.PSObject.Properties['InstallLocation']
    if ($loc -and $loc.Value) { return (Get-FullPath ([string]$loc.Value).Trim('"')) }
    $un = $Entry.PSObject.Properties['UninstallString']
    if ($un -and $un.Value) {
        $s = [string]$un.Value
        if ($s -match '^\s*"([^"]+)"') { $exe = $Matches[1] } else { $exe = ($s -split '\s+')[0] }
        if ($exe) { return (Get-FullPath (Split-Path -Parent $exe)) }
    }
    return ''
}

function Test-InstallDir([string]$Dir) {
    if (-not $Dir -or -not (Test-Path -LiteralPath $Dir -PathType Container)) { return $false }
    # A CMake build tree also holds FinceptTerminal.exe; it is not an install.
    foreach ($m in @('CMakeCache.txt', 'build.ninja', 'CMakeFiles')) {
        if (Test-Path -LiteralPath (Join-Path $Dir $m)) { return $false }
    }
    return (Test-Path -LiteralPath (Join-Path $Dir $ExeName)) -or (Test-Path -LiteralPath (Join-Path $Dir $ToolName))
}

function Find-InstallDirs {
    $candidates = @()
    foreach ($e in @(Get-UninstallEntries)) { $candidates += (Get-EntryDir $e) }
    $candidates += @("$env:ProgramFiles\FinceptTerminal", "${env:ProgramFiles(x86)}\FinceptTerminal",
        "$env:ProgramFiles\Fincept Terminal", "$env:LOCALAPPDATA\Programs\FinceptTerminal",
        "$env:LOCALAPPDATA\Programs\Fincept Terminal", "$env:USERPROFILE\FinceptTerminal")
    if ($FromDir) { $candidates += $FromDir }
    if ($PSScriptRoot) { $candidates += $PSScriptRoot }
    foreach ($proc in @(Get-Process -Name 'FinceptTerminal' -ErrorAction SilentlyContinue)) {
        try { if ($proc.Path) { $candidates += (Split-Path -Parent $proc.Path) } } catch { }
    }
    $dirs = @()
    foreach ($c in $candidates) {
        $full = Get-FullPath $c
        if (-not $full -or (Test-Protected $full)) { continue }
        if (-not (Test-InstallDir $full)) { continue }
        $dup = $false
        foreach ($d in $dirs) { if ([string]::Equals($d, $full, [StringComparison]::OrdinalIgnoreCase)) { $dup = $true } }
        if (-not $dup) { $dirs += $full }
    }
    return $dirs
}

function Test-Writable([string]$Dir) {
    $probe = Join-Path $Dir ('.fincept-uninstall-probe-' + [guid]::NewGuid().ToString('N'))
    try {
        [System.IO.File]::WriteAllText($probe, '')
        Remove-Item -LiteralPath $probe -Force -ErrorAction SilentlyContinue
        return $true
    } catch { return $false }
}

# Run the IFW maintenance tool headless (it removes its own registry entry and
# shortcuts), then delete whatever is left of the folder.
function Remove-InstallDir([string]$Dir) {
    $tool = Join-Path $Dir $ToolName
    if (Test-Path -LiteralPath $tool) {
        $script:Actions++
        if ($script:DryRunNow) {
            Write-Plan 'would run' "$tool purge"
        } else {
            Write-Info "running $ToolName purge (this can take a minute)"
            try {
                $p = Start-Process -FilePath $tool -ArgumentList @('purge', '--confirm-command', '--default-answer') `
                    -WorkingDirectory $env:TEMP -WindowStyle Hidden -PassThru -ErrorAction Stop
                if (-not $p.WaitForExit(300000)) {
                    try { $p.Kill() } catch { }
                    Write-Warn "$ToolName did not finish in 5 minutes; removing the folder directly"
                }
            } catch {
                Write-Warn "$ToolName could not run ($($_.Exception.Message)); removing the folder directly"
            }
            # The tool re-launches a temp copy of itself to delete the folder.
            for ($i = 0; $i -lt 120; $i++) {
                if (-not (Get-Process -Name 'FinceptMaintenanceTool*' -ErrorAction SilentlyContinue)) { break }
                Start-Sleep -Milliseconds 500
            }
        }
    }
    Remove-Target $Dir
}

# Apps & Features entries whose install folder is gone (or about to be).
function Remove-UninstallEntries([bool]$Machine) {
    foreach ($e in @(Get-UninstallEntries)) {
        $key = [string]$e.PSPath
        $isMachine = $key -match 'HKEY_LOCAL_MACHINE'
        if ($isMachine -ne $Machine) { continue }
        $dir = Get-EntryDir $e
        if ($dir -and (Test-InstallDir $dir) -and -not $script:DryRunNow) { continue } # still installed
        $script:Actions++
        $label = $key -replace '^Microsoft\.PowerShell\.Core\\Registry::', ''
        if ($script:DryRunNow) { Write-Plan 'would remove' "registry entry $label (if left behind)"; continue }
        try { Remove-Item -LiteralPath $key -Recurse -Force -ErrorAction Stop; Write-Done 'removed' "registry entry $label" }
        catch { Add-Failure "could not remove registry entry $label" }
    }
}

function Get-LinkTarget([string]$Lnk) {
    try { return (New-Object -ComObject WScript.Shell).CreateShortcut($Lnk).TargetPath } catch { return '' }
}

# Start Menu / Desktop / pinned taskbar shortcuts pointing at the app.
function Remove-Shortcuts([bool]$Machine) {
    if ($Machine) {
        $menus = @([Environment]::GetFolderPath('CommonPrograms'))
        $desktops = @([Environment]::GetFolderPath('CommonDesktopDirectory'))
    } else {
        $menus = @([Environment]::GetFolderPath('Programs'))
        $desktops = @([Environment]::GetFolderPath('Desktop'),
            "$env:APPDATA\Microsoft\Internet Explorer\Quick Launch\User Pinned\TaskBar",
            "$env:APPDATA\Microsoft\Internet Explorer\Quick Launch")
    }
    $pattern = '\\(FinceptTerminal|FinceptMaintenanceTool)\.exe$'
    foreach ($menu in $menus) {
        if (-not $menu) { continue }
        # The installer's Start Menu folder is "Fincept"; drop it when it only holds shortcuts.
        $folder = Join-Path $menu $QtOrg
        if (Test-Path -LiteralPath $folder -PathType Container) {
            $other = @(Get-ChildItem -LiteralPath $folder -Force -ErrorAction SilentlyContinue |
                    Where-Object { $_.PSIsContainer -or ($_.Extension -ne '.lnk' -and $_.Extension -ne '.url') })
            if ($other.Count -eq 0) {
                Remove-Target $folder
            } else {
                foreach ($l in @(Get-ChildItem -LiteralPath $folder -Filter '*.lnk' -ErrorAction SilentlyContinue)) {
                    if ((Get-LinkTarget $l.FullName) -match $pattern) { Remove-Target $l.FullName }
                }
            }
        }
        $desktops += $menu
    }
    foreach ($dir in $desktops) {
        if (-not $dir -or -not (Test-Path -LiteralPath $dir -PathType Container)) { continue }
        foreach ($l in @(Get-ChildItem -LiteralPath $dir -Filter '*.lnk' -ErrorAction SilentlyContinue)) {
            if ($l.Name -eq 'Fincept Terminal.lnk' -or ((Get-LinkTarget $l.FullName) -match $pattern)) {
                Remove-Target $l.FullName
            }
        }
    }
}

# -- Data and settings ---------------------------------------------------------

function Test-DataRoot([string]$Dir) {
    if (-not (Test-Path -LiteralPath $Dir -PathType Container)) { return $false }
    if ((Split-Path -Leaf $Dir) -eq $AppId) { return $true }
    foreach ($m in @('profiles.json', 'data\fincept.db', 'workspace.db', 'venv-numpy2', 'venv-numpy1', '.setup_complete', 'logs\fincept.log')) {
        if (Test-Path -LiteralPath (Join-Path $Dir $m)) { return $true }
    }
    return $false
}

function Get-DataRoots {
    $roots = @("$env:LOCALAPPDATA\$AppId")
    foreach ($scope in @('Process', 'User', 'Machine')) {
        $v = [Environment]::GetEnvironmentVariable('FINCEPT_DATA_DIR', $scope)
        if ($v) { $roots += $v }
    }
    $roots += $DataDir
    $out = @()
    foreach ($r in $roots) {
        $full = Get-FullPath $r
        if (-not $full) { continue }
        $dup = $false
        foreach ($o in $out) { if ([string]::Equals($o, $full, [StringComparison]::OrdinalIgnoreCase)) { $dup = $true } }
        if (-not $dup) { $out += $full }
    }
    return $out
}

function Remove-DataRoot([string]$Dir) {
    if (-not (Test-Path -LiteralPath $Dir -PathType Container)) { return }
    if (-not (Test-DataRoot $Dir)) { Write-Warn "skipping ${Dir}: it does not look like a Fincept Terminal data folder"; return }
    $note = ''
    if ($script:DryRunNow) {
        try {
            $bytes = (Get-ChildItem -LiteralPath $Dir -Recurse -Force -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum
            if ($bytes) { $note = '  ({0:N1} GB, all profiles)' -f ($bytes / 1GB) }
        } catch { }
    }
    Remove-Target $Dir $note
}

# Generic credentials "FinceptTerminal/<KEY>" written by versions <= 4.0.2.
$script:CredSource = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class FinceptUninstallCred {
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct CREDENTIAL {
        public int Flags; public int Type; public IntPtr TargetName; public IntPtr Comment;
        public System.Runtime.InteropServices.ComTypes.FILETIME LastWritten;
        public int CredentialBlobSize; public IntPtr CredentialBlob; public int Persist;
        public int AttributeCount; public IntPtr Attributes; public IntPtr TargetAlias; public IntPtr UserName;
    }
    [DllImport("advapi32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool CredEnumerateW(string filter, int flags, out int count, out IntPtr creds);
    [DllImport("advapi32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool CredDeleteW(string target, int type, int flags);
    [DllImport("advapi32.dll")]
    private static extern void CredFree(IntPtr buffer);
    public static string[] List(string filter) {
        var result = new List<string>();
        int count; IntPtr creds;
        if (!CredEnumerateW(filter, 0, out count, out creds)) return result.ToArray();
        try {
            for (int i = 0; i < count; i++) {
                IntPtr item = Marshal.ReadIntPtr(creds, i * IntPtr.Size);
                var c = (CREDENTIAL)Marshal.PtrToStructure(item, typeof(CREDENTIAL));
                result.Add(c.Type + "|" + Marshal.PtrToStringUni(c.TargetName));
            }
        } finally { CredFree(creds); }
        return result.ToArray();
    }
    public static bool Delete(string target, int type) { return CredDeleteW(target, type, 0); }
}
'@

function Remove-LegacyCredentials {
    $items = @()
    $api = $false
    try {
        if (-not ('FinceptUninstallCred' -as [type])) { Add-Type -TypeDefinition $script:CredSource -ErrorAction Stop }
        $items = @([FinceptUninstallCred]::List("$QtApp/*"))
        $api = $true
    } catch {
        # Constrained language mode / no compiler: fall back to cmdkey.
        foreach ($line in @(& cmdkey.exe "/list:$QtApp/*" 2>$null)) {
            if ($line -match 'Target:\s*(?:\S+:target=)?(\S+)') { $items += ('1|' + $Matches[1]) }
        }
    }
    foreach ($item in $items) {
        $type, $target = $item -split '\|', 2
        if (-not $target -or -not $target.StartsWith("$QtApp/")) { continue }
        $script:Actions++
        if ($script:DryRunNow) { Write-Plan 'would delete' "saved credential $target (Credential Manager)"; continue }
        $ok = $false
        if ($api) { $ok = [FinceptUninstallCred]::Delete($target, [int]$type) }
        else { & cmdkey.exe "/delete:$target" 2>$null | Out-Null; $ok = ($LASTEXITCODE -eq 0) }
        if ($ok) { Write-Done 'deleted' "saved credential $target" } else { Add-Failure "could not delete saved credential $target" }
    }
}

function Remove-RegistrySettings {
    foreach ($k in @("HKCU:\Software\$QtOrg\$QtApp", "HKCU:\Software\$QtOrg\$QtApp-Secure")) {
        if (-not (Test-Path -LiteralPath $k)) { continue }
        $script:Actions++
        if ($script:DryRunNow) { Write-Plan 'would remove' "registry key $k"; continue }
        try { Remove-Item -LiteralPath $k -Recurse -Force -ErrorAction Stop; Write-Done 'removed' "registry key $k" }
        catch { Add-Failure "could not remove registry key $k" }
    }
    # The vendor key only when nothing else (another Fincept product) lives there.
    $parent = "HKCU:\Software\$QtOrg"
    if (-not $script:DryRunNow -and (Test-Path -LiteralPath $parent)) {
        $item = Get-Item -LiteralPath $parent -ErrorAction SilentlyContinue
        if ($item -and $item.SubKeyCount -eq 0 -and $item.ValueCount -eq 0) {
            try { Remove-Item -LiteralPath $parent -Force -ErrorAction Stop; Write-Done 'removed' "registry key $parent (empty)" } catch { }
        }
    }
}

function Remove-TempFiles {
    $t = $env:TEMP
    if (-not $t -or -not (Test-Path -LiteralPath $t)) { return }
    # fincept_* : Python cell/arg files, report autosave, chart/paste images,
    # cancel flags, replay files, databento cache folder. Not this script's own
    # copy (fincept-uninstall*.ps1).
    foreach ($i in @(Get-ChildItem -LiteralPath $t -Filter 'fincept_*' -Force -ErrorAction SilentlyContinue)) {
        Remove-Target $i.FullName
    }
    Remove-Target (Join-Path $t 'fincept-boot.log')
}

function Remove-HomeExtras {
    Remove-Target "$env:USERPROFILE\.fincept"                 # AI Quant Lab: mlruns, models, pipelines
    Remove-Target "$env:USERPROFILE\.oecd_cache"              # OECD connector cache
    Remove-Target "$env:USERPROFILE\.cache\fincept-terminal"  # exchange market cache (outside the app)
    Remove-Target "$env:APPDATA\fincept-terminal"             # agent repository (agents.db)
    Remove-Target "$env:LOCALAPPDATA\fincept-dev"             # corporate-finance scripts run outside the app
    # Claude Code history of the coding-agent runs (cwd <root>\agent-workspace\...).
    $projects = "$env:USERPROFILE\.claude\projects"
    if (Test-Path -LiteralPath $projects) {
        foreach ($d in @(Get-ChildItem -LiteralPath $projects -Directory -Force -ErrorAction SilentlyContinue)) {
            if ($d.Name -like '*com-fincept-terminal-agent-workspace*') { Remove-Target $d.FullName }
        }
    }
}

function Remove-SharedCaches {
    foreach ($p in @("$env:LOCALAPPDATA\py-yfinance", "$env:USERPROFILE\.matplotlib", "$env:USERPROFILE\.cache\yt-dlp",
            "$env:LOCALAPPDATA\yt-dlp", "$env:APPDATA\nltk_data", "$env:USERPROFILE\nltk_data", "$env:USERPROFILE\.qlib",
            "$env:USERPROFILE\.edgar", "$env:TEMP\data-gym-cache")) {
        Remove-Target $p
    }
}

function Remove-DataEnvVar {
    $v = [Environment]::GetEnvironmentVariable('FINCEPT_DATA_DIR', 'User')
    if ($v) {
        $script:Actions++
        if ($script:DryRunNow) { Write-Plan 'would clear' "user environment variable FINCEPT_DATA_DIR ($v)" }
        else {
            try { [Environment]::SetEnvironmentVariable('FINCEPT_DATA_DIR', $null, 'User'); Write-Done 'cleared' 'user environment variable FINCEPT_DATA_DIR' }
            catch { Add-Failure 'could not clear the user environment variable FINCEPT_DATA_DIR' }
        }
    }
    if ([Environment]::GetEnvironmentVariable('FINCEPT_DATA_DIR', 'Machine')) {
        Write-Warn 'FINCEPT_DATA_DIR is also set machine-wide; remove it in System Properties > Environment Variables'
    }
}

function Remove-Data {
    foreach ($r in @(Get-DataRoots)) { Remove-DataRoot $r }
    # Legacy data folders (migrated by newer versions) and Qt's shader cache.
    Remove-Target "$env:LOCALAPPDATA\$QtOrg\$QtApp"
    Remove-IfEmpty "$env:LOCALAPPDATA\$QtOrg"
    Remove-Target "$env:LOCALAPPDATA\$QtApp"
    Remove-Target "$env:APPDATA\$QtOrg\$QtApp"
    Remove-IfEmpty "$env:APPDATA\$QtOrg"
    Remove-RegistrySettings
    Remove-LegacyCredentials
    Remove-HomeExtras
    Remove-TempFiles
    Remove-DataEnvVar
}

# MCP client configs are the user's files: report, never edit.
function Show-McpConfigs {
    $found = @()
    foreach ($f in @("$env:APPDATA\Claude\claude_desktop_config.json", "$env:USERPROFILE\.claude.json",
            "$env:USERPROFILE\.codex\config.toml", "$env:USERPROFILE\.gemini\settings.json",
            "$env:USERPROFILE\.cursor\mcp.json", "$env:USERPROFILE\.codeium\windsurf\mcp_config.json")) {
        if (-not (Test-Path -LiteralPath $f -PathType Leaf)) { continue }
        if (Select-String -LiteralPath $f -Pattern 'fincept_terminal|fincept_mcp|com\.fincept\.terminal|com-fincept-terminal|"fincept"|mcp_servers\.fincept' -Quiet -ErrorAction SilentlyContinue) {
            $found += $f
        }
    }
    if ($found.Count -eq 0) { return }
    Write-Section 'Left for you to edit'
    Write-Info 'These files still mention Fincept Terminal (MCP server entries or agent history).'
    Write-Info 'Remove the "fincept" / "fincept_terminal" entries yourself - this script never edits them:'
    foreach ($f in $found) { Write-Info "  $f" }
}

# -- Elevated child: per-machine installs only ----------------------------------
if ($ElevatedInstallDirs) {
    $script:DryRunNow = $false
    $dirs = @($ElevatedInstallDirs -split '\|' | Where-Object { $_ })
    Stop-FinceptProcesses $dirs
    foreach ($d in $dirs) { if (Test-InstallDir $d) { Remove-InstallDir $d } }
    Remove-UninstallEntries $true
    Remove-Shortcuts $true
    if ($ResultFile) {
        try { [System.IO.File]::WriteAllLines($ResultFile, [string[]]$script:Failures.ToArray()) } catch { }
    }
    if ($script:Failures.Count -gt 0) { exit 1 }
    exit 0
}

# -- Main ---------------------------------------------------------------------

function Invoke-All {
    $script:Actions = 0
    $script:Failures.Clear()
    $installDirs = @()
    if (-not $DataOnly) { $installDirs = @(Find-InstallDirs) }
    $dataRoots = @(Get-DataRoots)

    Write-Section 'Running processes'
    Stop-FinceptProcesses ($installDirs + $dataRoots)
    Show-OtherAppProcesses ($installDirs + $dataRoots)

    if (-not $DataOnly) {
        Write-Section 'Application'
        $elevated = @()
        foreach ($d in $installDirs) {
            if ((Test-IsAdmin) -or (Test-Writable $d)) {
                Remove-InstallDir $d
            } else {
                $elevated += $d
            }
        }
        $machineEntries = @(Get-UninstallEntries | Where-Object { [string]$_.PSPath -match 'HKEY_LOCAL_MACHINE' })
        if ($elevated.Count -gt 0 -or ($machineEntries.Count -gt 0 -and -not (Test-IsAdmin))) {
            if ($script:DryRunNow) {
                foreach ($d in $elevated) {
                    $script:Actions++
                    Write-Plan 'would remove' "$d  (asks for administrator approval)"
                }
                foreach ($e in $machineEntries) {
                    $script:Actions++
                    Write-Plan 'would remove' "Apps & Features entry $($e.PSChildName) (administrator)"
                }
            } else {
                Write-Info 'removing the per-machine install (approve the administrator prompt)'
                $result = Join-Path $env:TEMP ('fincept-uninstall-result-' + [guid]::NewGuid().ToString('N') + '.txt')
                $self = $PSCommandPath
                # Never empty: an empty value would start the full (user) flow elevated.
                $joined = $elevated -join '|'
                if (-not $joined) { $joined = '|' }
                $argList = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$self`"",
                    '-ElevatedInstallDirs', "`"$joined`"", '-ResultFile', "`"$result`"")
                try {
                    $child = Start-Process -FilePath 'powershell.exe' -ArgumentList $argList -Verb RunAs `
                        -WindowStyle Minimized -Wait -PassThru -ErrorAction Stop
                    if (Test-Path -LiteralPath $result) {
                        foreach ($line in @(Get-Content -LiteralPath $result -ErrorAction SilentlyContinue)) {
                            if ($line) { Add-Failure $line }
                        }
                        Remove-Item -LiteralPath $result -Force -ErrorAction SilentlyContinue
                    }
                    foreach ($d in $elevated) {
                        $script:Actions++
                        if (Test-Path -LiteralPath $d) {
                            if ($child.ExitCode -eq 0) { Add-Failure "could not remove $d" }
                        } else { Write-Done 'removed' $d }
                    }
                } catch {
                    Add-Failure 'administrator approval was declined; the per-machine install was not removed'
                }
            }
        }
        Remove-UninstallEntries $false
        if ((Test-IsAdmin) -and -not $script:DryRunNow) { Remove-UninstallEntries $true; Remove-Shortcuts $true }
        Remove-Shortcuts $false
    }

    if (-not $KeepData) {
        Write-Section 'Data, settings and saved credentials'
        Remove-Data
        if ($SharedCaches) {
            Write-Section 'Shared Python caches'
            Remove-SharedCaches
        }
    }
}

if ($KeepData -and $DataOnly) {
    Write-Host 'uninstall.ps1: -KeepData and -DataOnly cannot be combined.' -ForegroundColor Red
    Exit-Script 2
}

Write-Host ''
Write-Host "Fincept Terminal uninstaller - Windows, user $env:USERNAME ($env:USERPROFILE)" -ForegroundColor White
if (Test-IsAdmin) { Write-Info "running as administrator: data of $env:USERNAME is cleaned" }
if ($KeepData) { Write-Info '-KeepData: data and settings are kept' }
if ($DataOnly) { Write-Info '-DataOnly: the application is kept' }

if ($DryRun) {
    Invoke-All
    Write-Section 'Dry run'
    Write-Info "$($script:Actions) item(s) would be removed. Nothing was changed."
    Show-McpConfigs
    Exit-Script 0
}

if (-not $Yes) {
    $script:DryRunNow = $true
    Invoke-All
    $script:DryRunNow = $false
    if ($script:Actions -eq 0) {
        Write-Section 'Nothing to remove'
        Write-Info 'No Fincept Terminal files were found.'
        Show-McpConfigs
        Exit-Script 0
    }
    if (-not [Environment]::UserInteractive -or [Console]::IsInputRedirected) {
        Write-Host ''
        Write-Host 'No console to ask for confirmation; re-run with -Yes to remove the items above.' -ForegroundColor Yellow
        Exit-Script 2
    }
    Write-Host ''
    $answer = Read-Host "Remove the $($script:Actions) item(s) listed above? This cannot be undone. [y/N]"
    if ($answer -notmatch '^(y|yes)$') {
        Write-Host 'Cancelled; nothing was changed.'
        Exit-Script 2
    }
}

Invoke-All

Write-Section 'Summary'
if ($script:Failures.Count -gt 0) {
    Write-Host "  $($script:Failures.Count) item(s) could not be removed:" -ForegroundColor Red
    foreach ($f in $script:Failures) { Write-Host "    $f" }
    Write-Info 'Close Fincept Terminal, then run this uninstaller again.'
} elseif ($script:Actions -eq 0) {
    Write-Info 'No Fincept Terminal files were found.'
} elseif ($KeepData) {
    Write-Host '  Fincept Terminal was removed; your data and settings were kept.' -ForegroundColor Green
} elseif ($DataOnly) {
    Write-Host '  Fincept Terminal data and settings were removed; the application was kept.' -ForegroundColor Green
} else {
    Write-Host '  Fincept Terminal and all of its data were removed.' -ForegroundColor Green
}
Show-McpConfigs
if ($script:Failures.Count -gt 0) { Exit-Script 1 }
Exit-Script 0
