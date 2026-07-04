#!/usr/bin/env bash
# Export db/inventory/*.csv ground truth from the fa-re Ghidra project
# (ExportInventory.java). Run from the canonical Fedora project only —
# see db/README.md.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/_env.sh"

REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
echo "PROJECT_DIR=$FA_PROJECT"
echo "REPO_ROOT=$REPO_ROOT"
"$ANALYZE_HEADLESS" "$FA_PROJECT" fa-re -process FA.EXE \
    -postScript ExportInventory.java "$REPO_ROOT" \
    -scriptPath "$SCRIPT_DIR" -noanalysis -readOnly
