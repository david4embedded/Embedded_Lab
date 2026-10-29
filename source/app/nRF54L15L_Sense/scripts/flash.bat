@echo off
setlocal

set "TOOLCHAIN=C:\ncs\toolchains\fd21892d0f"
set "NCS_ROOT=C:\ncs\v3.2.4"
set "BUILD_DIR=C:\Repository\Embedded_Lab\source\app\nRF54L15L_Sense\build"
set "OPENOCD_BIN=C:\openocd\OpenOCD-20260302-0.12.0\bin"

if not exist "%TOOLCHAIN%\opt\zephyr-sdk" (
    echo ERROR: Zephyr SDK not found: "%TOOLCHAIN%\opt\zephyr-sdk"
    exit /b 1
)
if not exist "%NCS_ROOT%\.west" (
    echo ERROR: nRF Connect SDK workspace not found: "%NCS_ROOT%"
    exit /b 1
)
if not exist "%BUILD_DIR%\zephyr" (
    echo ERROR: Build directory not found or not configured: "%BUILD_DIR%"
    exit /b 1
)
if not exist "%OPENOCD_BIN%\openocd.exe" (
    echo ERROR: OpenOCD not found: "%OPENOCD_BIN%\openocd.exe"
    exit /b 1
)

set "PATH=%TOOLCHAIN%;%TOOLCHAIN%\mingw64\bin;%TOOLCHAIN%\bin;%TOOLCHAIN%\opt\bin;%TOOLCHAIN%\opt\bin\Scripts;%TOOLCHAIN%\opt\nanopb\generator-bin;%TOOLCHAIN%\nrfutil\bin;%TOOLCHAIN%\opt\zephyr-sdk\arm-zephyr-eabi\bin;%OPENOCD_BIN%;%PATH%"
set "PYTHONPATH=%TOOLCHAIN%\opt\bin;%TOOLCHAIN%\opt\bin\Lib;%TOOLCHAIN%\opt\bin\Lib\site-packages"
set "NRFUTIL_HOME=%TOOLCHAIN%\nrfutil\home"
set "ZEPHYR_TOOLCHAIN_VARIANT=zephyr"
set "ZEPHYR_SDK_INSTALL_DIR=%TOOLCHAIN%\opt\zephyr-sdk"
set "ZEPHYR_BASE=%NCS_ROOT%\zephyr"

where west >nul 2>nul
if errorlevel 1 (
    echo ERROR: west was not found on PATH after setting the toolchain environment.
    exit /b 1
)

cd /d "%NCS_ROOT%"
if errorlevel 1 (
    echo ERROR: Could not change to the nRF Connect SDK workspace: "%NCS_ROOT%"
    exit /b 1
)

west flash -d "%BUILD_DIR%" %*
exit /b %errorlevel%
