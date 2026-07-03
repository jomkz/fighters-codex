#!/usr/bin/env bash
# Extracts all PE overlay DLLs from FA_1.LIB and FA_2.LIB into per-format subdirs.
#   FA_1.LIB -- FNT (bitmap fonts, 15 files)
#   FA_2.LIB -- BI, CAM, HUD, LAY, MC, MUS (115 files)
# Output: $FA_PROJECT/overlays/{BI,CAM,MC,HUD,LAY,FNT,MUS}/
#
# Requires fx on PATH or set FX_EXE. Prefers the repo build output if present.
#
# Usage:  scripts/ghidra/extract_overlays.sh
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/_env.sh"

if [[ -z "${FX_EXE:-}" ]]; then
    for c in "$SCRIPT_DIR"/../../build/{release,gcc,clang}/cli/fx; do
        [[ -x "$c" ]] && FX_EXE="$c" && break
    done
fi
FX_EXE="${FX_EXE:-fx}"

LIB1="$FA_INSTALL/FA_1.LIB"
LIB2="$FA_INSTALL/FA_2.LIB"
OVERLAY_ROOT="$FA_PROJECT/overlays"

echo "============================================================"
echo " FA overlay DLL extraction"
echo " Source 1 : $LIB1  (FNT)"
echo " Source 2 : $LIB2  (BI CAM HUD LAY MC MUS)"
echo " Output   : $OVERLAY_ROOT"
echo "============================================================"
echo

for lib in "$LIB1" "$LIB2"; do
    if [[ ! -f "$lib" ]]; then
        echo "ERROR: $lib not found." >&2
        exit 1
    fi
done

# Unpack both archives into a shared staging directory
STAGING="$OVERLAY_ROOT/_all"
mkdir -p "$STAGING"
echo "[1/3] Unpacking FA_1.LIB to staging dir..."
"$FX_EXE" lib unpack "$LIB1" "$STAGING"
echo "[1b/3] Unpacking FA_2.LIB to staging dir..."
"$FX_EXE" lib unpack "$LIB2" "$STAGING"

# Copy each format into its own subdirectory
echo
echo "[2/3] Sorting overlay files by extension..."
for fmt in BI CAM MC HUD LAY FNT MUS; do
    mkdir -p "$OVERLAY_ROOT/$fmt"
    find "$STAGING" -maxdepth 1 -iname "*.$fmt" -exec cp -f {} "$OVERLAY_ROOT/$fmt/" \;
    echo "  $fmt: copied to $OVERLAY_ROOT/$fmt"
done

# Also copy secondary game binaries for their own projects
echo
echo "[3/3] Copying secondary game binaries..."
SECONDARY="$OVERLAY_ROOT/secondary"
mkdir -p "$SECONDARY"
for f in IP.EXE WAIL32.DLL msapi.dll CDRVDL32.DLL CDRVHF32.DLL CDRVXF32.DLL COMMSC32.DLL; do
    if [[ -f "$FA_INSTALL/$f" ]]; then
        cp -f "$FA_INSTALL/$f" "$SECONDARY/"
        echo "  Copied $f"
    fi
done

echo
echo "============================================================"
echo " Extraction complete."
echo " Sources:      FA_1.LIB (FNT x15) + FA_2.LIB (BI/CAM/HUD/LAY/MC/MUS x115)"
echo " Overlay dirs: $OVERLAY_ROOT/{BI,CAM,MC,HUD,LAY,FNT,MUS}"
echo " Secondary:    $OVERLAY_ROOT/secondary"
echo " Next:         scripts/ghidra/import_overlays.sh"
echo "============================================================"
