@echo off
setlocal enabledelayedexpansion

REM Sign wrapper batch file for Tauri builds
REM Parameter %1 is the file to sign

echo Checking file: %~1

REM Skip Python files
echo %~1 | findstr /i "\\python\\" >nul
if !errorlevel! equ 0 (
    echo SKIPPED Python file
    exit /b 0
)

REM Sign other files
echo Signing with relic...
"C:\Program Files\Relic\relic.exe" sign --file "%~1" --key azure --config "%~dp0relic.conf"
if !errorlevel! neq 0 (
    echo ERROR: Signing failed with error code !errorlevel!
    exit /b !errorlevel!
)

echo Successfully signed: %~1
exit /b 0
