#!/usr/bin/env bash
# One-command from-scratch workbench (#376).
#
# From an empty $FA_PROJECT and a full FA install (see game_files.txt), builds
# the canonical fa-re Ghidra project (FA.EXE + the six #247 companion binaries),
# applies db/ names and types, exports the local db/inventory/ ground truth, and
# verifies with check_status.py --check. Everything under $FA_PROJECT except the
# game inputs is regenerated.
#
# Unlike rebuild_audit.sh (a throwaway project, single binary, no types — the
# reproducibility check), bootstrap builds the canonical project and applies
# types.
#
# Usage:
#   scripts/ghidra/bootstrap.sh              full build + check
#   scripts/ghidra/bootstrap.sh --evidence   also regenerate run_all + overlays
#
# Prereqs — Ghidra 12.1 + JDK 21+ (resolved/validated by _env.sh), an fx build,
# and the manifest game files — are all checked up front.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/_env.sh"
REPO_ROOT="$(cd "$HERE/../.." && pwd)"

EVIDENCE=0
case "${1:-}" in
    --evidence) EVIDENCE=1 ;;
    "") ;;
    *) echo "Unknown option: $1  (only --evidence)" >&2; exit 1 ;;
esac

echo "============================================================"
echo " FA workbench bootstrap"
echo " Project : $FA_PROJECT"
echo " Install : $FA_INSTALL"
echo "============================================================"

# --- Prerequisite checks ---------------------------------------------------
echo
echo "[check] toolchain + inputs"
# Ghidra + JDK are already validated by _env.sh on source. Resolve fx:
if [[ -z "${FX_EXE:-}" ]]; then
    for c in "$REPO_ROOT"/build/{release,gcc,clang}/cli/fx; do
        [[ -x "$c" ]] && FX_EXE="$c" && break
    done
fi
FX_EXE="${FX_EXE:-$(command -v fx || true)}"
if [[ -z "$FX_EXE" || ! -x "$FX_EXE" ]]; then
    echo "ERROR: fx not found. Build it (cmake --build --preset gcc) or set FX_EXE." >&2
    exit 1
fi
export FX_EXE
echo "  ghidra : $GHIDRA_HOME"
echo "  fx     : $FX_EXE"

# Game-file manifest: every listed file must resolve (case-insensitively).
missing=()
while read -r name; do
    [[ -z "$name" ]] && continue
    fa_find "$name" >/dev/null || missing+=("$name")
done < <(sed 's/#.*//' "$HERE/game_files.txt" | awk '{print $1}')
if (( ${#missing[@]} )); then
    echo "ERROR: game files missing under $FA_INSTALL:" >&2
    printf '  %s\n' "${missing[@]}" >&2
    echo "See scripts/ghidra/game_files.txt; point FA_INSTALL at your install/mount." >&2
    exit 1
fi
echo "  inputs : all present under $FA_INSTALL"

# --- Build -----------------------------------------------------------------
run() { echo; echo ">>> $*"; "$@"; }

run "$HERE/setup_project.sh"          # import FA.EXE + FA.SMS
run "$HERE/import_targets.sh"         # the six #247 companion binaries
run "$HERE/apply_symbols.sh" ALL      # db/symbols/ names
run "$HERE/apply_types.sh" ALL        # db/types/ + the type column
run "$HERE/export_inventory.sh" ALL   # local db/inventory/ ground truth

if (( EVIDENCE )); then
    run "$HERE/run_all.sh"            # per-subsystem evidence dumps
    run "$HERE/run_overlays.sh"      # format-overlay DLL projects
fi

echo
echo ">>> check_status.py --check"
python3 "$REPO_ROOT/tools/check_status.py" --check

echo
echo "============================================================"
echo " Bootstrap complete — fa-re built, db applied, inventory exported, checks green."
(( EVIDENCE )) || echo " (add --evidence to also regenerate run_all + overlay outputs)"
echo "============================================================"
