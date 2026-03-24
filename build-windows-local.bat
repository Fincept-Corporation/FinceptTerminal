@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Preview\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b 1

set QT_ROOT=C:\Qt\6.8.3\msvc2022_64
set BUILD_DIR=C:\windowsdisk\finceptTerminal\fincept-qt\build-win

echo === CMake Configure ===
cmake -B "%BUILD_DIR%" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_PREFIX_PATH="%QT_ROOT%" ^
  -S C:\windowsdisk\finceptTerminal\fincept-qt
if errorlevel 1 (echo CONFIGURE FAILED & exit /b 1)

echo === CMake Build ===
cmake --build "%BUILD_DIR%" --config Release --parallel
if errorlevel 1 (echo BUILD FAILED & exit /b 1)

echo === Verify ===
if exist "%BUILD_DIR%\Release\FinceptTerminal.exe" (
  echo BUILD SUCCEEDED
  dir "%BUILD_DIR%\Release\FinceptTerminal.exe"
) else (
  echo ERROR: Binary not found
  exit /b 1
)
