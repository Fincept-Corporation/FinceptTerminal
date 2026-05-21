@echo off
setlocal

set ROOT_DIR=%~dp0

echo [0/2] Initializing MSVC environment...
set "VS_WHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VS_WHERE%" (
    echo Error: vswhere.exe not found. Ensure Visual Studio 2022 is installed.
    pause
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VS_WHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_PATH=%%i"
)

if not exist "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" (
    echo Error: vcvarsall.bat not found at %VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat
    echo Attempting common BuildTools path...
    set "VS_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools"
)

if exist "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" (
    call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x64
) else (
    echo Error: Could not find vcvarsall.bat. Please ensure MSVC is installed correctly.
    pause
    exit /b 1
)

set QT_PATH=%ROOT_DIR%.qt\6.8.3\msvc2022_64
set APP_DIR=%ROOT_DIR%fincept-qt
set BUILD_DIR=%APP_DIR%\build\win-release
set EXE_PATH=%BUILD_DIR%\FinceptTerminal.exe

echo ================================================
echo   Fincept Terminal Local Runner (Windows)
echo ================================================

if not exist "%EXE_PATH%" (
    echo [1/2] App not built yet. Starting build...

    cd /d "%APP_DIR%"
    python -m cmake --build . --preset win-release
    if %ERRORLEVEL% neq 0 (
        echo Error: Build failed.
        pause
        exit /b %ERRORLEVEL%
    )
) else (
    echo [1/2] Found existing build. (Run setup_windows.py again if you want a clean rebuild)
)

echo [2/2] Launching Fincept Terminal...
echo Mode: Personal (Auth Bypassed)
echo.

rem Add Qt DLLs to PATH for runtime
set PATH=%QT_PATH%\bin;%PATH%

start "" "%EXE_PATH%"

echo App launched. This window will close.
timeout /t 3 > nul
exit /b 0
