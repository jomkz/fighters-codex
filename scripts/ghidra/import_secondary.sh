#!/usr/bin/env bash
# Imports secondary FA binaries (IP.EXE, WAIL32.DLL, msapi.dll, and serial/CD-ROM DLLs)
# into separate Ghidra projects under $FA_PROJECT/secondary_projects/.
#
# Each binary gets its own project so analyses stay isolated.
#
# Prerequisite: the binaries must exist under $FA_INSTALL.
#
# Usage:
#   import_secondary.sh               -- import all secondary binaries
#   import_secondary.sh IP            -- import IP.EXE only
#   import_secondary.sh WAIL32        -- import WAIL32.DLL only
#   import_secondary.sh MSAPI         -- import msapi.dll only
#
# Output projects: $FA_PROJECT/secondary_projects/{ip,wail32,msapi,cdrom,serial}
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/_env.sh"

SEC_ROOT="$FA_PROJECT/secondary_projects"
TARGET="${1:-ALL}"
TARGET="${TARGET^^}"

import_bin() {
    local label="$1" bin="$2" proj_dir="$3" proj_name="$4"
    if [[ ! -f "$bin" ]]; then
        echo "[$label] WARNING: $bin not found -- skipping"
        return 0
    fi
    echo "[$label] Importing $bin ..."
    mkdir -p "$proj_dir"
    "$ANALYZE_HEADLESS" "$proj_dir" "$proj_name" \
        -import "$bin" -overwrite \
        -scriptPath "$SCRIPT_DIR"
    echo "[$label] Done."
    echo
}

echo "============================================================"
echo " FA secondary binary Ghidra import"
echo " Projects: $SEC_ROOT"
echo " Target  : $TARGET"
echo "============================================================"
echo

mkdir -p "$SEC_ROOT"

# IP.EXE -- multiplayer launcher, feeds AnalyzeNetwork
if [[ "$TARGET" == "IP" || "$TARGET" == "ALL" ]]; then
    import_bin IP "$FA_INSTALL/IP.EXE" "$SEC_ROOT/ip" fa-ip
fi

# WAIL32.DLL -- Miles Sound System (AIL) -- resolve FA.EXE imports
if [[ "$TARGET" == "WAIL32" || "$TARGET" == "ALL" ]]; then
    import_bin WAIL32 "$FA_INSTALL/WAIL32.DLL" "$SEC_ROOT/wail32" fa-wail32
fi

# msapi.dll -- MS API wrapper (purpose TBD)
if [[ "$TARGET" == "MSAPI" || "$TARGET" == "ALL" ]]; then
    import_bin MSAPI "$FA_INSTALL/msapi.dll" "$SEC_ROOT/msapi" fa-msapi
fi

# CD-ROM driver DLLs (CDRVDL32.DLL / HF32.DLL / XF32.DLL) -- low priority skim
if [[ "$TARGET" == "ALL" ]]; then
    for d in CDRVDL32.DLL CDRVHF32.DLL CDRVXF32.DLL; do
        import_bin "$d" "$FA_INSTALL/$d" "$SEC_ROOT/cdrom" fa-cdrom
    done
fi

# COMMSC32.DLL -- serial comms wrapper
if [[ "$TARGET" == "ALL" ]]; then
    import_bin COMMSC32 "$FA_INSTALL/COMMSC32.DLL" "$SEC_ROOT/serial" fa-serial
fi

echo "============================================================"
echo " Secondary import complete.  Projects: $SEC_ROOT"
echo "============================================================"
