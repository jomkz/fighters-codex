#!/usr/bin/env bash
# Reproducibility audit for the FA.EXE reconstruction program.
#
# Rebuilds the named Ghidra project FROM SCRATCH — a clean import of FA.EXE, a
# fresh auto-analysis, FA.SMS symbols, then db/symbols/ applied via
# ApplySymbols.java — and diffs the exported inventory against the working
# project's local db/inventory/ export (the baseline; never committed, #342 —
# there is no committed inventory to diff against). A clean diff proves db/ is
# the true source of truth (no hidden state lives only in the working project)
# and that ApplySymbols' function materialisation holds from a clean analysis.
# See db/README.md § Verification without a committed baseline.
#
# Uses a SEPARATE 'fa-re-audit' project so it never touches the working 'fa-re'
# project — the two can run concurrently. Raw output + report land outside the
# repo (AUDIT_OUT); commit only a curated summary.
#
# Usage: scripts/ghidra/rebuild_audit.sh [BINARY] [AUDIT_OUT_DIR]
#   BINARY defaults to FA.EXE. The <stem>.SMS import step is skipped when the
#   binary ships no symbol map (the overlays don't), so the same audit works for
#   FA.EXE and every overlay.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/_env.sh"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BINARY="${1:-FA.EXE}"
PROJ="fa-re-audit"
AUDIT_OUT="${2:-$FA_PROJECT/audit-inventory/$BINARY}"
SMS="$FA_INSTALL/${BINARY%.*}.SMS"
mkdir -p "$AUDIT_OUT"

echo "[audit] binary=$BINARY  fresh project $FA_PROJECT/$PROJ (removing any prior)"
rm -rf "$FA_PROJECT/$PROJ.rep" "$FA_PROJECT/$PROJ.gpr" "$FA_PROJECT/$PROJ.lock" \
       "$FA_PROJECT/$PROJ.lock~" 2>/dev/null || true

echo "[audit 1/4] import $BINARY + full auto-analysis (slow: minutes)"
"$ANALYZE_HEADLESS" "$FA_PROJECT" "$PROJ" -import "$FA_INSTALL/$BINARY" -overwrite \
    -scriptPath "$SCRIPT_DIR"

if [[ -f "$SMS" ]]; then
    echo "[audit 2/4] import $(basename "$SMS") symbols"
    "$ANALYZE_HEADLESS" "$FA_PROJECT" "$PROJ" -process "$BINARY" \
        -postScript ImportFASmsHeadless.java "$SMS" \
        -scriptPath "$SCRIPT_DIR" -noanalysis
else
    echo "[audit 2/4] no $(basename "$SMS") — skipping symbol-map import (overlay)"
fi

echo "[audit 3/4] apply db/symbols (ApplySymbols materialises functions)"
"$ANALYZE_HEADLESS" "$FA_PROJECT" "$PROJ" -process "$BINARY" \
    -postScript ApplySymbols.java "$REPO_ROOT" \
    -scriptPath "$SCRIPT_DIR" -noanalysis

echo "[audit 4/4] export fresh inventory -> $AUDIT_OUT"
"$ANALYZE_HEADLESS" "$FA_PROJECT" "$PROJ" -process "$BINARY" \
    -postScript ExportInventory.java "$REPO_ROOT" "$AUDIT_OUT" \
    -scriptPath "$SCRIPT_DIR" -noanalysis -readOnly

echo "[audit] diff fresh rebuild vs local db/inventory/$BINARY/ baseline"
python3 "$SCRIPT_DIR/rebuild_diff.py" "$REPO_ROOT/db/inventory/$BINARY" "$AUDIT_OUT" \
    | tee "$AUDIT_OUT/REPORT.md"
echo "[audit] done. report: $AUDIT_OUT/REPORT.md"
