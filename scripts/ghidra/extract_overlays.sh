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

LIB1="$(fa_find FA_1.LIB)" || { echo "ERROR: FA_1.LIB not found under $FA_INSTALL" >&2; exit 1; }
LIB2="$(fa_find FA_2.LIB)" || { echo "ERROR: FA_2.LIB not found under $FA_INSTALL" >&2; exit 1; }
OVERLAY_ROOT="$FA_PROJECT/overlays"

echo "============================================================"
echo " FA overlay DLL extraction"
echo " Source 1 : $LIB1  (FNT)"
echo " Source 2 : $LIB2  (BI CAM HUD LAY MC MUS)"
echo " Output   : $OVERLAY_ROOT"
echo "============================================================"
echo

# Unpack both archives into a temp staging dir, then keep only the ~130 overlay
# DLLs — the LIBs hold 7,406 entries, so the staging dir is removed after sorting
# rather than left to bloat $FA_PROJECT.
STAGING="$OVERLAY_ROOT/_all"
rm -rf "$STAGING"
mkdir -p "$STAGING"
trap 'rm -rf "$STAGING"' EXIT
echo "[1/2] Unpacking FA_1.LIB + FA_2.LIB to a temp staging dir..."
"$FX_EXE" lib unpack "$LIB1" "$STAGING"
"$FX_EXE" lib unpack "$LIB2" "$STAGING"

# Copy each format into its own subdirectory
echo
echo "[2/2] Sorting overlay files by extension..."
for fmt in BI CAM MC HUD LAY FNT MUS; do
    mkdir -p "$OVERLAY_ROOT/$fmt"
    find "$STAGING" -maxdepth 1 -iname "*.$fmt" -exec cp -f {} "$OVERLAY_ROOT/$fmt/" \;
    echo "  $fmt: copied to $OVERLAY_ROOT/$fmt"
done
rm -rf "$STAGING"; trap - EXIT   # drop the 7,406-entry staging dir

echo
echo "============================================================"
echo " Extraction complete."
echo " Sources:      FA_1.LIB (FNT x15) + FA_2.LIB (BI/CAM/HUD/LAY/MC/MUS x115)"
echo " Overlay dirs: $OVERLAY_ROOT/{BI,CAM,MC,HUD,LAY,FNT,MUS}"
echo " Next:         scripts/ghidra/import_overlays.sh"
echo "============================================================"
