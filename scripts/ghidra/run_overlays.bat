@echo off
:: Full overlay pipeline: extract PE DLLs from FA_2.LIB, import into Ghidra.
::
:: Usage:
::   run_overlays.bat              -- extract + import all formats
::   run_overlays.bat --extract    -- extraction step only
::   run_overlays.bat --import     -- import step only (extraction must have run first)
::   run_overlays.bat --import BI  -- import a single format (BI, CAM, MC, HUD, LAY, FNT, MUS)
::
:: Output projects: %FA_PROJECT%\overlay_projects\{BI,CAM,MC,HUD,LAY,FNT,MUS,secondary}

setlocal

set DO_EXTRACT=1
set DO_IMPORT=1
set IMPORT_TARGET=ALL

if /I "%1"=="--extract" ( set DO_IMPORT=0 )
if /I "%1"=="--import"  ( set DO_EXTRACT=0 )
if /I "%1"=="--import"  if not "%2"=="" ( set IMPORT_TARGET=%2 )

echo ============================================================
echo  FA overlay DLL pipeline
if %DO_EXTRACT%==1 echo   Step 1: extract overlays from FA_2.LIB
if %DO_IMPORT%==1  echo   Step 2: import overlays into Ghidra (%IMPORT_TARGET%)
echo ============================================================
echo.

if %DO_EXTRACT%==1 (
    call "%~dp0extract_overlays.bat"
    if errorlevel 1 (
        echo ERROR: extraction failed.
        exit /b 1
    )
    echo.
)

if %DO_IMPORT%==1 (
    call "%~dp0import_overlays.bat" %IMPORT_TARGET%
    if errorlevel 1 (
        echo ERROR: import failed.
        exit /b 1
    )
)

echo.
echo ============================================================
echo  Overlay pipeline complete.
echo  Projects: %FA_PROJECT%\overlay_projects\
echo ============================================================
endlocal
