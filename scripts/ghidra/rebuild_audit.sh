#!/usr/bin/env bash
# Reproducibility audit for the FA.EXE reconstruction program.
#
# Rebuilds the named Ghidra project FROM SCRATCH — a clean import of FA.EXE, a
# fresh auto-analysis, FA.SMS symbols, then db/symbols/ applied via
# ApplySymbols.java — and diffs the exported inventory against the committed
# db/inventory/. A clean diff proves db/ is the true source of truth (no hidden
# state lives only in the working project) and that ApplySymbols' function
# materialisation holds from a clean analysis. See db/README.md.
#
# Uses a SEPARATE 'fa-re-audit' project so it never touches the working 'fa-re'
# project — the two can run concurrently. Raw output + report land outside the
# repo (AUDIT_OUT); commit only a curated summary.
#
# Usage: scripts/ghidra/rebuild_audit.sh [AUDIT_OUT_DIR]
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/_env.sh"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
PROJ="fa-re-audit"
AUDIT_OUT="${1:-$FA_PROJECT/audit-inventory}"
mkdir -p "$AUDIT_OUT"

echo "[audit] fresh project $FA_PROJECT/$PROJ (removing any prior)"
rm -rf "$FA_PROJECT/$PROJ.rep" "$FA_PROJECT/$PROJ.gpr" "$FA_PROJECT/$PROJ.lock" \
       "$FA_PROJECT/$PROJ.lock~" 2>/dev/null || true

echo "[audit 1/4] import FA.EXE + full auto-analysis (slow: minutes)"
"$ANALYZE_HEADLESS" "$FA_PROJECT" "$PROJ" -import "$FA_INSTALL/FA.EXE" -overwrite \
    -scriptPath "$SCRIPT_DIR"

echo "[audit 2/4] import FA.SMS symbols"
"$ANALYZE_HEADLESS" "$FA_PROJECT" "$PROJ" -process FA.EXE \
    -postScript ImportFASmsHeadless.java "$FA_INSTALL/FA.SMS" \
    -scriptPath "$SCRIPT_DIR" -noanalysis

echo "[audit 3/4] apply db/symbols (ApplySymbols materialises functions)"
"$ANALYZE_HEADLESS" "$FA_PROJECT" "$PROJ" -process FA.EXE \
    -postScript ApplySymbols.java "$REPO_ROOT" \
    -scriptPath "$SCRIPT_DIR" -noanalysis

echo "[audit 4/4] export fresh inventory -> $AUDIT_OUT"
"$ANALYZE_HEADLESS" "$FA_PROJECT" "$PROJ" -process FA.EXE \
    -postScript ExportInventory.java "$REPO_ROOT" "$AUDIT_OUT" \
    -scriptPath "$SCRIPT_DIR" -noanalysis -readOnly

echo "[audit] diff fresh vs committed db/inventory/"
python3 "$SCRIPT_DIR/rebuild_diff.py" "$REPO_ROOT/db/inventory" "$AUDIT_OUT" \
    | tee "$AUDIT_OUT/REPORT.md"
echo "[audit] done. report: $AUDIT_OUT/REPORT.md"
