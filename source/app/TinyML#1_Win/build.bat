@echo off
setlocal
cd /d "%~dp0"

where mingw32-make >nul 2>nul
if errorlevel 1 (
    echo ERROR: mingw32-make was not found on PATH.
    exit /b 1
)

where gcc >nul 2>nul
if errorlevel 1 (
    echo ERROR: gcc was not found on PATH.
    exit /b 1
)

where g++ >nul 2>nul
if errorlevel 1 (
    echo ERROR: g++ was not found on PATH.
    exit /b 1
)

mingw32-make
exit /b %errorlevel%
