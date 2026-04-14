@echo off
REM 5G UE Simulation Build Script for Windows
REM This script helps build the project on Windows environments

echo 5G UE Simulation Build Script
echo =============================
echo.

REM Check if MinGW/MSYS2 is available
where gcc >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: GCC not found. Please install MinGW or MSYS2 with GCC.
    echo.
    echo For MSYS2:
    echo   1. Download from https://www.msys2.org/
    echo   2. Install base packages: pacman -S mingw-w64-x86_64-gcc
    echo   3. Add to PATH: C:\msys64\mingw64\bin
    echo.
    pause
    exit /b 1
)

REM Check if make is available
where make >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: make not found. Please install make through MSYS2.
    echo   pacman -S make
    echo.
    pause
    exit /b 1
)

REM Display build options
echo Build Options:
echo   1. Build main executable (uesim.exe)
echo   2. Build debug version (uesim-debug.exe)
echo   3. Build and run all tests
echo   4. Build OAI-O-RAN integration tests only
echo   5. Build Commercial O-RAN integration tests only
echo   6. Clean build artifacts
echo   7. Clean all generated files
echo.

set /p choice="Enter your choice (1-7): "

echo.
echo Building...
echo.

if "%choice%"=="1" (
    make all
) else if "%choice%"=="2" (
    make debug
) else if "%choice%"=="3" (
    make test
) else if "%choice%"=="4" (
    make test-gnb-oai
) else if "%choice%"=="5" (
    make test-gnb-oran
) else if "%choice%"=="6" (
    make clean
) else if "%choice%"=="7" (
    make distclean
) else (
    echo Invalid choice. Building main executable by default...
    make all
)

if %errorlevel% equ 0 (
    echo.
    echo Build completed successfully!
) else (
    echo.
    echo Build failed with error code %errorlevel%!
)

echo.
pause