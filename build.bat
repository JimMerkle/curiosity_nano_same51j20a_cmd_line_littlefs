@echo off
setlocal
rem build.bat - clean rebuild of the MPLAB X project.
rem Artifacts land in: LittleFS.X\dist\default\production
set "T0=%TIME%"
set "RC=0"
set "XPROJ=LittleFS.X"

rem Auto-detect MPLAB X (prefer v6.30, else v6.25).
set "MPLABX="
if exist "C:\Program Files\Microchip\MPLABX\v6.30\mplab_platform\bin\prjMakefilesGenerator.bat" set "MPLABX=C:\Program Files\Microchip\MPLABX\v6.30"
if not defined MPLABX if exist "C:\Program Files\Microchip\MPLABX\v6.25\mplab_platform\bin\prjMakefilesGenerator.bat" set "MPLABX=C:\Program Files\Microchip\MPLABX\v6.25"
if not defined MPLABX (
    echo MPLAB X v6.30 or v6.25 not found. Update build.bat.
    set "RC=1"
    goto :done
)

set "PATH=%MPLABX%\gnuBins\GnuWin32\bin;%PATH%"
pushd "%~dp0%XPROJ%"
if exist build rmdir /s /q build
if exist debug rmdir /s /q debug
if exist dist  rmdir /s /q dist
call "%MPLABX%\mplab_platform\bin\prjMakefilesGenerator.bat" -v .
"%MPLABX%\gnuBins\GnuWin32\bin\make.exe" -f nbproject/Makefile-default.mk SUBPROJECTS= .build-conf
set "RC=%errorlevel%"
popd

if "%RC%"=="0" (echo Build completed successfully.) else (echo BUILD FAILED with exit code %RC%.)

:done
set "T1=%TIME%"
set "S=%T0: =0%"
set /a "s0=(1%S:~0,2%-100)*3600+(1%S:~3,2%-100)*60+(1%S:~6,2%-100)"
set "S=%T1: =0%"
set /a "s1=(1%S:~0,2%-100)*3600+(1%S:~3,2%-100)*60+(1%S:~6,2%-100)"
set /a "elapsed=s1-s0"
if %elapsed% lss 0 set /a "elapsed+=86400"
echo build.bat elapsed: %elapsed% s
endlocal & exit /b %RC%
