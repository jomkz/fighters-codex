#!/usr/bin/env bash
# Runs every FA.EXE analysis script; each writes its own file under
# $FA_PROJECT/output/.
#
# Usage:
#   run_all.sh            -- run all scripts serially (project must already exist)
#   run_all.sh --setup    -- create/refresh the Ghidra project first, then run all
#   run_all.sh -j N       -- run N analyzers concurrently (see below)
#
# All scripts run -readOnly: they only produce evidence files, and any functions
# they materialise for the dump are transient scaffolding (persistent
# materialisation is ApplySymbols' job). Keeping the project frozen makes the
# outputs reproducible run to run — no analysis-state accumulation.
#
# Parallelism (-j N, #414): Ghidra takes an exclusive lock on the project
# directory, so workers can't share one project. Each worker instead gets its
# own throwaway copy of the 50 MB fa-re project and runs a slice of the roster
# against it; outputs still land in the one shared output dir (each script
# writes a distinct file). N is capped to nproc-2.
#
# Output files: $FA_PROJECT/output/Analyze*.txt
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/_env.sh"

JOBS=1
DO_SETUP=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --setup)   DO_SETUP=1; shift ;;
        -j|--jobs) JOBS="${2:?-j needs a worker count}"; shift 2 ;;
        --jobs=*)  JOBS="${1#*=}"; shift ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

if (( DO_SETUP )); then
    echo "Running project setup first..."
    "$HERE/setup_project.sh"
    echo
fi

# The roster is single-sourced from the Analyze*.java files themselves: every
# one runs against FA.EXE except the overlay-DLL scripts below, which target
# their own projects via run_overlays.sh. A new Analyze*.java therefore joins
# run_all automatically — roster drift (the #373 gap) cannot recur. Keep this
# exclusion list in sync with any new overlay-only analyzers.
OVERLAY_ONLY="AnalyzeCAMDLL.java AnalyzeMCDLL.java"

SCRIPTS=()
for path in "$HERE"/Analyze*.java; do
    s="$(basename "$path")"
    [[ " $OVERLAY_ONLY " == *" $s "* ]] && continue
    SCRIPTS+=("$s")
done
# Fixed tail: struct/global recovery + the whole-image decompile (not Analyze*).
SCRIPTS+=(DumpGlobals.java RecoverStructs.java DumpAllFunctions.java)

# Run one analysis script -readOnly against the given project directory.
# The caller redirects stdout/stderr as needed (redirection can't be passed
# through a variable, so it must live at the call site).
run_one() {
    local proj="$1" script="$2"
    "$ANALYZE_HEADLESS" "$proj" fa-re -process FA.EXE \
        -postScript "$script" -scriptPath "$HERE" -noanalysis -readOnly < /dev/null
}

# Cap the worker count to the machine.
NCPU="$(nproc 2>/dev/null || echo 4)"
MAXJ=$(( NCPU > 2 ? NCPU - 2 : 1 ))
if (( JOBS > MAXJ )); then
    echo "run_all: capping -j $JOBS to $MAXJ (nproc-2)"
    JOBS=$MAXJ
fi
(( JOBS < 1 )) && JOBS=1

if (( JOBS == 1 )); then
    echo "Running ${#SCRIPTS[@]} FA.EXE analysis scripts (serial)..."
    echo
    for s in "${SCRIPTS[@]}"; do
        echo "  $s"
        run_one "$FA_PROJECT" "$s"
    done
else
    echo "Running ${#SCRIPTS[@]} FA.EXE analysis scripts across $JOBS workers..."
    echo
    PARDIR="$FA_PROJECT/runall-workers"
    rm -rf "$PARDIR"; mkdir -p "$PARDIR"
    trap 'rm -rf "$PARDIR"' EXIT
    for ((i = 0; i < JOBS; i++)); do
        mkdir -p "$PARDIR/w$i"
        cp -r "$FA_PROJECT/fa-re.gpr" "$FA_PROJECT/fa-re.rep" "$PARDIR/w$i/"
        find "$PARDIR/w$i" -name '*.lock' -delete 2>/dev/null || true
    done
    # Modest per-worker heap so N JVMs fit in RAM (analysis stays well under 1 GB).
    export GHIDRA_HEADLESS_MAXMEM="${PAR_HEAP:-2G}"

    worker() {
        local wi="$1"; shift
        for s in "$@"; do
            echo "  [w$wi] $s"
            if ! run_one "$PARDIR/w$wi" "$s" > "$PARDIR/w$wi-${s%.java}.log" 2>&1; then
                echo "  [w$wi] FAILED: $s (see $PARDIR/w$wi-${s%.java}.log)" >&2
            fi
        done
    }

    # Round-robin the roster across workers; each worker runs its slice serially.
    declare -a BUCKET
    for ((k = 0; k < ${#SCRIPTS[@]}; k++)); do
        BUCKET[$((k % JOBS))]+=" ${SCRIPTS[k]}"
    done
    for ((i = 0; i < JOBS; i++)); do
        worker "$i" ${BUCKET[$i]:-} &
    done
    wait
fi

echo
echo "All scripts complete. Output: $FA_PROJECT/output/"
