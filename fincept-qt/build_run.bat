@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Preview\VC\Auxiliary\Build\vcvars64.bat" > C:\windowsdisk\finceptTerminal\fincept-qt\build_log.txt 2>&1
echo VCVARS loaded >> C:\windowsdisk\finceptTerminal\fincept-qt\build_log.txt
cmake --build "C:\windowsdisk\finceptTerminal\fincept-qt\build" --config Release >> C:\windowsdisk\finceptTerminal\fincept-qt\build_log.txt 2>&1
echo BUILD_EXIT=%ERRORLEVEL% >> C:\windowsdisk\finceptTerminal\fincept-qt\build_log.txt
