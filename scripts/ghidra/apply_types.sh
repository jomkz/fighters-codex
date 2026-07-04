#!/usr/bin/env bash
# Apply db/types/*.h (+ db/types/<binary>/*.h) and the db/symbols type column to
# the fa-re Ghidra project (ApplyTypes.java). Run apply_symbols.sh first so the
# names exist; idempotent. Writes to the project database. See db/types/README.md.
#
# Usage: apply_types.sh [BINARY]   (default FA.EXE; ALL loops every binary).
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/_env.sh"

REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
echo "PROJECT_DIR=$FA_PROJECT"
echo "REPO_ROOT=$REPO_ROOT"

types_one() {
    echo "== ApplyTypes: $1 =="
    "$ANALYZE_HEADLESS" "$FA_PROJECT" fa-re -process "$1" \
        -postScript ApplyTypes.java "$REPO_ROOT" \
        -scriptPath "$SCRIPT_DIR" -noanalysis
}

BINARY="${1:-FA.EXE}"
if [[ "$BINARY" == "ALL" ]]; then
    for b in $(fa_binaries "$REPO_ROOT"); do types_one "$b"; done
else
    types_one "$BINARY"
fi
