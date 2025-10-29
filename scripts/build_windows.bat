@echo off
REM =============================================================================
REM Kalahari Build Script (Windows)
REM =============================================================================
REM Automated build script for Windows 10/11
REM Requires: Visual Studio 2019+ with C++ Desktop workload
REM
REM Usage:
REM   scripts\build_windows.bat              # Debug build
REM   scripts\build_windows.bat Release      # Release build
REM   scripts\build_windows.bat Debug clean  # Clean + Debug build
REM   scripts\build_windows.bat Release test # Release + run tests
REM
REM =============================================================================

setlocal enabledelayedexpansion

REM Get project root (script is in scripts\ subdirectory)
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."
cd /d "%PROJECT_ROOT%"

REM Default configuration
set "BUILD_TYPE=Debug"
set "CLEAN_BUILD=0"
set "RUN_TESTS=0"

REM Parse arguments
:parse_args
if "%~1"=="" goto args_done
if /i "%~1"=="Release" set "BUILD_TYPE=Release"
if /i "%~1"=="Debug" set "BUILD_TYPE=Debug"
if /i "%~1"=="clean" set "CLEAN_BUILD=1"
if /i "%~1"=="test" set "RUN_TESTS=1"
if /i "%~1"=="--help" goto show_help
if /i "%~1"=="-h" goto show_help
if /i "%~1"=="/?" goto show_help
shift
goto parse_args

:args_done

echo.
echo ========================================
echo Kalahari Build Script (Windows)
echo ========================================
echo.

REM =============================================================================
REM Auto-detect and initialize Visual Studio environment
REM =============================================================================
REM Skip if already in VS Developer Command Prompt (VCINSTALLDIR is set)
if defined VCINSTALLDIR (
    echo [OK] Already in Visual Studio environment
    goto skip_vs_init
)

echo [INFO] Detecting Visual Studio installation...

REM Try Visual Studio 2022 (most common)
set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022"
if exist "%VS_PATH%\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VCVARSALL=%VS_PATH%\Community\VC\Auxiliary\Build\vcvarsall.bat"
    set "VS_VERSION=2022 Community"
    goto found_vs
)
if exist "%VS_PATH%\Professional\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VCVARSALL=%VS_PATH%\Professional\VC\Auxiliary\Build\vcvarsall.bat"
    set "VS_VERSION=2022 Professional"
    goto found_vs
)
if exist "%VS_PATH%\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VCVARSALL=%VS_PATH%\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
    set "VS_VERSION=2022 Enterprise"
    goto found_vs
)

REM Try Visual Studio 2019
set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019"
if exist "%VS_PATH%\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VCVARSALL=%VS_PATH%\Community\VC\Auxiliary\Build\vcvarsall.bat"
    set "VS_VERSION=2019 Community"
    goto found_vs
)
if exist "%VS_PATH%\Professional\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VCVARSALL=%VS_PATH%\Professional\VC\Auxiliary\Build\vcvarsall.bat"
    set "VS_VERSION=2019 Professional"
    goto found_vs
)
if exist "%VS_PATH%\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VCVARSALL=%VS_PATH%\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
    set "VS_VERSION=2019 Enterprise"
    goto found_vs
)

REM Visual Studio not found
echo [ERROR] Visual Studio 2019+ not found!
echo.
echo Please install Visual Studio 2019 or 2022 with:
echo   - Desktop development with C++
echo   - CMake tools for Windows
echo.
echo Download from: https://visualstudio.microsoft.com/downloads/
exit /b 1

:found_vs
echo [OK] Found Visual Studio %VS_VERSION%
echo [INFO] Initializing build environment...

REM Initialize VS environment for x64
call "%VCVARSALL%" x64 >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Failed to initialize Visual Studio environment
    exit /b 1
)

echo [OK] Visual Studio environment initialized

:skip_vs_init

REM =============================================================================
REM Check prerequisites
REM =============================================================================
echo [INFO] Checking prerequisites...

REM Check CMake (should be available after VS init)
where cmake >nul 2>&1
if errorlevel 1 (
    echo [ERROR] CMake still not found after VS initialization!
    echo.
    echo Please ensure Visual Studio includes:
    echo   - Desktop development with C++
    echo   - CMake tools for Windows
    echo.
    echo Or manually add CMake to PATH
    exit /b 1
)

REM Check Ninja
where ninja >nul 2>&1
if errorlevel 1 (
    echo [WARNING] Ninja not found, will use MSBuild
    set "GENERATOR=Visual Studio 16 2019"
) else (
    set "GENERATOR=Ninja"
)

