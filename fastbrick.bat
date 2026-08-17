@echo off

where powershell >nul 2>&1
if %errorlevel% neq 0 (
    echo PowerShell is not available on this system.
    echo Please install PowerShell or use Windows 10/11.
    pause
    exit /b 1
)

powershell -ExecutionPolicy Bypass -File "%~dp0fastbrick.ps1"

if %errorlevel% neq 0 (
    echo.
    echo PowerShell script encountered an error.
    pause
    exit /b %errorlevel%
)

exit /b 0
