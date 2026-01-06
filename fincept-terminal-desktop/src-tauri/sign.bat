@echo off
REM Simple sign wrapper - no spaces in path
echo Signing: %1
echo %1 | findstr /i "\\python\\" >nul && (echo SKIP: Python file && exit /b 0)
relic sign --file %1 --key azure --config %~dp0relic.conf
exit /b %errorlevel%
