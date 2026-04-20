@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Preview\VC\Auxiliary\Build\vcvars64.bat" >nul
cmake --build --preset win-release 2>&1
exit /b %errorlevel%
