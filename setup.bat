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
echo   Fincept Terminal — Windows Setup
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
    echo   MSVC compiler not found in PATH.
    echo   Please install Visual Studio 2022 with "Desktop development with C++" workload:
    echo   https://visualstudio.microsoft.com/
    echo   Then re-run from a "Developer Command Prompt for VS 2022".
    if "!CI_MODE!"=="false" pause
    exit /b 1
)
echo   OK

:: ── Locate Qt6 ──────────────────────────────────────────────
echo [5/5] Locating Qt6 and building...

:: Try to auto-detect Qt6 install (common paths)
set "QT_PREFIX="
if exist "C:\Qt\6.8.3\msvc2022_64\lib\cmake\Qt6\Qt6Config.cmake" (
    set "QT_PREFIX=C:\Qt\6.8.3\msvc2022_64"
) else if exist "C:\Qt\6.7.0\msvc2022_64\lib\cmake\Qt6\Qt6Config.cmake" (
    set "QT_PREFIX=C:\Qt\6.7.0\msvc2022_64"
) else if exist "C:\Qt\6.6.0\msvc2022_64\lib\cmake\Qt6\Qt6Config.cmake" (
    set "QT_PREFIX=C:\Qt\6.6.0\msvc2022_64"
)

:: Allow override via environment variable
if defined Qt6_DIR (
    set "QT_PREFIX=!Qt6_DIR!"
)

if "!QT_PREFIX!"=="" (
    if "!CI_MODE!"=="true" (
        echo   ERROR: Qt6 not found. Set Qt6_DIR environment variable to your Qt6 install path.
        exit /b 1
    )
    echo.
    echo   Qt6 not found in common locations.
    echo   Please install Qt6 from https://www.qt.io/download-qt-installer
    echo   (Select: Qt 6.x ^> MSVC 2022 64-bit)
    echo.
    echo   After installing, set the Qt6_DIR environment variable:
    echo     set Qt6_DIR=C:\Qt\6.x.x\msvc2022_64
    echo   Then re-run this script.
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
echo ================================================
echo.
echo   Run the terminal:
echo     build\Release\FinceptTerminal.exe
echo.

:: Skip launch prompt in CI
if "!CI_MODE!"=="true" goto :done

set /p LAUNCH="Launch now? (y/n): "
if /i "!LAUNCH!"=="y" (
    start "" "build\Release\FinceptTerminal.exe"
)

:done
endlocal
