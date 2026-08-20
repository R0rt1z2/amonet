@echo off
setlocal

set PY=
where py >nul 2>&1 && set PY=py -3
if defined PY goto haspy
where python >nul 2>&1 && set PY=python
:haspy

if not defined PY (
    echo Python not found, install it from python.org
    pause
    exit /b 1
)

%PY% -c "import serial" >nul 2>&1
if errorlevel 1 (
    echo Installing pyserial...
    %PY% -m pip install pyserial
    if errorlevel 1 (
        echo Failed to install pyserial
        pause
        exit /b 1
    )
)

cd /d "%~dp0modules"
%PY% handshake2.py FACTFACT
pause
