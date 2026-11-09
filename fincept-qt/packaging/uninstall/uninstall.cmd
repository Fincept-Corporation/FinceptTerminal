@echo off
rem ---------------------------------------------------------------------------
rem uninstall.cmd - remove Fincept Terminal and everything it created (Windows)
rem
rem   Double-click it, or run:  uninstall.cmd [-DryRun] [-Yes] [-KeepData]
rem                                           [-DataOnly] [-SharedCaches]
rem                                           [-DataDir "D:\FinceptData"]
rem
rem Runs uninstall.ps1 (next to this file) without changing the PowerShell
rem execution policy. The script is copied to %TEMP% first and run from there,
rem so the install folder holding these files can be deleted while it runs.
rem ---------------------------------------------------------------------------
setlocal
set "FT_SRC=%~dp0uninstall.ps1"
set "FT_RUN=%TEMP%\fincept-uninstall.ps1"
set "FT_FROM=%~dp0."
if not exist "%FT_SRC%" (
    echo uninstall.ps1 was not found next to %~nx0.
    exit /b 1
)
copy /y "%FT_SRC%" "%FT_RUN%" >nul || (
    echo Could not copy the uninstaller to %TEMP%.
    exit /b 1
)

rem Started by double-click (Explorer runs "cmd /c <this file>"): keep the
rem window open at the end so the summary can be read.
set "FT_PAUSE="
echo %cmdcmdline% | findstr /i /c:"%~nx0" >nul && set "FT_PAUSE=-PauseAtEnd"

rem Leave the install folder so nothing holds it open.
cd /d "%TEMP%"

rem Run, then exit on the same line: once uninstall.ps1 removes the install
rem folder this file no longer exists, and cmd must not read another line.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%FT_RUN%" -FromDir "%FT_FROM%" %FT_PAUSE% %* & exit /b
