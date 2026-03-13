@echo off
REM Fincept Terminal C++ Build Script (Windows)
REM Prerequisites: CMake, Ninja, vcpkg, Visual Studio Build Tools

echo ================================================
echo  Fincept Terminal C++ Build
echo ================================================

REM Check vcpkg
if not defined VCPKG_ROOT (
    echo ERROR: VCPKG_ROOT is not set
    echo Please set VCPKG_ROOT to your vcpkg installation directory
    echo Example: set VCPKG_ROOT=C:\vcpkg
    exit /b 1
)

echo Using vcpkg at: %VCPKG_ROOT%

REM Install dependencies
echo.
echo [1/3] Installing dependencies via vcpkg...
%VCPKG_ROOT%\vcpkg install --triplet x64-windows

REM Configure
echo.
echo [2/3] Configuring with CMake...
cmake --preset default

if %errorlevel% neq 0 (
    echo CMAKE CONFIGURE FAILED
    exit /b 1
)

REM Build
echo.
echo [3/3] Building...
cmake --build build --config RelWithDebInfo

if %errorlevel% neq 0 (
    echo BUILD FAILED
    exit /b 1
)

echo.
echo ================================================
echo  Build Complete!
echo  Run: build\FinceptTerminal.exe
echo ================================================
