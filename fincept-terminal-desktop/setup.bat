@echo off
REM Fincept Terminal - Python Setup Script
REM Downloads and configures Python embeddable package for contributors

echo ========================================
echo Fincept Terminal - Python Setup
echo ========================================
echo.

REM Check if Python is already installed
if exist "src-tauri\resources\python\python.exe" (
    echo Python is already installed.
    goto :INSTALL_DEPS
)

echo [1/4] Downloading Python 3.12.8 embeddable package...
curl -L -o python-embed.zip https://www.python.org/ftp/python/3.12.8/python-3.12.8-embed-amd64.zip
if errorlevel 1 (
    echo ERROR: Failed to download Python
    pause
    exit /b 1
)

echo [2/4] Extracting Python...
powershell -Command "Expand-Archive -Path 'python-embed.zip' -DestinationPath 'src-tauri\resources\python' -Force"
del python-embed.zip

echo [3/4] Configuring Python for pip...
powershell -Command "(Get-Content src-tauri\resources\python\python312._pth) -replace '#import site', 'import site' | Set-Content src-tauri\resources\python\python312._pth"

echo [4/4] Installing pip...
curl -o src-tauri\resources\python\get-pip.py https://bootstrap.pypa.io/get-pip.py
cd src-tauri\resources\python
python.exe get-pip.py
cd ..\..\..

:INSTALL_DEPS
echo.
echo Installing Python dependencies...
cd src-tauri\resources\python
python.exe -m pip install setuptools wheel
python.exe -m pip install -r ..\requirements.txt
cd ..\..\..

echo.
echo ========================================
echo Setup Complete!
echo ========================================
echo Python is ready at: src-tauri\resources\python\
echo.
echo You can now run: npm run dev
echo.
pause
