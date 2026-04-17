@echo off
setlocal EnableDelayedExpansion
title Fincept Terminal - Setup

:: ── Pinned versions (must match CMakeLists.txt) ─────────────
set "QT_VERSION=6.7.2"
set "QT_ARCH=win64_msvc2022_64"
set "QT_KIT_DIR=msvc2022_64"
set "MSVC_MIN=19.38"
set "PYTHON_MIN=3.11"

:: ── Parse args ──────────────────────────────────────────────
set CI_MODE=false
for %%A in (%*) do (
    if /i "%%A"=="--ci" set CI_MODE=true
)

echo.
echo ================================================
echo   Fincept Terminal v4.0.1 - Windows Setup
echo   Pinned: Qt %QT_VERSION% ^| MSVC 2022 17.8+ ^| Python %PYTHON_MIN%+
if "!CI_MODE!"=="true" echo   (CI mode -- skipping interactive steps)
echo ================================================
echo.

:: ── Step 1: Git ─────────────────────────────────────────────
echo [1/7] Checking Git...
where git >nul 2>&1
if errorlevel 1 (
    echo   ERROR: Git not found. Install from https://git-scm.com/downloads
    if "!CI_MODE!"=="false" pause
    exit /b 1
)
echo   OK

:: ── Step 2: CMake ≥ 3.27 ────────────────────────────────────
echo [2/7] Checking CMake ^>= 3.27...
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
        pause ^& exit /b 1
    )
)
for /f "tokens=3" %%V in ('cmake --version ^| findstr /R "cmake version"') do set "CMAKE_VER=%%V"
echo   cmake !CMAKE_VER!
echo   OK

:: ── Step 3: Ninja ───────────────────────────────────────────
echo [3/7] Checking Ninja...
where ninja >nul 2>&1
if errorlevel 1 (
    if "!CI_MODE!"=="true" (
        echo   ERROR: Ninja not found in CI environment.
        exit /b 1
    )
    echo   Ninja not found. Installing via winget...
    winget install --id Ninja-build.Ninja -e --silent
    if errorlevel 1 (
        echo   ERROR: Ninja install failed. Install manually from https://github.com/ninja-build/ninja/releases
        pause ^& exit /b 1
    )
)
echo   OK

:: ── Step 4: Python ≥ 3.11 ───────────────────────────────────
echo [4/7] Checking Python ^>= %PYTHON_MIN%...
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
        pause ^& exit /b 1
    )
)
echo   OK

:: ── Step 5: MSVC 2022 17.8+ ─────────────────────────────────
echo [5/7] Checking MSVC (VS 2022 17.8+)...
where cl >nul 2>&1
if errorlevel 1 (
    set "VCVARS="
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
        set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
        set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
        set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    )
    if defined VCVARS (
        echo   Activating VS 2022 environment...
        call "!VCVARS!"
    ) else (
        echo   ERROR: MSVC not found. Install Visual Studio 2022 17.8+ with "Desktop development with C++":
        echo   https://visualstudio.microsoft.com/downloads/
        if "!CI_MODE!"=="false" pause
        exit /b 1
    )
)
:: cl prints its version on stderr; capture first line
for /f "tokens=*" %%L in ('cl 2^>^&1 ^| findstr /R "Microsoft.*Compiler"') do set "CL_LINE=%%L"
echo   !CL_LINE!
echo   OK

:: ── Step 6: Install pinned Qt %QT_VERSION% via aqtinstall ──
set "SCRIPT_DIR=%~dp0"
set "QT_INSTALL_ROOT=%SCRIPT_DIR%.qt"
if defined FINCEPT_QT_ROOT set "QT_INSTALL_ROOT=%FINCEPT_QT_ROOT%"
set "QT_PREFIX=%QT_INSTALL_ROOT%\%QT_VERSION%\%QT_KIT_DIR%"

echo [6/7] Locating Qt %QT_VERSION%...

:: If user already has Qt6_DIR set and valid, honor it
if defined Qt6_DIR (
    if exist "%Qt6_DIR%\lib\cmake\Qt6\Qt6Config.cmake" (
        set "QT_PREFIX=%Qt6_DIR%"
        echo   Using Qt from Qt6_DIR env: !QT_PREFIX!
        goto :qt_ready
    )
)

:: Check default Qt Online Installer location (C:\Qt\6.7.2\msvc2022_64)
if exist "C:\Qt\%QT_VERSION%\%QT_KIT_DIR%\lib\cmake\Qt6\Qt6Config.cmake" (
    set "QT_PREFIX=C:\Qt\%QT_VERSION%\%QT_KIT_DIR%"
    echo   Found Qt at !QT_PREFIX!
    goto :qt_ready
)

:: Check aqtinstall location (%SCRIPT_DIR%.qt\6.7.2\msvc2022_64)
if exist "%QT_PREFIX%\lib\cmake\Qt6\Qt6Config.cmake" (
    echo   Found Qt at !QT_PREFIX!
    goto :qt_ready
)

:: Install via aqtinstall
echo   Installing Qt %QT_VERSION% via aqtinstall to %QT_INSTALL_ROOT% ...
python -m pip install --user --quiet --upgrade aqtinstall
if errorlevel 1 (
    echo   ERROR: Failed to install aqtinstall. Check internet/pip.
    if "!CI_MODE!"=="false" pause
    exit /b 1
)
python -m aqt install-qt windows desktop %QT_VERSION% %QT_ARCH% ^
    --outputdir "%QT_INSTALL_ROOT%" ^
    --modules qtcharts qtwebsockets qtmultimedia qtspeech
if errorlevel 1 (
    echo   ERROR: aqtinstall failed. Install Qt %QT_VERSION% manually from https://www.qt.io/download-qt-installer
    if "!CI_MODE!"=="false" pause
    exit /b 1
)
if not exist "%QT_PREFIX%\lib\cmake\Qt6\Qt6Config.cmake" (
    echo   ERROR: Qt install completed but Qt6Config.cmake not found at %QT_PREFIX%
    if "!CI_MODE!"=="false" pause
    exit /b 1
)

:qt_ready
echo   CMAKE_PREFIX_PATH=!QT_PREFIX!
echo   OK

:: ── Step 7: Configure + Build (using CMake preset) ──────────
cd /d "%SCRIPT_DIR%fincept-qt"
if errorlevel 1 (
    echo   ERROR: fincept-qt directory not found.
    exit /b 1
)

echo [7/7] Configuring and building (preset: win-release)...
cmake --preset win-release -DCMAKE_PREFIX_PATH="!QT_PREFIX!"
if errorlevel 1 ( echo   ERROR: CMake configure failed. ^& exit /b 1 )

cmake --build --preset win-release
if errorlevel 1 ( echo   ERROR: Build failed. ^& exit /b 1 )

echo.
echo ================================================
echo   Build complete!
echo   Run: build\win-release\FinceptTerminal.exe
echo ================================================
echo.

if "!CI_MODE!"=="true" goto :done

set /p LAUNCH="Launch now? (y/n): "
if /i "!LAUNCH!"=="y" (
    start "" "build\win-release\FinceptTerminal.exe"
)

:done
endlocal
