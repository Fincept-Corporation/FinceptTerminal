@echo off
set "PATH=C:\Program Files (x86)\Microsoft Visual Studio\Installer;%PATH%"
call "C:\Program Files\Microsoft Visual Studio\2022\Preview\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "C:\windowsdisk\finceptTerminal\fincept-qt"
cmake --build build-win --config Release --parallel
exit /b %errorlevel%
