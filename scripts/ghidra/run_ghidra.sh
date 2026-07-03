#!/usr/bin/env bash
# Run a single analysis script against FA.EXE in the fa-re project.
#
# Usage: scripts/ghidra/run_ghidra.sh <Script.java> [scriptArg ...]
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/_env.sh"

SCRIPT="${1:?usage: run_ghidra.sh <Script.java> [scriptArg ...]}"
shift

echo "PROJECT_DIR=$FA_PROJECT"
echo "SCRIPT=$SCRIPT"
"$ANALYZE_HEADLESS" "$FA_PROJECT" fa-re -process FA.EXE \
    -postScript "$SCRIPT" "$@" -scriptPath "$SCRIPT_DIR" -noanalysis
