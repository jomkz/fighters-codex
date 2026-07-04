@echo off
:: Apply db\types\*.h + the db\symbols type column to the fa-re Ghidra project
:: (ApplyTypes.java). Run apply_symbols.bat first. Bench-parity launcher; the
:: Fedora project is canonical (db\types\README.md).
set JAVA_HOME=C:\java\jdk-26.0.1
set GHIDRA_HOME=C:\tools\ghidra_12.1_PUBLIC
set FA_PROJECT=%USERPROFILE%\src\fa
:: Strip trailing backslash from %~dp0 to avoid the \" quote-escape bug
set SCRIPT_DIR=%~dp0
if "%SCRIPT_DIR:~-1%"=="\" set SCRIPT_DIR=%SCRIPT_DIR:~0,-1%
for %%I in ("%SCRIPT_DIR%\..\..") do set REPO_ROOT=%%~fI
set BINARY=%1
if "%BINARY%"=="" set BINARY=FA.EXE
echo PROJECT_DIR=%FA_PROJECT%
echo REPO_ROOT=%REPO_ROOT%
echo BINARY=%BINARY%
"%GHIDRA_HOME%\support\analyzeHeadless.bat" "%FA_PROJECT%" fa-re -process %BINARY% -postScript ApplyTypes.java "%REPO_ROOT%" -scriptPath "%SCRIPT_DIR%" -noanalysis
