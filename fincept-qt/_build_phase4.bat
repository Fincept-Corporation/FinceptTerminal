@echo off
set "PATH=C:\Program Files (x86)\Microsoft Visual Studio\Installer;%PATH%"
call "C:\Program Files\Microsoft Visual Studio\2022\Preview\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "C:\windowsdisk\finceptTerminal\fincept-qt"
cmake -S . -B build-win -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
if errorlevel 1 exit /b 1
cmake --build build-win --config Release --parallel
exit /b %errorlevel%
