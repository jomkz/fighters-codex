#!/usr/bin/env bash
# Import the epic #247 overlay/companion binaries into the canonical fa-re Ghidra
# project as their own programs, then auto-analyse each. All are standard PE/i386
# (no Phar Lap PL patch, unlike the LIB-embedded data-overlay DLLs), so a plain
# -import works. FA.EXE is already in the project.
#
# Usage: import_targets.sh [BINARY ...]   (default: all six companion binaries)
# Each program's name = its filename, matching the subsystems.csv `binary` column.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/_env.sh"

TARGETS=("$@")
if [[ ${#TARGETS[@]} -eq 0 ]]; then
    TARGETS=(IP.EXE WAIL32.DLL CDRVDL32.DLL CDRVHF32.DLL CDRVXF32.DLL COMMSC32.DLL)
fi

echo "PROJECT_DIR=$FA_PROJECT   INSTALL=$FA_INSTALL"
for b in "${TARGETS[@]}"; do
    src="$(fa_find "$b" || true)"   # case-insensitive; empty if absent
    if [[ -z "$src" ]]; then
        echo "SKIP $b — not found under $FA_INSTALL" >&2
        continue
    fi
    echo "== import $b + full auto-analysis (slow: minutes) =="
    "$ANALYZE_HEADLESS" "$FA_PROJECT" fa-re -import "$src" -overwrite \
        -scriptPath "$SCRIPT_DIR"
done
echo "done. Next: apply_symbols.sh <BINARY> / export_inventory.sh <BINARY>."
