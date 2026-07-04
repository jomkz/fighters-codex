#!/usr/bin/env bash
# Export db/inventory/<binary>/*.csv ground truth from the fa-re Ghidra project
# (ExportInventory.java). Run from the canonical Fedora project only — see
# db/README.md.
#
# Usage: export_inventory.sh [BINARY]   (default FA.EXE; ALL loops every binary).
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/_env.sh"

REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
echo "PROJECT_DIR=$FA_PROJECT"
echo "REPO_ROOT=$REPO_ROOT"

export_one() {
    echo "== ExportInventory: $1 -> db/inventory/$1/ =="
    "$ANALYZE_HEADLESS" "$FA_PROJECT" fa-re -process "$1" \
        -postScript ExportInventory.java "$REPO_ROOT" \
        -scriptPath "$SCRIPT_DIR" -noanalysis -readOnly
}

BINARY="${1:-FA.EXE}"
if [[ "$BINARY" == "ALL" ]]; then
    for b in $(fa_binaries "$REPO_ROOT"); do export_one "$b"; done
else
    export_one "$BINARY"
fi
