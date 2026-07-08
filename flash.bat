@echo off
setlocal
set "T0=%TIME%"
set "RC=0"

rem Program the SAM E51 Curiosity Nano (ATSAME51J20A) via its on-board nEDBG.
rem Program (-M), then a separate connect+release (-OK -OL) so the target RUNS
rem after flashing (a combined "-M -OL" leaves the nEDBG holding it in reset).
set "DEVICE=ATSAME51J20A"

set "IPECMD="
if exist "C:\Program Files\Microchip\MPLABX\v6.30\mplab_platform\mplab_ipe\ipecmd.exe" set "IPECMD=C:\Program Files\Microchip\MPLABX\v6.30\mplab_platform\mplab_ipe\ipecmd.exe"
if not defined IPECMD if exist "C:\Program Files\Microchip\MPLABX\v6.25\mplab_platform\mplab_ipe\ipecmd.exe" set "IPECMD=C:\Program Files\Microchip\MPLABX\v6.25\mplab_platform\mplab_ipe\ipecmd.exe"

if not defined IPECMD (
    echo ipecmd.exe not found under MPLABX v6.30 or v6.25.
    set "RC=1"
    goto :done
)

set "HEX_FILE=%~dp0LittleFS.X\dist\default\production\LittleFS.X.production.hex"

if not exist "%HEX_FILE%" (
    echo Hex file not found: "%HEX_FILE%"
    echo Run build.bat first.
    set "RC=1"
    goto :done
)

echo Using ipecmd: "%IPECMD%"
echo Programming %DEVICE% via on-board nEDBG...
"%IPECMD%" -P%DEVICE% -tpnEdbg -F"%HEX_FILE%" -M
set "RC=%errorlevel%"
if not "%RC%"=="0" goto :done

echo Releasing target from reset...
"%IPECMD%" -P%DEVICE% -tpnEdbg -OK -OL
set "RC=%errorlevel%"

:done
set "T1=%TIME%"
set "S=%T0: =0%"
set /a "s0=(1%S:~0,2%-100)*3600+(1%S:~3,2%-100)*60+(1%S:~6,2%-100)"
set "S=%T1: =0%"
set /a "s1=(1%S:~0,2%-100)*3600+(1%S:~3,2%-100)*60+(1%S:~6,2%-100)"
set /a "elapsed=s1-s0"
if %elapsed% lss 0 set /a "elapsed+=86400"
echo flash.bat elapsed: %elapsed% s
endlocal & exit /b %RC%
