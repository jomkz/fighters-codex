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

# The roster is single-sourced from the Analyze*.java files themselves: every
# one runs against FA.EXE except the overlay-DLL scripts below, which target
# their own projects via run_overlays.sh. A new Analyze*.java therefore joins
# run_all automatically — roster drift (the #373 gap) cannot recur. Keep this
# exclusion list in sync with any new overlay-only analyzers.
OVERLAY_ONLY="AnalyzeCAMDLL.java AnalyzeMCDLL.java"

SCRIPTS=()
for path in "$HERE"/Analyze*.java; do
    s="$(basename "$path")"
    [[ " $OVERLAY_ONLY " == *" $s "* ]] && continue
    SCRIPTS+=("$s")
done
# Fixed tail: struct/global recovery + the whole-image decompile (not Analyze*).
SCRIPTS+=(DumpGlobals.java RecoverStructs.java DumpAllFunctions.java)

for s in "${SCRIPTS[@]}"; do
    "$HERE/run_ghidra.sh" "$s"
done

echo
echo "All scripts complete. Output: $FA_PROJECT/output/"
