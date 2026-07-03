#!/usr/bin/env bash
# Full overlay pipeline: extract PE DLLs from FA_2.LIB, import into Ghidra,
# and optionally run analysis scripts or import secondary binaries.
#
# Usage:
#   run_overlays.sh                    -- extract + import all formats
#   run_overlays.sh --extract          -- extraction step only
#   run_overlays.sh --import           -- import step only (extraction must have run first)
#   run_overlays.sh --import BI        -- import a single format (BI, CAM, MC, HUD, LAY, FNT, MUS)
#   run_overlays.sh --analyze          -- run DumpOverlayDLL against all overlay projects
#   run_overlays.sh --analyze BI       -- run DumpOverlayDLL against a single overlay project
#   run_overlays.sh --analyze-cam      -- run AnalyzeCAMDLL against the CAM overlay project
#   run_overlays.sh --analyze-mc       -- run AnalyzeMCDLL against the MC overlay project
#   run_overlays.sh --secondary            -- import secondary binaries (IP.EXE, WAIL32.DLL, msapi.dll, CD-ROM, serial)
#   run_overlays.sh --secondary IP         -- import a single secondary binary
#   run_overlays.sh --analyze-secondary    -- run DumpOverlayDLL against all secondary projects
#
# Output projects: $FA_PROJECT/overlay_projects/{BI,CAM,MC,HUD,LAY,FNT,MUS,secondary}
#                  $FA_PROJECT/secondary_projects/{ip,wail32,msapi,cdrom,serial}
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/_env.sh"

PROJECTS_ROOT="$FA_PROJECT/overlay_projects"
SEC_ROOT="$FA_PROJECT/secondary_projects"

# Runs DumpOverlayDLL.java against one overlay project (with analysis).
analyze_overlay() {
    local fmt="$1"
    local proj_dir="$PROJECTS_ROOT/$fmt"
    if [[ ! -d "$proj_dir" ]]; then
        echo "  [$fmt] Project dir not found: $proj_dir -- run --import first"
        return 0
    fi
    # Clear output file so each run starts fresh (DumpOverlayDLL appends per-DLL)
    rm -f "$FA_PROJECT/output/Overlay_$fmt.txt"
    echo "  [$fmt] Running DumpOverlayDLL.java (with analysis)..."
    "$ANALYZE_HEADLESS" "$proj_dir" "fa-$fmt/${fmt}_pe" \
        -process "*.$fmt" \
        -postScript DumpOverlayDLL.java "$fmt" \
        -scriptPath "$HERE" < /dev/null
    echo "  [$fmt] Done. Output: $FA_PROJECT/output/Overlay_$fmt.txt"
}

# Runs a named analysis script against one overlay project (with analysis).
analyze_overlay_script() {
    local fmt="$1" script="$2"
    local proj_dir="$PROJECTS_ROOT/$fmt"
    if [[ ! -d "$proj_dir" ]]; then
        echo "  [$fmt] Project dir not found: $proj_dir -- run --import first"
        return 0
    fi
    rm -f "$FA_PROJECT/output/${script%.java}.txt"
    echo "  [$fmt] Running $script (with analysis)..."
    "$ANALYZE_HEADLESS" "$proj_dir" "fa-$fmt/${fmt}_pe" \
        -process "*.$fmt" \
        -postScript "$script" \
        -scriptPath "$HERE" < /dev/null
    echo "  [$fmt] Done."
}

# Runs DumpOverlayDLL against one secondary project.
analyze_secondary() {
    local label="$1" proj_dir="$2" proj_name="$3" binary="$4"
    if [[ ! -d "$proj_dir" ]]; then
        echo "  [$label] Project dir not found: $proj_dir -- run --secondary first"
        return 0
    fi
    rm -f "$FA_PROJECT/output/Overlay_$label.txt"
    echo "  [$label] Running DumpOverlayDLL.java (with analysis)..."
    "$ANALYZE_HEADLESS" "$proj_dir" "$proj_name" \
        -process "$binary" \
        -postScript DumpOverlayDLL.java "$label" \
        -scriptPath "$HERE" < /dev/null
    echo "  [$label] Done."
}

DO_EXTRACT=1
DO_IMPORT=1
DO_ANALYZE=0
DO_ANALYZE_CAM=0
DO_ANALYZE_MC=0
DO_SECONDARY=0
DO_ANALYZE_SECONDARY=0
IMPORT_TARGET=ALL
ANALYZE_TARGET=ALL
SECONDARY_TARGET=ALL

