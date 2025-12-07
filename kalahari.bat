@echo off
REM Kalahari Wrapper Script
REM Launches Claude Code with "session" parameter to trigger SESSION RESTORE
REM
REM Usage:
REM   kalahari.bat           - Start with session restore (default)
REM   kalahari.bat --new     - Start fresh without session restore
REM

cd /d E:\Python\Projekty\Kalahari

if "%1"=="--new" (
    echo Starting Kalahari without session restore...
    claude
) else (
    echo Starting Kalahari with session restore...
    claude "session"
)
