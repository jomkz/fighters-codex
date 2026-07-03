#!/usr/bin/env bash
# Imports all extracted overlay DLLs and secondary game binaries into
# per-format Ghidra projects under $FA_PROJECT/overlay_projects/.
#
# Run extract_overlays.sh first to populate the overlay directories.
#
# Note on Phar Lap PE format:
#   FA overlay DLLs use signature PL\0\0 instead of the standard PE\0\0.
#   Ghidra 12.1 PE loader rejects these. This script patches byte 1 at the
#   PE header offset (read from MZ stub at 0x3C) from 0x4C to 0x45.
#   The patched copies live at $FA_PROJECT/overlays/{fmt}_pe/.
#
# Usage:  scripts/ghidra/import_overlays.sh [FORMAT]
#   FORMAT (optional): BI, CAM, MC, HUD, LAY, FNT, MUS, or ALL (default)
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/_env.sh"

OVERLAY_ROOT="$FA_PROJECT/overlays"
PROJECTS_ROOT="$FA_PROJECT/overlay_projects"
TARGET="${1:-ALL}"

# Patch PL\0\0 -> PE\0\0 at the PE header offset (from the MZ stub at 0x3C);
# copies only -- originals are preserved in the source dir.
patch_pl_to_pe() {
    python3 - "$1" "$2" <<'EOF'
import struct, sys
data = bytearray(open(sys.argv[1], "rb").read())
off = struct.unpack_from("<i", data, 0x3C)[0]
if data[off:off + 2] == b"PL":
    data[off + 1] = 0x45
open(sys.argv[2], "wb").write(data)
EOF
}

import_one() {
    local fmt="$1"
    local src_dir="$OVERLAY_ROOT/$fmt"
    local pe_dir="$OVERLAY_ROOT/${fmt}_pe"
    local proj_dir="$PROJECTS_ROOT/$fmt"
    local proj_name="fa-$fmt"

    if [[ ! -d "$src_dir" ]]; then
        echo "  [$fmt] Source dir not found: $src_dir"
        return 0
    fi

    mkdir -p "$pe_dir"
    local count=0
    for f in "$src_dir"/*; do
        [[ -f "$f" ]] || continue
        patch_pl_to_pe "$f" "$pe_dir/$(basename "$f")"
        count=$((count + 1))
    done
    echo "  [$fmt] Patched $count files to PE signature"

    mkdir -p "$proj_dir"
    echo "  [$fmt] Importing into Ghidra project $proj_name..."
    "$ANALYZE_HEADLESS" "$proj_dir" "$proj_name" -import "$pe_dir" -overwrite \
        -scriptPath "$SCRIPT_DIR"
    echo "  [$fmt] Done."
}

echo "============================================================"
echo " FA overlay DLL Ghidra import"
echo " Projects: $PROJECTS_ROOT"
echo " Target  : $TARGET"
echo "============================================================"
echo

mkdir -p "$PROJECTS_ROOT"

if [[ "${TARGET^^}" == "ALL" ]]; then
    for fmt in BI CAM MC HUD LAY FNT MUS; do
        echo
        echo "--- $fmt ---"
        import_one "$fmt"
    done
else
    import_one "$TARGET"
fi

SEC_DIR="$OVERLAY_ROOT/secondary"
if [[ -d "$SEC_DIR" ]]; then
    echo
    echo "--- secondary ---"
    mkdir -p "$PROJECTS_ROOT/secondary"
    "$ANALYZE_HEADLESS" "$PROJECTS_ROOT/secondary" fa-secondary -import "$SEC_DIR" -overwrite \
        -scriptPath "$SCRIPT_DIR"
    echo "  [secondary] Done."
fi

echo
echo "============================================================"
echo " Import complete.  Projects: $PROJECTS_ROOT"
echo "============================================================"
