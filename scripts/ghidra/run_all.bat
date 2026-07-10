@echo off
:: Runs all FA.EXE analysis scripts individually.
:: Each script produces its own output file under %FA_PROJECT%\output\.
:: Also runs AnalyzeFA.java to produce the consolidated single-file report.
::
:: Usage:
::   run_all.bat           -- run all scripts (project must already exist)
::   run_all.bat --setup   -- create/refresh the Ghidra project first, then run all
::
:: Output files: %FA_PROJECT%\output\Analyze*.txt

setlocal

if /I "%1"=="--setup" (
    echo Running project setup first...
    call "%~dp0setup_project.bat"
    if errorlevel 1 exit /b 1
    echo.
)

echo Running all FA.EXE analysis scripts...
echo.

:: --- Original subsystem scripts ---
call "%~dp0run_ghidra.bat" AnalyzeLAY.java
call "%~dp0run_ghidra.bat" AnalyzeHUD.java
call "%~dp0run_ghidra.bat" AnalyzeDLG.java
call "%~dp0run_ghidra.bat" AnalyzePROJ.java
call "%~dp0run_ghidra.bat" AnalyzeSEE.java
call "%~dp0run_ghidra.bat" AnalyzeMM.java
call "%~dp0run_ghidra.bat" AnalyzeBI.java
call "%~dp0run_ghidra.bat" AnalyzeECM.java
call "%~dp0run_ghidra.bat" AnalyzeHGR.java
call "%~dp0run_ghidra.bat" AnalyzeMUS.java
call "%~dp0run_ghidra.bat" AnalyzeOTNT.java
call "%~dp0run_ghidra.bat" AnalyzeT2.java
call "%~dp0run_ghidra.bat" AnalyzeGAS.java
call "%~dp0run_ghidra.bat" AnalyzePT.java
call "%~dp0run_ghidra.bat" AnalyzePLT.java
call "%~dp0run_ghidra.bat" AnalyzeCAM.java
call "%~dp0run_ghidra.bat" AnalyzeMC.java
call "%~dp0run_ghidra.bat" AnalyzeT2DLL.java
call "%~dp0run_ghidra.bat" AnalyzeCB8.java

:: --- Dark-zone targeted scripts (new subsystems) ---
call "%~dp0run_ghidra.bat" AnalyzeGameLoop.java
call "%~dp0run_ghidra.bat" AnalyzeRenderer.java
call "%~dp0run_ghidra.bat" AnalyzePhysics.java
call "%~dp0run_ghidra.bat" AnalyzeNetwork.java
call "%~dp0run_ghidra.bat" AnalyzeInput.java

:: --- SH shape interpreter (evidence for #124/#52/#125) ---
call "%~dp0run_ghidra.bat" AnalyzeSHHeader.java
call "%~dp0run_ghidra.bat" AnalyzeSHDispatch.java
call "%~dp0run_ghidra.bat" AnalyzeSHX86.java

:: --- Struct and global recovery ---
call "%~dp0run_ghidra.bat" DumpGlobals.java
call "%~dp0run_ghidra.bat" RecoverStructs.java

:: --- Full function dump (highest ROI -- covers all remaining dark zones) ---
call "%~dp0run_ghidra.bat" DumpAllFunctions.java

:: Consolidated single-file report (all subsystems in one pass)
call "%~dp0run_ghidra.bat" AnalyzeFA.java

echo.
echo All scripts complete. Output: %FA_PROJECT%\output\
endlocal
