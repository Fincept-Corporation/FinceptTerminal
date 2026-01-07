@echo off
set PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1
set PATH=%USERPROFILE%\.cargo\bin;%PATH%
cd /d C:\Users\avira\FinceptTerminal\fincept-terminal-desktop
npm run tauri dev
