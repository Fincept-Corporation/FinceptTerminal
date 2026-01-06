@echo off
:: Fincept Terminal - Windows Development Environment Setup Script
:: NO ADMIN REQUIRED - All user-level installations
setlocal enabledelayedexpansion

echo.
echo ========================================
echo   Fincept Terminal Setup Script
echo   Windows Development Environment
echo   (No Admin Rights Required)
echo ========================================
echo.

:: Check for curl
where curl >nul 2>&1
if %errorlevel% neq 0 (
    echo [!!] curl not found! Please install curl first.
    echo      Download from: https://curl.se/windows/
    pause
    exit /b 1
)
echo [OK] curl available

:: Check/Install Python (User-level only, no admin)
set "PYTHON_INSTALLED=0"
where python >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] Python already installed
    python --version
    python -m pip install --upgrade pip --user
    set "PYTHON_INSTALLED=1"
) else (
    echo [..] Installing Python 3.12.7 (user-level)...
    curl -sSL https://www.python.org/ftp/python/3.12.7/python-3.12.7-amd64.exe -o python-installer.exe
    if %errorlevel% neq 0 (
        echo [!!] Failed to download Python installer
        pause
        exit /b 1
    )

    :: Install for current user only (no admin needed)
    start /wait python-installer.exe /passive InstallAllUsers=0 PrependPath=1 Include_pip=1 Include_test=0
    del python-installer.exe
    echo [OK] Python installed

    :: Refresh PATH for current session
    for /f "tokens=2*" %%a in ('reg query "HKCU\Environment" /v Path 2^>nul') do set "USR_PATH=%%b"
    for /f "tokens=2*" %%a in ('reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment" /v Path 2^>nul') do set "SYS_PATH=%%b"
    set "PATH=%USR_PATH%;%SYS_PATH%"

    :: Wait a moment for PATH to update
    timeout /t 2 /nobreak >nul

    :: Verify Python installation
    where python >nul 2>&1
    if %errorlevel% equ 0 (
        python -m pip install --upgrade pip --user
        set "PYTHON_INSTALLED=1"
    ) else (
        echo [!!] Python installed but not in PATH. Restart terminal and re-run script.
        pause
        exit /b 1
    )
)

:: Set PYO3_PYTHON (user environment variable only)
for /f "delims=" %%i in ('where python 2^>nul') do set "PYO3_PYTHON=%%i" & goto :found_python
:found_python
if defined PYO3_PYTHON (
    :: Set user environment variable (no admin needed)
    reg add "HKCU\Environment" /v PYO3_PYTHON /t REG_SZ /d "%PYO3_PYTHON%" /f >nul 2>&1
    echo [OK] PYO3_PYTHON set to %PYO3_PYTHON%
) else (
    echo [!!] Failed to set PYO3_PYTHON
    pause
    exit /b 1
)

:: Check/Install Rust
set "RUST_INSTALLED=0"
where rustc >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] Rust already installed
    rustc --version
    set "RUST_INSTALLED=1"
) else (
    echo [..] Installing Rust...
    curl -sSf https://win.rustup.rs/x86_64 -o rustup-init.exe
    if %errorlevel% neq 0 (
        echo [!!] Failed to download Rust installer
        pause
        exit /b 1
    )
    start /wait rustup-init.exe -y --default-toolchain stable
    del rustup-init.exe

    :: Add to current session PATH
    set "PATH=%USERPROFILE%\.cargo\bin;%PATH%"

    :: Verify Rust installation
    where rustc >nul 2>&1
    if %errorlevel% equ 0 (
        echo [OK] Rust installed
        rustc --version
        set "RUST_INSTALLED=1"
    ) else (
        echo [!!] Rust installed but not in PATH. Restart terminal and re-run script.
        pause
        exit /b 1
    )
)

