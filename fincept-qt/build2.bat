@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Preview\VC\Auxiliary\Build\vcvars64.bat"
cmake --build build --config Release > build_output.txt 2>&1
echo EXIT_CODE=%ERRORLEVEL% >> build_output.txt
