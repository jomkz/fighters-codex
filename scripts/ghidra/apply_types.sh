#!/usr/bin/env bash
# Apply db/types/*.h + the db/symbols type column to the fa-re Ghidra project
# (ApplyTypes.java). Run apply_symbols.sh first so the names exist; idempotent.
# Writes to the project database. See db/types/README.md.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/_env.sh"

REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
echo "PROJECT_DIR=$FA_PROJECT"
echo "REPO_ROOT=$REPO_ROOT"
"$ANALYZE_HEADLESS" "$FA_PROJECT" fa-re -process FA.EXE \
    -postScript ApplyTypes.java "$REPO_ROOT" \
    -scriptPath "$SCRIPT_DIR" -noanalysis
