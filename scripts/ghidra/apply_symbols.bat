@echo off
:: Apply db\symbols\*.csv to the fa-re Ghidra project (ApplySymbols.java).
:: Bench-parity launcher; the Fedora project is canonical (db\README.md).
set JAVA_HOME=C:\java\jdk-26.0.1
set GHIDRA_HOME=C:\tools\ghidra_12.1_PUBLIC
set FA_PROJECT=%USERPROFILE%\src\fa
:: Strip trailing backslash from %~dp0 to avoid the \" quote-escape bug
set SCRIPT_DIR=%~dp0
if "%SCRIPT_DIR:~-1%"=="\" set SCRIPT_DIR=%SCRIPT_DIR:~0,-1%
for %%I in ("%SCRIPT_DIR%\..\..") do set REPO_ROOT=%%~fI
echo PROJECT_DIR=%FA_PROJECT%
echo REPO_ROOT=%REPO_ROOT%
"%GHIDRA_HOME%\support\analyzeHeadless.bat" "%FA_PROJECT%" fa-re -process FA.EXE -postScript ApplySymbols.java "%REPO_ROOT%" -scriptPath "%SCRIPT_DIR%" -noanalysis
