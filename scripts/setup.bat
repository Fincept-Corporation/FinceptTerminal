@echo off
:: Fincept Terminal - Windows Development Environment Setup Script
setlocal enabledelayedexpansion

echo.
echo ========================================
echo   Fincept Terminal Setup Script
echo   Windows Development Environment
echo ========================================
echo.

:: Check for admin rights
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] Some installations may require admin rights.
    echo     Right-click and "Run as Administrator" if needed.
    echo.
)

:: Check for curl
where curl >nul 2>&1
if %errorlevel% neq 0 (
    echo [!!] curl not found! Please install curl first.
    echo      Download from: https://curl.se/windows/
    pause
    exit /b 1
)
echo [OK] curl available

:: Check/Install Python
where python >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] Python already installed
    python --version
) else (
    echo [..] Installing Python...
    curl -sSL https://www.python.org/ftp/python/3.12.7/python-3.12.7-amd64.exe -o python-installer.exe
    python-installer.exe /quiet InstallAllUsers=1 PrependPath=1 Include_pip=1
    del python-installer.exe
    echo [OK] Python installed - RESTART terminal to use
)

:: Set PYO3_PYTHON
for /f "delims=" %%i in ('where python') do set "PYO3_PYTHON=%%i" & goto :found_python
:found_python
setx PYO3_PYTHON "%PYO3_PYTHON%" >nul 2>&1
echo [OK] PYO3_PYTHON set to %PYO3_PYTHON%

:: Check/Install Rust
where rustc >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] Rust already installed
    rustc --version
) else (
    echo [..] Installing Rust...
    curl -sSf https://win.rustup.rs/x86_64 -o rustup-init.exe
    rustup-init.exe -y
    del rustup-init.exe
    set "PATH=%USERPROFILE%\.cargo\bin;%PATH%"
    echo [OK] Rust installed
)

:: Check/Install Bun
where bun >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] Bun already installed
    bun --version
) else (
    echo [..] Installing Bun...
    powershell -Command "irm bun.sh/install.ps1 | iex"
    set "PATH=%USERPROFILE%\.bun\bin;%PATH%"
    echo [OK] Bun installed
)

:: Check for Visual Studio Build Tools
where cl >nul 2>&1
if %errorlevel% neq 0 (
    echo.
    echo [!!] Visual Studio Build Tools not found!
    echo      Download from: https://visualstudio.microsoft.com/visual-cpp-build-tools/
    echo      Select "Desktop development with C++" workload
    echo.
    pause
)

:: Check for WebView2
reg query "HKLM\SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}" >nul 2>&1
if %errorlevel% neq 0 (
    echo.
    echo [!!] WebView2 Runtime not found!
    echo      Download from: https://developer.microsoft.com/microsoft-edge/webview2/
    echo.
)

:: Install project dependencies
echo.
echo [..] Installing project dependencies...
cd /d "%~dp0..\fincept-terminal-desktop"
if not exist "fincept-terminal-desktop" cd /d "%~dp0.."
call bun install
call bun add -d @tauri-apps/cli

echo.
echo ========================================
echo   Setup Complete!
echo   Run: cd fincept-terminal-desktop
echo        bun run tauri:dev
echo ========================================
echo.
pause
