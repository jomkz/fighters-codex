#!/usr/bin/env bash
# Runs all FA.EXE analysis scripts individually.
# Each script produces its own output file under $FA_PROJECT/output/.
# Also runs AnalyzeFA.java to produce the consolidated single-file report.
#
# Usage:
#   run_all.sh           -- run all scripts (project must already exist)
#   run_all.sh --setup   -- create/refresh the Ghidra project first, then run all
#
# Output files: $FA_PROJECT/output/Analyze*.txt
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/_env.sh"

if [[ "${1:-}" == "--setup" ]]; then
    echo "Running project setup first..."
    "$HERE/setup_project.sh"
    echo
fi

echo "Running all FA.EXE analysis scripts..."
echo

SCRIPTS=(
    # --- Original subsystem scripts ---
    AnalyzeLAY.java
    AnalyzeHUD.java
    AnalyzeDLG.java
    AnalyzePROJ.java
    AnalyzeSEE.java
    AnalyzeMM.java
    AnalyzeBI.java
    AnalyzeECM.java
    AnalyzeHGR.java
    AnalyzeMUS.java
    AnalyzeOTNT.java
    AnalyzeT2.java
    AnalyzeGAS.java
    AnalyzePT.java
    AnalyzePLT.java
    AnalyzeCAM.java
    AnalyzeMC.java
    AnalyzeT2DLL.java
    # --- Dark-zone targeted scripts (new subsystems) ---
    AnalyzeGameLoop.java
    AnalyzeRenderer.java
    AnalyzePhysics.java
    AnalyzeNetwork.java
    AnalyzeInput.java
    # --- Struct and global recovery ---
    DumpGlobals.java
    RecoverStructs.java
    # --- Full function dump (highest ROI -- covers all remaining dark zones) ---
    DumpAllFunctions.java
    # Consolidated single-file report (all subsystems in one pass)
    AnalyzeFA.java
)

for s in "${SCRIPTS[@]}"; do
    "$HERE/run_ghidra.sh" "$s"
done

echo
echo "All scripts complete. Output: $FA_PROJECT/output/"
