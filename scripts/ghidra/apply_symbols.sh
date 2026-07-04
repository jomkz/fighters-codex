#!/usr/bin/env bash
# Apply db/symbols/*.csv to the fa-re Ghidra project (ApplySymbols.java).
# Writes to the project database; idempotent. See db/README.md.
#
# Usage: apply_symbols.sh [BINARY]   (default FA.EXE; ALL loops every binary in
#        subsystems.csv). ApplySymbols only applies the target binary's symbol
#        files — VAs collide across images.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/_env.sh"

REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
echo "PROJECT_DIR=$FA_PROJECT"
echo "REPO_ROOT=$REPO_ROOT"

apply_one() {
    echo "== ApplySymbols: $1 =="
    "$ANALYZE_HEADLESS" "$FA_PROJECT" fa-re -process "$1" \
        -postScript ApplySymbols.java "$REPO_ROOT" \
        -scriptPath "$SCRIPT_DIR" -noanalysis
}

BINARY="${1:-FA.EXE}"
if [[ "$BINARY" == "ALL" ]]; then
    for b in $(fa_binaries "$REPO_ROOT"); do apply_one "$b"; done
else
    apply_one "$BINARY"
fi
