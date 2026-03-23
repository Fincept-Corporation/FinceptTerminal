param(
    [switch]$Fix,
    [switch]$Tidy,
    [switch]$Cppcheck,
    [switch]$All,
    [switch]$Configure
)

$Root         = Split-Path $PSScriptRoot -Parent
$SrcDir       = Join-Path $Root "src"
$BuildDir     = Join-Path $Root "build-tidy"
$QT_PATH      = "C:\Qt\6.8.3\msvc2022_64"
$NINJA_EXE    = "C:\Users\Tilak\AppData\Local\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe\ninja.exe"
$CPPCHECK_EXE = "C:\Program Files\Cppcheck\cppcheck.exe"
$RUN_TIDY     = "C:\Program Files\LLVM\bin\run-clang-tidy"

$script:Failed = $false

function Write-Ok($msg)   { Write-Host $msg -ForegroundColor Green }
function Write-Warn($msg) { Write-Host $msg -ForegroundColor Yellow }
function Write-Err($msg)  { Write-Host $msg -ForegroundColor Red }

function Initialize-VsEnv {
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vsWhere)) { Write-Warn "vswhere not found"; return }
    $vsPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
    if (-not $vsPath) { Write-Warn "No VS with MSVC found"; return }
    $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
    if (-not (Test-Path $vcvars)) { Write-Warn "vcvars64.bat not found"; return }
    Write-Ok "Bootstrapping VS environment..."
    $envDump = cmd /c """$vcvars"" && set" 2>$null
    foreach ($line in $envDump) {
        if ($line -match "^([^=]+)=(.+)$") {
            [System.Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], "Process")
        }
    }
    Write-Ok "VS environment ready."
}

function Invoke-ClangFormat($doFix) {
    if (-not (Get-Command clang-format -ErrorAction SilentlyContinue)) {
        Write-Err "clang-format not found. Install LLVM."
        $script:Failed = $true; return
    }
    # Collect all files into array — one process call instead of N spawns
    $files = Get-ChildItem -Path $SrcDir -Recurse -Include "*.cpp","*.h" |
        Select-Object -ExpandProperty FullName
    $count = $files.Count
    if ($doFix) {
        Write-Ok "clang-format: fixing $count files..."
        & clang-format -i --style=file @files
        Write-Ok "clang-format: done."
    } else {
        Write-Ok "clang-format: checking $count files..."
        # Single process for all files — fast
        & clang-format --style=file --dry-run --Werror @files 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) {
            # Re-run to find which files failed
            $bad = @()
            foreach ($f in $files) {
                & clang-format --style=file --dry-run --Werror $f 2>&1 | Out-Null
                if ($LASTEXITCODE -ne 0) { $bad += $f }
            }
            Write-Err "clang-format: $($bad.Count) file(s) need formatting:"
            $bad | ForEach-Object { Write-Host "  $_" }
            Write-Warn "Run: .\scripts\lint.ps1 -Fix"
            $script:Failed = $true
        } else {
            Write-Ok "clang-format: all $count files correctly formatted."
        }
    }
}

function Invoke-Configure {
    Initialize-VsEnv
    if (-not (Test-Path $NINJA_EXE)) {
        Write-Err "ninja not found at: $NINJA_EXE"
        $script:Failed = $true; return
    }
    $ninjaDir = [System.IO.Path]::GetDirectoryName($NINJA_EXE)
    $env:PATH = "$ninjaDir;$env:PATH"
    Write-Ok "Configuring CMake (compile_commands.json)..."
    cmake -B $BuildDir -S $Root -G Ninja "-DCMAKE_BUILD_TYPE=Debug" "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON" "-DCMAKE_PREFIX_PATH=$QT_PATH" 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Err "CMake configure failed."
        $script:Failed = $true
    } else {
        Write-Ok "compile_commands.json ready at: $BuildDir"
    }
}

function Invoke-ClangTidy {
    Initialize-VsEnv
    if (-not (Test-Path $RUN_TIDY)) {
        Write-Err "run-clang-tidy not found at: $RUN_TIDY"
        $script:Failed = $true; return
    }
    $compileDb = Join-Path $BuildDir "compile_commands.json"
    if (-not (Test-Path $compileDb)) {
        Write-Warn "compile_commands.json not found -- running configure first..."
        Invoke-Configure
        if ($script:Failed) { return }
    }
    $cores = [Environment]::ProcessorCount
    $srcFwd = $SrcDir -replace "\\","/"
    Write-Ok "clang-tidy: running in parallel on $cores cores..."
    python3 $RUN_TIDY -p $BuildDir -j $cores "-header-filter=$srcFwd/.*" "$srcFwd/.*\.cpp" 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Err "clang-tidy: issues found."
        $script:Failed = $true
    } else {
        Write-Ok "clang-tidy: no issues."
    }
}

function Invoke-Cppcheck {
    if (-not (Test-Path $CPPCHECK_EXE)) {
        Write-Err "cppcheck not found at: $CPPCHECK_EXE"
        $script:Failed = $true; return
    }
    $cores = [Environment]::ProcessorCount
    Write-Ok "cppcheck: running on $cores cores..."
    & $CPPCHECK_EXE --enable=warning,performance,portability "--suppressions-list=$Root\.cppcheck-suppressions" --inline-suppr --std=c++20 --error-exitcode=1 -I $SrcDir -j $cores $SrcDir 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Err "cppcheck: issues found."
        $script:Failed = $true
    } else {
        Write-Ok "cppcheck: no issues."
    }
}

Push-Location $Root
try {
    if ($Configure)    { Invoke-Configure }
    elseif ($Fix)      { Invoke-ClangFormat $true }
    elseif ($Tidy)     { Invoke-ClangTidy }
    elseif ($Cppcheck) { Invoke-Cppcheck }
    elseif ($All) {
        Invoke-ClangFormat $false
        Write-Host ""
        Invoke-ClangTidy
        Write-Host ""
        Invoke-Cppcheck
    }
    else { Invoke-ClangFormat $false }
} finally {
    Pop-Location
}

if ($script:Failed) {
    Write-Err "Lint FAILED."
    exit 1
}
Write-Ok "All checks passed."
exit 0