REM Check Git
where git >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Git not found!
    echo Install Git from: https://git-scm.com/download/win
    exit /b 1
)

echo [OK] Prerequisites found

REM =============================================================================
REM Initialize vcpkg submodule
REM =============================================================================
if not exist "vcpkg\bootstrap-vcpkg.bat" (
    echo [WARNING] vcpkg submodule not initialized
    echo [INFO] Initializing git submodules...
    git submodule update --init --recursive
    if errorlevel 1 (
        echo [ERROR] Failed to initialize submodules
        exit /b 1
    )
    echo [OK] Submodules initialized
) else (
    echo [OK] vcpkg submodule already initialized
)

REM =============================================================================
REM Bootstrap vcpkg
REM =============================================================================
if not exist "vcpkg\vcpkg.exe" (
    echo [INFO] Bootstrapping vcpkg (first time setup^)...
    echo [WARNING] This may take 2-5 minutes...
    cd vcpkg
    call bootstrap-vcpkg.bat
    if errorlevel 1 (
        echo [ERROR] vcpkg bootstrap failed
        exit /b 1
    )
    cd ..
    echo [OK] vcpkg bootstrapped successfully
) else (
    echo [OK] vcpkg already bootstrapped
)

REM =============================================================================
REM Clean build directory
REM =============================================================================
if %CLEAN_BUILD%==1 (
    echo [INFO] Cleaning build directory...
    if exist build-windows rmdir /s /q build-windows
    echo [OK] Build directory cleaned
)

REM =============================================================================
REM Configure CMake
REM =============================================================================
echo [INFO] Configuring CMake (%BUILD_TYPE% build^)...
echo [WARNING] First build may take 10-20 minutes (vcpkg dependencies^)
echo [WARNING] Subsequent builds: ~1-2 minutes (cached^)

cmake -B build-windows ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake ^
    -G "%GENERATOR%"

if errorlevel 1 (
    echo [ERROR] CMake configuration failed
    echo.
    echo Troubleshooting:
    echo   1. Check BUILDING.md for detailed instructions
    echo   2. Ensure all Visual Studio C++ components are installed
    echo   3. Try running: vcpkg integrate install
    echo   4. Check vcpkg_manifest.log in build-windows/ for details
    exit /b 1
)

echo [OK] CMake configuration completed

REM =============================================================================
REM Build project
REM =============================================================================
echo [INFO] Building Kalahari (%BUILD_TYPE%^)...

set "START_TIME=%time%"

cmake --build build-windows --config %BUILD_TYPE% --parallel

if errorlevel 1 (
    echo [ERROR] Build failed
    exit /b 1
)

echo [OK] Build completed

REM =============================================================================
REM Run tests
REM =============================================================================
if %RUN_TESTS%==1 (
    echo [INFO] Running tests...
    cd build-windows
    ctest -C %BUILD_TYPE% --output-on-failure
    if errorlevel 1 (
        echo [ERROR] Some tests failed
        exit /b 1
    )
    echo [OK] All tests passed
    cd ..
)

REM =============================================================================
REM Show build summary
REM =============================================================================
echo.
echo ========================================
echo Build Successful!
echo ========================================
echo.
echo Build type:    %BUILD_TYPE%
echo Executable:    build-windows\bin\kalahari.exe
echo Tests:         build-windows\bin\kalahari-tests.exe
echo.
echo Run application:
echo   build-windows\bin\kalahari.exe
echo.
echo Run tests:
echo   cd build-windows
echo   ctest -C %BUILD_TYPE% --output-on-failure
echo.
echo ========================================

goto :eof

REM =============================================================================
REM Show help
REM =============================================================================
:show_help
echo Kalahari Build Script (Windows)
echo.
echo Usage: %~nx0 [BUILD_TYPE] [OPTIONS]
echo.
echo Build Types:
echo   Debug        Build in Debug mode (default)
echo   Release      Build in Release mode
echo.
echo Options:
echo   clean        Clean build directory before building
echo   test         Run tests after building
echo   --help, -h   Show this help message
echo.
echo Examples:
echo   %~nx0                     # Debug build
echo   %~nx0 Release             # Release build
echo   %~nx0 Debug clean         # Clean + Debug build
echo   %~nx0 Release test        # Release build + tests
echo.
echo Build output:
echo   Executable: build-windows\bin\kalahari.exe
echo   Tests:      build-windows\bin\kalahari-tests.exe
echo.
echo For more information, see BUILDING.md
goto :eof
