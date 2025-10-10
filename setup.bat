@echo off
REM Fincept Terminal - Automated Setup Script for Windows
REM This script installs Node.js and Rust, then sets up the project

setlocal enabledelayedexpansion

echo =======================================
echo   Fincept Terminal Setup
echo   Automated installer for Windows
echo =======================================
echo.

REM Check for admin rights
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [ERROR] This script requires Administrator privileges.
    echo Right-click setup.bat and select "Run as administrator"
    pause
    exit /b 1
)

REM Check if Node.js is installed
echo [INFO] Checking for Node.js...
where node >nul 2>&1
if %errorLevel% equ 0 (
    for /f "tokens=*" %%i in ('node -v') do set NODE_VERSION=%%i
    echo [SUCCESS] Node.js is already installed: !NODE_VERSION!
) else (
    echo [INFO] Node.js not found. Installing Node.js LTS v22.14.0...

    REM Download Node.js LTS installer
    echo [INFO] Downloading Node.js installer...
    powershell -Command "& {Invoke-WebRequest -Uri 'https://nodejs.org/dist/v22.14.0/node-v22.14.0-x64.msi' -OutFile '%TEMP%\nodejs-installer.msi'}"

    if %errorLevel% neq 0 (
        echo [ERROR] Failed to download Node.js installer
        pause
        exit /b 1
    )

    echo [INFO] Installing Node.js (this may take a few minutes)...
    msiexec /i "%TEMP%\nodejs-installer.msi" /qn /norestart

    REM Wait for installation to complete
    timeout /t 5 /nobreak >nul

    REM Refresh environment variables
    call :RefreshEnv

    echo [SUCCESS] Node.js installed successfully!
)

REM Check if Rust is installed
echo [INFO] Checking for Rust...
where rustc >nul 2>&1
if %errorLevel% equ 0 (
    for /f "tokens=*" %%i in ('rustc --version') do set RUST_VERSION=%%i
    echo [SUCCESS] Rust is already installed: !RUST_VERSION!
) else (
    echo [INFO] Rust not found. Installing Rust via rustup...

    REM Download rustup-init.exe
    echo [INFO] Downloading Rust installer...
    powershell -Command "& {Invoke-WebRequest -Uri 'https://static.rust-lang.org/rustup/dist/x86_64-pc-windows-msvc/rustup-init.exe' -OutFile '%TEMP%\rustup-init.exe'}"

    if %errorLevel% neq 0 (
        echo [ERROR] Failed to download Rust installer
        pause
        exit /b 1
    )

    echo [INFO] Installing Rust (this may take a few minutes)...
    "%TEMP%\rustup-init.exe" -y --default-toolchain stable

    REM Refresh environment variables
    call :RefreshEnv

    echo [SUCCESS] Rust installed successfully!
)

REM Navigate to project directory
echo [INFO] Navigating to project directory...
cd /d "%~dp0fincept-terminal-desktop"
if %errorLevel% neq 0 (
    echo [ERROR] Failed to find fincept-terminal-desktop directory
    pause
    exit /b 1
)

REM Install npm dependencies
echo [INFO] Installing npm dependencies...
call npm install

if %errorLevel% neq 0 (
    echo [ERROR] Failed to install dependencies
    pause
    exit /b 1
)

echo [SUCCESS] Dependencies installed successfully!

echo.
echo =======================================
echo [SUCCESS] Setup Complete!
echo =======================================
echo.
echo To start development:
echo   cd fincept-terminal-desktop
echo   npm run tauri dev
echo.
echo To build for production:
echo   npm run tauri build
echo.
pause
exit /b 0

REM Function to refresh environment variables
:RefreshEnv
for /f "tokens=2*" %%a in ('reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment" /v Path 2^>nul') do set "SysPath=%%b"
for /f "tokens=2*" %%a in ('reg query "HKCU\Environment" /v Path 2^>nul') do set "UserPath=%%b"
set "PATH=%SysPath%;%UserPath%;%USERPROFILE%\.cargo\bin"
exit /b 0
