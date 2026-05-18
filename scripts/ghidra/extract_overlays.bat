@echo off
:: Extracts all PE overlay DLLs from FA_2.LIB into per-format subdirectories.
:: Output: %FA_PROJECT%\overlays\{bi,cam,mc,hud,lay,fnt,mus}\
::
:: Requires ft.exe on PATH or set FT_EXE below.
:: Run from the repo root or any directory; uses absolute paths throughout.
::
:: Usage:  scripts\ghidra\extract_overlays.bat

setlocal

set FA_PROJECT=%USERPROFILE%\src\fa
set FA_INSTALL=C:\JANES\Fighters Anthology
set FT_EXE=ft

:: Prefer the repo build output if present
if exist "%~dp0..\..\build\cli\Release\ft.exe" (
    set FT_EXE=%~dp0..\..\build\cli\Release\ft.exe
)

set LIB=%FA_INSTALL%\FA_2.LIB
set OVERLAY_ROOT=%FA_PROJECT%\overlays

echo ============================================================
echo  FA overlay DLL extraction
echo  Source : %LIB%
echo  Output : %OVERLAY_ROOT%
echo ============================================================
echo.

if not exist "%LIB%" (
    echo ERROR: %LIB% not found.
    exit /b 1
)

:: Unpack everything into a staging directory
set STAGING=%OVERLAY_ROOT%\_all
echo [1/3] Unpacking FA_2.LIB to staging dir...
if not exist "%STAGING%" mkdir "%STAGING%"
"%FT_EXE%" lib unpack "%LIB%" "%STAGING%"
if errorlevel 1 (
    echo ERROR: ft lib unpack failed.
    exit /b 1
)

:: Copy each format into its own subdirectory
echo.
echo [2/3] Sorting overlay files by extension...
for %%E in (BI CAM MC HUD LAY FNT MUS) do (
    set DEST=%OVERLAY_ROOT%\%%E
    if not exist "%OVERLAY_ROOT%\%%E" mkdir "%OVERLAY_ROOT%\%%E"
    for %%F in ("%STAGING%\*.%%E") do (
        if exist "%%F" copy /Y "%%F" "%OVERLAY_ROOT%\%%E\" >nul
    )
    echo   %%E: copied to %OVERLAY_ROOT%\%%E
)

:: Also copy secondary game binaries for their own projects
echo.
echo [3/3] Copying secondary game binaries...
set SECONDARY=%OVERLAY_ROOT%\secondary
if not exist "%SECONDARY%" mkdir "%SECONDARY%"
for %%F in (IP.EXE WAIL32.DLL msapi.dll CDRVDL32.DLL CDRVHF32.DLL CDRVXF32.DLL COMMSC32.DLL) do (
    if exist "%FA_INSTALL%\%%F" (
        copy /Y "%FA_INSTALL%\%%F" "%SECONDARY%\" >nul
        echo   Copied %%F
    )
)

echo.
echo ============================================================
echo  Extraction complete.
echo  Overlay dirs: %OVERLAY_ROOT%\{BI,CAM,MC,HUD,LAY,FNT,MUS}
echo  Secondary:    %OVERLAY_ROOT%\secondary
echo  Next:         scripts\ghidra\import_overlays.bat
echo ============================================================
endlocal
