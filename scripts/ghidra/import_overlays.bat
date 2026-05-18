@echo off
:: Imports all extracted overlay DLLs and secondary game binaries into
:: per-format Ghidra projects under %FA_PROJECT%\overlay_projects\.
::
:: Run extract_overlays.bat first to populate the overlay directories.
::
:: Note on Phar Lap PE format:
::   FA overlay DLLs use signature PL\0\0 instead of the standard PE\0\0.
::   Ghidra 12.1 PE loader rejects these. This script patches the first two
::   bytes to PE before import, then restores the original in a _pl\ copy.
::   The patched copies live at %FA_PROJECT%\overlays\{fmt}_pe\.
::
:: Usage:  scripts\ghidra\import_overlays.bat [FORMAT]
::   FORMAT (optional): BI, CAM, MC, HUD, LAY, FNT, MUS, secondary, or ALL (default)

setlocal enabledelayedexpansion

set JAVA_HOME=C:\java\jdk-26.0.1
set GHIDRA_HOME=C:\tools\ghidra_12.1_PUBLIC
set FA_PROJECT=%USERPROFILE%\src\fa
set OVERLAY_ROOT=%FA_PROJECT%\overlays
set PROJECTS_ROOT=%FA_PROJECT%\overlay_projects
set SCRIPT_DIR=%~dp0

set TARGET=%1
if "%TARGET%"=="" set TARGET=ALL

echo ============================================================
echo  FA overlay DLL Ghidra import
echo  Projects: %PROJECTS_ROOT%
echo  Target  : %TARGET%
echo ============================================================
echo.

if not exist "%PROJECTS_ROOT%" mkdir "%PROJECTS_ROOT%"

:: Import a single format group
:: Usage: call :import_format FORMAT
goto :skip_fn

:import_format
set FMT=%1
set SRC_DIR=%OVERLAY_ROOT%\%FMT%
set PE_DIR=%OVERLAY_ROOT%\%FMT%_pe
set PROJ_DIR=%PROJECTS_ROOT%\%FMT%
set PROJ_NAME=fa-%FMT:-=_%

if not exist "%SRC_DIR%" (
    echo   [%FMT%] Source dir not found: %SRC_DIR% -- run extract_overlays.bat first
    exit /b 0
)

:: Patch PL -> PE signature in copies
if not exist "%PE_DIR%" mkdir "%PE_DIR%"
set COUNT=0
for %%F in ("%SRC_DIR%\*") do (
    set IN=%%F
    set OUT=%PE_DIR%\%%~nxF
    :: PowerShell one-liner: read bytes, patch offset 0 from 0x50 0x4C -> 0x50 0x45, write
    powershell -NoProfile -Command ^
      "$b=[System.IO.File]::ReadAllBytes('!IN!');" ^
      "if($b[0] -eq 0x50 -and $b[1] -eq 0x4C){$b[1]=0x45};" ^
      "[System.IO.File]::WriteAllBytes('!OUT!',$b)"
    set /A COUNT+=1
)
echo   [%FMT%] Patched %COUNT% files to PE signature

:: Import all patched files into one Ghidra project
if not exist "%PROJ_DIR%" mkdir "%PROJ_DIR%"
echo   [%FMT%] Importing into Ghidra project %PROJ_NAME%...
"%GHIDRA_HOME%\support\analyzeHeadless.bat" ^
    "%PROJ_DIR%" %PROJ_NAME% ^
    -import "%PE_DIR%" ^
    -overwrite ^
    -scriptPath "%SCRIPT_DIR%"

echo   [%FMT%] Done.
exit /b 0

:skip_fn

:: Dispatch by target
if /I "%TARGET%"=="ALL" goto :all
call :import_format %TARGET%
goto :done

:all
for %%F in (BI CAM MC HUD LAY FNT MUS) do (
    echo.
    echo --- %%F ---
    call :import_format %%F
)

:: Secondary binaries (already standard PE, no patching needed)
set SEC_DIR=%OVERLAY_ROOT%\secondary
if exist "%SEC_DIR%" (
    echo.
    echo --- secondary ---
    if not exist "%PROJECTS_ROOT%\secondary" mkdir "%PROJECTS_ROOT%\secondary"
    "%GHIDRA_HOME%\support\analyzeHeadless.bat" ^
        "%PROJECTS_ROOT%\secondary" fa-secondary ^
        -import "%SEC_DIR%" ^
        -overwrite ^
        -scriptPath "%SCRIPT_DIR%"
    echo   [secondary] Done.
)

:done
echo.
echo ============================================================
echo  Import complete.  Projects: %PROJECTS_ROOT%
echo ============================================================
endlocal
