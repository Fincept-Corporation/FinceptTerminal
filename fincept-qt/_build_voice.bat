@echo off
cd /d C:\windowsdisk\finceptTerminal\fincept-qt
call "C:\Program Files\Microsoft Visual Studio\2022\Preview\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cmake --build --preset win-release > build_voice.log 2>&1
exit /b %ERRORLEVEL%
