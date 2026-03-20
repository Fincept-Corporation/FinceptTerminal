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
echo [1/6] Checking Git...
where git >nul 2>&1
if errorlevel 1 (
    echo   ERROR: Git not found. Install from https://git-scm.com/downloads
    if "!CI_MODE!"=="false" pause
    exit /b 1
)
echo   OK

:: ── Check: CMake ────────────────────────────────────────────
echo [2/6] Checking CMake...
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

:: ── Check: Ninja ────────────────────────────────────────────
echo [3/6] Checking Ninja...
where ninja >nul 2>&1
if errorlevel 1 (
    if "!CI_MODE!"=="true" (
        echo   ERROR: Ninja not found in CI environment.
        exit /b 1
    )
    echo   Ninja not found. Installing via winget...
    winget install --id Ninja-build.Ninja -e --silent
    if errorlevel 1 (
        echo   ERROR: Ninja install failed. Install manually from https://ninja-build.org/
        pause & exit /b 1
    )
)
echo   OK

:: ── Check: Python ───────────────────────────────────────────
echo [4/6] Checking Python...
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
echo [5/6] Checking C++ compiler (MSVC)...
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

:: ── vcpkg ───────────────────────────────────────────────────
echo [6/6] Setting up vcpkg and building...
if not defined VCPKG_ROOT (
    set "VCPKG_ROOT=%USERPROFILE%\vcpkg"
)
if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo   vcpkg not found. Cloning into %VCPKG_ROOT%...
    git clone https://github.com/microsoft/vcpkg.git "%VCPKG_ROOT%"
    if errorlevel 1 ( echo   ERROR: Failed to clone vcpkg. & exit /b 1 )
    call "%VCPKG_ROOT%\bootstrap-vcpkg.bat" -disableMetrics
    if errorlevel 1 ( echo   ERROR: vcpkg bootstrap failed. & exit /b 1 )
    echo   vcpkg installed at %VCPKG_ROOT%
    :: Persist for future sessions (skip in CI — env var already set)
    if "!CI_MODE!"=="false" (
        setx VCPKG_ROOT "%VCPKG_ROOT%" >nul
    )
) else (
    echo   vcpkg already installed at %VCPKG_ROOT%
)
echo   OK

:: ── Build ────────────────────────────────────────────────────
echo.
echo Configuring (vcpkg downloads dependencies -- may take 10-20 min on first run)...
cd /d "%~dp0fincept-cpp"
cmake --preset=default -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
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