case "${1:-}" in
    --extract)     DO_IMPORT=0 ;;
    --import)      DO_EXTRACT=0; IMPORT_TARGET="${2:-ALL}" ;;
    --analyze)     DO_EXTRACT=0; DO_IMPORT=0; DO_ANALYZE=1; ANALYZE_TARGET="${2:-ALL}" ;;
    --analyze-cam) DO_EXTRACT=0; DO_IMPORT=0; DO_ANALYZE_CAM=1 ;;
    --analyze-mc)  DO_EXTRACT=0; DO_IMPORT=0; DO_ANALYZE_MC=1 ;;
    --secondary)   DO_EXTRACT=0; DO_IMPORT=0; DO_SECONDARY=1; SECONDARY_TARGET="${2:-ALL}" ;;
    --analyze-secondary) DO_EXTRACT=0; DO_IMPORT=0; DO_ANALYZE_SECONDARY=1 ;;
    "") ;;
    *) echo "Unknown option: $1" >&2; exit 1 ;;
esac

echo "============================================================"
echo " FA overlay DLL pipeline"
[[ $DO_EXTRACT == 1 ]]           && echo "  Step 1: extract overlays from FA_2.LIB"
[[ $DO_IMPORT == 1 ]]            && echo "  Step 2: import overlays into Ghidra ($IMPORT_TARGET)"
[[ $DO_ANALYZE == 1 ]]           && echo "  Step A: analyze overlay DLLs with DumpOverlayDLL ($ANALYZE_TARGET)"
[[ $DO_ANALYZE_CAM == 1 ]]       && echo "  Step A: analyze CAM DLLs with AnalyzeCAMDLL"
[[ $DO_ANALYZE_MC == 1 ]]        && echo "  Step A: analyze MC DLLs with AnalyzeMCDLL"
[[ $DO_SECONDARY == 1 ]]         && echo "  Step S: import secondary binaries ($SECONDARY_TARGET)"
[[ $DO_ANALYZE_SECONDARY == 1 ]] && echo "  Step A: analyze secondary projects with DumpOverlayDLL"
echo "============================================================"
echo

if [[ $DO_EXTRACT == 1 ]]; then
    "$HERE/extract_overlays.sh"
    echo
fi

if [[ $DO_IMPORT == 1 ]]; then
    "$HERE/import_overlays.sh" "$IMPORT_TARGET"
    echo
fi

if [[ $DO_ANALYZE == 1 ]]; then
    if [[ "${ANALYZE_TARGET^^}" == "ALL" ]]; then
        for fmt in BI CAM MC HUD LAY FNT MUS; do
            echo
            echo "--- DumpOverlayDLL [$fmt] ---"
            analyze_overlay "$fmt"
        done
    else
        echo
        echo "--- DumpOverlayDLL [$ANALYZE_TARGET] ---"
        analyze_overlay "$ANALYZE_TARGET"
    fi
    echo
fi

if [[ $DO_ANALYZE_CAM == 1 ]]; then
    echo
    echo "--- AnalyzeCAMDLL ---"
    analyze_overlay_script CAM AnalyzeCAMDLL.java
    echo
fi

if [[ $DO_ANALYZE_MC == 1 ]]; then
    echo
    echo "--- AnalyzeMCDLL ---"
    analyze_overlay_script MC AnalyzeMCDLL.java
    echo
fi

if [[ $DO_ANALYZE_SECONDARY == 1 ]]; then
    for spec in \
        "IP.EXE|$SEC_ROOT/ip|fa-ip|IP.EXE" \
        "WAIL32.DLL|$SEC_ROOT/wail32|fa-wail32|WAIL32.DLL" \
        "msapi.dll|$SEC_ROOT/msapi|fa-msapi|msapi.dll" \
        "CDRVDL32.DLL|$SEC_ROOT/cdrom|fa-cdrom|CDRVDL32.DLL" \
        "CDRVHF32.DLL|$SEC_ROOT/cdrom|fa-cdrom|CDRVHF32.DLL" \
        "CDRVXF32.DLL|$SEC_ROOT/cdrom|fa-cdrom|CDRVXF32.DLL" \
        "COMMSC32.DLL|$SEC_ROOT/serial|fa-serial|COMMSC32.DLL"
    do
        IFS='|' read -r label proj_dir proj_name binary <<< "$spec"
        echo
        echo "--- DumpOverlayDLL [$label] ---"
        analyze_secondary "$label" "$proj_dir" "$proj_name" "$binary"
    done
    echo
fi

if [[ $DO_SECONDARY == 1 ]]; then
    echo
    echo "--- Secondary binary import ---"
    "$HERE/import_secondary.sh" "$SECONDARY_TARGET"
    echo
fi

echo
echo "============================================================"
echo " Overlay pipeline complete."
echo " Overlay projects  : $FA_PROJECT/overlay_projects/"
echo " Secondary projects: $FA_PROJECT/secondary_projects/"
echo "============================================================"
