@echo off
REM =============================================================================
REM Kalahari - SonarQube analysis on demand (Windows)
REM =============================================================================
REM Double-click this file, or run it from the repository root:
REM   scripts\sonar_analysis.bat               build (incremental), clang-tidy, scan
REM   scripts\sonar_analysis.bat --no-build    analyse the existing build
REM Other options are passed to scripts\sonar_analysis.py (see its header), e.g.
REM   scripts\sonar_analysis.bat --no-build --skip-clang-tidy
REM
REM SonarQube cannot start an analysis from its web interface: the scanner sends
REM it, and the web interface shows the result at
REM   http://localhost:9000/dashboard?id=kalahari
REM
REM The SonarQube token comes from the SONAR_TOKEN environment variable. When this
REM window was opened before the variable was set, it is read from the user
REM environment. The token is never printed or written anywhere.
REM =============================================================================

setlocal
set "PROJECT_ROOT=%~dp0.."
cd /d "%PROJECT_ROOT%"
set "EXIT_CODE=0"

set "RUN_BUILD=1"
set "ANALYSIS_ARGS="
:parse_args
if "%~1"=="" goto args_done
if /i "%~1"=="--no-build" (
    set "RUN_BUILD=0"
) else (
    set ANALYSIS_ARGS=%ANALYSIS_ARGS% %1
)
shift
goto parse_args
:args_done

REM --- Token -------------------------------------------------------------------
if not defined SONAR_TOKEN (
    for /f "usebackq delims=" %%T in (`powershell -NoProfile -Command "[Environment]::GetEnvironmentVariable('SONAR_TOKEN','User')"`) do set "SONAR_TOKEN=%%T"
)
if not defined SONAR_TOKEN (
    echo [ERROR] SONAR_TOKEN is not set.
    echo         Create a user token in SonarQube ^(My Account, Security^) and store it
    echo         in the SONAR_TOKEN user environment variable.
    goto fail
)

REM --- Scanner -----------------------------------------------------------------
REM Order: SONAR_SCANNER variable, PATH, then the default folder on the author's PC.
if not defined SONAR_SCANNER (
    for /f "delims=" %%S in ('where sonar-scanner.bat 2^>nul') do if not defined SONAR_SCANNER set "SONAR_SCANNER=%%S"
)
if not defined SONAR_SCANNER (
    for /d %%D in ("C:\SonarQube\skaner\sonar-scanner-*") do if exist "%%D\bin\sonar-scanner.bat" set "SONAR_SCANNER=%%D\bin\sonar-scanner.bat"
)
if not defined SONAR_SCANNER (
    echo [ERROR] sonar-scanner not found. Add it to PATH or set the SONAR_SCANNER variable.
    goto fail
)

where python >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Python 3 not found on PATH.
    goto fail
)

REM --- Build (the analysis needs compile_commands.json and Qt's generated files) ---
if "%RUN_BUILD%"=="1" (
    call "%~dp0build_windows.bat" Debug
    if errorlevel 1 goto fail
    cd /d "%PROJECT_ROOT%"
)

REM --- clang-tidy and scan ---------------------------------------------------------
python "%~dp0sonar_analysis.py" %ANALYSIS_ARGS%
if errorlevel 1 goto fail
goto end

:fail
set "EXIT_CODE=1"
echo.
echo [ERROR] SonarQube analysis failed.

:end
REM Keep the window open when the script was started by a double-click
echo %cmdcmdline% | find /i "%~nx0" >nul && pause
endlocal & exit /b %EXIT_CODE%
