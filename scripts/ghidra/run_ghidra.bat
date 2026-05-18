@echo off
set JAVA_HOME=C:\java\jdk-26.0.1
set GHIDRA_HOME=C:\tools\ghidra_12.1_PUBLIC
set FA_PROJECT=%USERPROFILE%\src\fa
echo PROJECT_DIR=%FA_PROJECT%
echo SCRIPT=%1
"%GHIDRA_HOME%\support\analyzeHeadless.bat" "%FA_PROJECT%" fa-re -process FA.EXE -postScript %1 -scriptPath "%~dp0" -noanalysis
