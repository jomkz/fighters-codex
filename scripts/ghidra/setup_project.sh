#!/usr/bin/env bash
# One-shot Ghidra project setup for FA.EXE reverse engineering.
# Creates the project dir, imports FA.EXE with full PE analysis,
# then imports FA.SMS symbols via ImportFASmsHeadless.java.
#
# Usage:  scripts/ghidra/setup_project.sh
#
# Set FA_INSTALL / GHIDRA_HOME / JAVA_HOME to override the defaults in _env.sh.
# After this script completes, run run_all.sh to produce all analysis outputs.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/_env.sh"

echo "============================================================"
echo " FA.EXE Ghidra project setup"
echo " Project : $FA_PROJECT"
echo " FA.EXE  : $FA_INSTALL/FA.EXE"
echo "============================================================"
echo

mkdir -p "$FA_PROJECT/output"

echo "[1/2] Importing FA.EXE and running auto-analysis..."
echo "      (This takes several minutes on first run)"
"$ANALYZE_HEADLESS" "$FA_PROJECT" fa-re \
    -import "$FA_INSTALL/FA.EXE" \
    -overwrite \
    -scriptPath "$SCRIPT_DIR"

echo
echo "[2/2] Importing FA.SMS symbols..."
"$ANALYZE_HEADLESS" "$FA_PROJECT" fa-re \
    -process FA.EXE \
    -postScript ImportFASmsHeadless.java "$FA_INSTALL/FA.SMS" \
    -scriptPath "$SCRIPT_DIR" \
    -noanalysis

echo
echo "============================================================"
echo " Setup complete."
echo " Project: $FA_PROJECT/fa-re.gpr"
echo " Next:    scripts/ghidra/run_all.sh"
echo "============================================================"
