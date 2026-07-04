@echo off
:: Import the epic #247 companion binaries into the fa-re Ghidra project as their
:: own programs (standard PE/i386; no Phar Lap patch). Bench-parity launcher; the
:: Fedora project is canonical. Usage: import_targets.bat [BINARY ...]
set JAVA_HOME=C:\java\jdk-26.0.1
set GHIDRA_HOME=C:\tools\ghidra_12.1_PUBLIC
set FA_PROJECT=%USERPROFILE%\src\fa
set FA_INSTALL=%FA_PROJECT%\game
:: Strip trailing backslash from %~dp0 to avoid the \" quote-escape bug
set SCRIPT_DIR=%~dp0
if "%SCRIPT_DIR:~-1%"=="\" set SCRIPT_DIR=%SCRIPT_DIR:~0,-1%

set TARGETS=%*
if "%TARGETS%"=="" set TARGETS=IP.EXE WAIL32.DLL CDRVDL32.DLL CDRVHF32.DLL CDRVXF32.DLL COMMSC32.DLL
for %%B in (%TARGETS%) do (
    echo == import %%B + full auto-analysis ==
    "%GHIDRA_HOME%\support\analyzeHeadless.bat" "%FA_PROJECT%" fa-re -import "%FA_INSTALL%\%%B" -overwrite -scriptPath "%SCRIPT_DIR%"
)
