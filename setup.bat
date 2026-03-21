@echo off
setlocal EnableDelayedExpansion
title Fincept Terminal — Setup

:: ── Parse args ──────────────────────────────────────────────
set CI_MODE=false
for %%A in (%*) do (
    if /i "%%A"=="--ci" set CI_MODE=true
)

echo.
echo ================================================
echo   Fincept Terminal v4.0.0 — Windows Setup
if "!CI_MODE!"=="true" echo   (CI mode -- skipping interactive steps)
echo ================================================
echo.

:: ── Check: Git ──────────────────────────────────────────────
echo [1/5] Checking Git...
where git >nul 2>&1
if errorlevel 1 (
    echo   ERROR: Git not found. Install from https://git-scm.com/downloads
    if "!CI_MODE!"=="false" pause
    exit /b 1
)
echo   OK

:: ── Check: CMake ────────────────────────────────────────────
echo [2/5] Checking CMake...
where cmake >nul 2>&1
if errorlevel 1 (
    if "!CI_MODE!"=="true" (
        echo   ERROR: CMake not found in CI environment.
        exit /b 1
    )
    echo   CMake not found. Installing via winget...
    winget install --id Kitware.CMake -e --silent
    if errorlevel 1 (
        echo   ERROR: CMake install failed. Install manually from https://cmake.org/download/
        pause & exit /b 1
    )
)
echo   OK

:: ── Check: Python ───────────────────────────────────────────
echo [3/5] Checking Python...
where python >nul 2>&1
if errorlevel 1 (
    if "!CI_MODE!"=="true" (
        echo   ERROR: Python not found in CI environment.
        exit /b 1
    )
    echo   Python not found. Installing via winget...
    winget install --id Python.Python.3.11 -e --silent
    if errorlevel 1 (
        echo   ERROR: Python install failed. Install from https://www.python.org/downloads/
        pause & exit /b 1
    )
)
echo   OK

:: ── Check: MSVC ─────────────────────────────────────────────
echo [4/5] Checking C++ compiler (MSVC)...
where cl >nul 2>&1
if errorlevel 1 (
    :: Try to activate VS 2022 environment automatically
    set "VCVARS="
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
        set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
        set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
    )
    if defined VCVARS (
        echo   Activating VS 2022 environment...
        call "!VCVARS!"
    ) else (
        echo   MSVC not found. Install Visual Studio 2022 with "Desktop development with C++":
        echo   https://visualstudio.microsoft.com/
        if "!CI_MODE!"=="false" pause
        exit /b 1
    )
)
echo   OK

:: ── Locate Qt6 ──────────────────────────────────────────────
echo [5/5] Locating Qt6 and building...

set "QT_PREFIX="

:: Allow override via environment variable first
if defined Qt6_DIR (
    set "QT_PREFIX=!Qt6_DIR!"
)

:: Auto-detect common Qt6 install paths
if "!QT_PREFIX!"=="" (
    for %%V in (6.9.0 6.8.3 6.8.0 6.7.0 6.6.0) do (
        if exist "C:\Qt\%%V\msvc2022_64\lib\cmake\Qt6\Qt6Config.cmake" (
            set "QT_PREFIX=C:\Qt\%%V\msvc2022_64"
            goto :qt_found
        )
    )
)

:qt_found
if "!QT_PREFIX!"=="" (
    if "!CI_MODE!"=="true" (
        echo   ERROR: Qt6 not found. Set Qt6_DIR to your Qt6 install path.
        exit /b 1
    )
    echo.
    echo   Qt6 not found. Install from https://www.qt.io/download-qt-installer
    echo   Select: Qt 6.x ^> MSVC 2022 64-bit
    echo.
    echo   Then set the environment variable and re-run:
    echo     set Qt6_DIR=C:\Qt\6.x.x\msvc2022_64
    pause & exit /b 1
)
echo   Qt6 found at !QT_PREFIX!
echo   OK

:: ── Build ────────────────────────────────────────────────────
echo.
echo Configuring...
cd /d "%~dp0fincept-qt"
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="!QT_PREFIX!"
if errorlevel 1 ( echo   ERROR: CMake configure failed. & exit /b 1 )

echo Compiling...
cmake --build build --config Release --parallel
if errorlevel 1 ( echo   ERROR: Build failed. & exit /b 1 )

echo.
echo ================================================
echo   Build complete!
echo   Run: build\Release\FinceptTerminal.exe
echo ================================================
echo.

if "!CI_MODE!"=="true" goto :done

set /p LAUNCH="Launch now? (y/n): "
if /i "!LAUNCH!"=="y" (
    start "" "build\Release\FinceptTerminal.exe"
)

:done
endlocal
