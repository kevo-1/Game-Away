@echo off
setlocal enabledelayedexpansion

echo.
echo ========================================
echo    Game-Away Build Script
echo ========================================
echo.

:: Check for Git
where git >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Git is not installed or not in PATH.
    echo Please install Git from https://git-scm.com/
    pause
    exit /b 1
)

:: Check for CMake
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] CMake is not installed or not in PATH.
    echo Please install CMake from https://cmake.org/
    pause
    exit /b 1
)

:: Download vcpkg if not present
if not exist "vcpkg\" (
    echo [1/4] Downloading vcpkg...
    git clone https://github.com/microsoft/vcpkg.git vcpkg
    if %errorlevel% neq 0 (
        echo [ERROR] Failed to clone vcpkg.
        pause
        exit /b 1
    )
) else (
    echo [1/4] vcpkg already exists, skipping download.
)

:: Bootstrap vcpkg if needed
if not exist "vcpkg\vcpkg.exe" (
    echo [2/4] Bootstrapping vcpkg...
    pushd vcpkg
    call bootstrap-vcpkg.bat -disableMetrics
    popd
    if not exist "vcpkg\vcpkg.exe" (
        echo [ERROR] Failed to bootstrap vcpkg.
        pause
        exit /b 1
    )
) else (
    echo [2/4] vcpkg already bootstrapped.
)

:: Configure with CMake (using MinGW)
echo [3/4] Configuring project...
cmake -B build -S . ^
    -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake ^
    -G "MinGW Makefiles" ^
    -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic ^
    -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe ^
    -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe ^
    -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed.
    pause
    exit /b 1
)

:: Build
echo [4/4] Building...
cmake --build build --config Release
if %errorlevel% neq 0 (
    echo [ERROR] Build failed.
    pause
    exit /b 1
)

echo.
echo ========================================
echo    Build Complete!
echo ========================================
echo.
echo Run the application:
echo    GameAway.exe
echo.
pause
