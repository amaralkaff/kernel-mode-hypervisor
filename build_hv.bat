@echo off
:: Build vmhv.sys from hv\src, output to x64\Release
setlocal

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

set WDK=C:\Program Files (x86)\Windows Kits\10
set WDKVER=10.0.26100.0
set WDKINC=%WDK%\Include\%WDKVER%
set WDKLIB=%WDK%\Lib\%WDKVER%
set SRC=%~dp0hv\src
set OBJ=%~dp0hv\obj
set OUT=%~dp0x64\Release

cd /d "%SRC%"

echo === Assembling ===
for %%f in (ctx.asm msr.asm nt.asm segments.asm vmexit.asm) do (
    ml64 /c /nologo /Fo"%OBJ%\%%~nf.obj" %%f
    if errorlevel 1 (echo FAILED: %%f && exit /b 1)
)

echo === Compiling ===
cl /c /O2 /GS- /kernel /W3 /Zc:wchar_t /Zi /Gm- /std:c++17 /EHs-c- /I"%WDKINC%\km" /I"%WDKINC%\km\crt" /I"%WDKINC%\shared" /I"%SRC%" /D "NDEBUG" /D "_KERNEL_MODE" /D "_AMD64_" /D "AMD64" /D "_WIN64" /Fo"%OBJ%\main.obj" main.cpp
if errorlevel 1 (echo FAILED: compile && exit /b 1)

echo === Linking vmhv.sys ===
link /OUT:"%OUT%\vmhv.sys" /NOLOGO /MACHINE:X64 /SUBSYSTEM:NATIVE /DRIVER:WDM /ENTRY:driver_entry /NODEFAULTLIB /MERGE:.rdata=.text "%OBJ%\main.obj" "%OBJ%\vmexit.obj" "%OBJ%\ctx.obj" "%OBJ%\msr.obj" "%OBJ%\nt.obj" "%OBJ%\segments.obj" "%WDKLIB%\km\x64\ntoskrnl.lib" "%WDKLIB%\km\x64\hal.lib" "%WDKLIB%\km\x64\wmilib.lib" "%WDKLIB%\km\x64\BufferOverflowFastFailK.lib"
if errorlevel 1 (echo FAILED: link && exit /b 1)

echo === Building test_ping.exe ===
cd /d "%~dp0hv"
cl /O2 /Fe:"%OUT%\test_ping.exe" test_ping.cpp >nul 2>&1
if errorlevel 1 (echo [!] test_ping failed) else (echo === test_ping OK ===)

echo.
echo === DONE: vmhv.sys in x64\Release ===
