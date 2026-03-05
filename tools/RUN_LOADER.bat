@echo off
:: Apex Legends Loader - Run as Admin
:: Right-click -> Run as Administrator

cd /d "%~dp0"

:: Find Python
where python >nul 2>nul
if %errorlevel% equ 0 (
    python apex_loader.py %*
    goto :end
)

:: Try common Python paths
if exist "C:\Python314\python.exe" (
    "C:\Python314\python.exe" apex_loader.py %*
    goto :end
)

if exist "C:\Python312\python.exe" (
    "C:\Python312\python.exe" apex_loader.py %*
    goto :end
)

if exist "C:\Python311\python.exe" (
    "C:\Python311\python.exe" apex_loader.py %*
    goto :end
)

echo Python not found! Install Python 3.x from python.org
:end
pause