:: Check/Install Bun
set "BUN_INSTALLED=0"
where bun >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] Bun already installed
    bun --version
    set "BUN_INSTALLED=1"
) else (
    echo [..] Installing Bun...
    powershell -ExecutionPolicy Bypass -Command "irm bun.sh/install.ps1 | iex"

    :: Add to current session PATH
    set "PATH=%USERPROFILE%\.bun\bin;%PATH%"

    :: Verify Bun installation
    where bun >nul 2>&1
    if %errorlevel% equ 0 (
        echo [OK] Bun installed
        bun --version
        set "BUN_INSTALLED=1"
    ) else (
        echo [!!] Bun installed but not in PATH. Restart terminal and re-run script.
        pause
        exit /b 1
    )
)

:: Check for Visual Studio Build Tools
where cl >nul 2>&1
if %errorlevel% neq 0 (
    echo.
    echo [!!] Visual Studio Build Tools not found!
    echo      This is REQUIRED for Tauri to build!
    echo      Download from: https://visualstudio.microsoft.com/visual-cpp-build-tools/
    echo      Select "Desktop development with C++" workload
    echo      OR install via winget (no admin): winget install Microsoft.VisualStudio.2022.BuildTools
    echo.
    set /p CONTINUE="Continue anyway? (y/N): "
    if /i not "!CONTINUE!"=="y" exit /b 1
) else (
    echo [OK] Visual Studio Build Tools found
)

:: Check for WebView2 Runtime
reg query "HKLM\SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}" >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] WebView2 Runtime found
) else (
    reg query "HKCU\SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}" >nul 2>&1
    if %errorlevel% equ 0 (
        echo [OK] WebView2 Runtime found (user install)
    ) else (
        echo.
        echo [!!] WebView2 Runtime not found!
        echo      This is REQUIRED for Tauri apps on Windows!
        echo      Downloading WebView2 Runtime (user install)...
        curl -sSL "https://go.microsoft.com/fwlink/p/?LinkId=2124703" -o webview2_installer.exe
        if %errorlevel% equ 0 (
            :: Try user-level install first (no admin)
            start /wait webview2_installer.exe /install
            del webview2_installer.exe
            echo [OK] WebView2 Runtime installed
        ) else (
            echo [!!] Failed to download WebView2
            echo      Download manually: https://developer.microsoft.com/microsoft-edge/webview2/
            echo      Or it will be installed when you first run the app
            echo.
        )
    )
)

:: Navigate to project directory
echo.
echo [..] Navigating to project directory...
cd /d "%~dp0.."
if exist "fincept-terminal-desktop" (
    cd fincept-terminal-desktop
    echo [OK] Found fincept-terminal-desktop directory
) else (
    echo [!!] fincept-terminal-desktop directory not found!
    echo      Current directory: %CD%
    pause
    exit /b 1
)

:: Install project dependencies
echo.
echo [..] Installing project dependencies...
call bun install
if %errorlevel% neq 0 (
    echo [!!] Failed to install dependencies with bun
    pause
    exit /b 1
)
echo [OK] Dependencies installed

echo [..] Adding Tauri CLI...
call bun add -d @tauri-apps/cli
if %errorlevel% neq 0 (
    echo [!!] Failed to add Tauri CLI
    pause
    exit /b 1
)
echo [OK] Tauri CLI added

:: Install Python packages for analytics (user-level only)
echo.
echo [..] Installing Python packages for analytics...
python -m pip install --upgrade pip setuptools wheel --user
python -m pip install pandas numpy yfinance requests langchain-ollama ccxt sqlalchemy --user
if %errorlevel% neq 0 (
    echo [!] Some packages failed to install, but continuing...
)
echo [OK] Python packages installed

echo.
echo ========================================
echo   Setup Complete!
echo ========================================
echo.
echo Next steps:
echo   1. RESTART your terminal to ensure PATH is updated
echo   2. cd fincept-terminal-desktop
echo   3. bun run tauri:dev
echo.
echo Note: If commands not found, restart terminal first!
echo ========================================
echo.
pause
