@echo off
:: One-shot Ghidra project setup for FA.EXE reverse engineering.
:: Creates the project dir, imports FA.EXE with full PE analysis,
:: then imports FA.SMS symbols via ImportFASmsHeadless.java.
::
:: Usage:  scripts\ghidra\setup_project.bat
::
:: Edit the paths at the top of run_ghidra.bat if your Ghidra / JDK locations differ.
:: After this script completes, run run_all.bat to produce all analysis outputs.

setlocal

set JAVA_HOME=C:\java\jdk-26.0.1
set GHIDRA_HOME=C:\tools\ghidra_12.1_PUBLIC
set FA_PROJECT=%USERPROFILE%\src\fa
set FA_INSTALL=C:\JANES\Fighters Anthology
set SCRIPT_DIR=%~dp0

echo ============================================================
echo  FA.EXE Ghidra project setup
echo  Project : %FA_PROJECT%
echo  FA.EXE  : %FA_INSTALL%\FA.EXE
echo ============================================================
echo.

:: Create output directory
if not exist "%FA_PROJECT%\output" (
    echo Creating %FA_PROJECT%\output ...
    mkdir "%FA_PROJECT%\output"
)

:: Step 1: Import FA.EXE and run full auto-analysis
echo [1/2] Importing FA.EXE and running auto-analysis...
echo       (This takes several minutes on first run)
"%GHIDRA_HOME%\support\analyzeHeadless.bat" ^
    "%FA_PROJECT%" fa-re ^
    -import "%FA_INSTALL%\FA.EXE" ^
    -overwrite ^
    -scriptPath "%SCRIPT_DIR%"
if errorlevel 1 (
    echo ERROR: FA.EXE import failed.
    exit /b 1
)

:: Step 2: Import FA.SMS symbols
echo.
echo [2/2] Importing FA.SMS symbols...
"%GHIDRA_HOME%\support\analyzeHeadless.bat" ^
    "%FA_PROJECT%" fa-re ^
    -process FA.EXE ^
    -postScript ImportFASmsHeadless.java "%FA_INSTALL%\FA.SMS" ^
    -scriptPath "%SCRIPT_DIR%" ^
    -noanalysis
if errorlevel 1 (
    echo ERROR: FA.SMS import failed.
    exit /b 1
)

echo.
echo ============================================================
echo  Setup complete.
echo  Project: %FA_PROJECT%\fa-re.gpr
echo  Next:    scripts\ghidra\run_all.bat
echo ============================================================
endlocal
