#!/usr/bin/env bash
# Apply db/symbols/*.csv to the fa-re Ghidra project (ApplySymbols.java).
# Writes to the project database; idempotent. See db/README.md.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/_env.sh"

REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
echo "PROJECT_DIR=$FA_PROJECT"
echo "REPO_ROOT=$REPO_ROOT"
"$ANALYZE_HEADLESS" "$FA_PROJECT" fa-re -process FA.EXE \
    -postScript ApplySymbols.java "$REPO_ROOT" \
    -scriptPath "$SCRIPT_DIR" -noanalysis
