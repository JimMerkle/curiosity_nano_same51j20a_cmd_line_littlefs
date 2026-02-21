@echo off
rem Build and flash helper for Curiosity Nano (SAM E51)
rem Usage: double-click or run from cmd. Requires 'make' in PATH and 'pyocd' for flashing.

setlocal
set "ROOT=%~dp0"

rem Prefer MPLAB X bundled GNU tools if present
set "MPLAB_GNU=C:\Program Files\Microchip\MPLABX\v6.25\gnuBins\GnuWin32\bin"
if exist "%MPLAB_GNU%\make.exe" (
    echo Using MPLAB X GNU tools from: %MPLAB_GNU%
    set "PATH=%MPLAB_GNU%;%PATH%"
) else (
    echo MPLAB X GNU tools not found at %MPLAB_GNU% (falling back to PATH)
)

echo Building project (LittleFS.X)...
echo Updating timestamp on command_line.c to force rebuild of that unit
"C:\Program Files\Microchip\MPLABX\v6.25\gnuBins\GnuWin32\bin\touch.exe" "%ROOT%src\command_line\command_line.c"
make -C "%ROOT%LittleFS.X" build
if errorlevel 1 (
    echo.
    echo Build failed. Check the output above for errors.
    exit /b 1
)

echo Searching for build artifacts (.elf and .hex)...
set "ELF="
set "HEX="
for /R "%ROOT%LittleFS.X" %%f in (*.elf) do if not defined ELF set "ELF=%%f"
for /R "%ROOT%LittleFS.X" %%f in (*.hex) do if not defined HEX set "HEX=%%f"

if not defined ELF if defined HEX set "ELF=%HEX%"

if not defined ELF (
    echo No .elf or .hex artifact found under LittleFS.X. Look under build/ or dist/.
    exit /b 2
)

echo Build complete.
echo ELF: %ELF%
if defined HEX echo HEX: %HEX%
echo To program the target, open MPLAB IPE and load the above ELF or HEX file.
endlocal
exit /b 0
