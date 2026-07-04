# Shared environment for the Linux Ghidra headless suite.
# Sourced by every launcher; override any variable by exporting it first.
#
#   FA_PROJECT  -- Ghidra project + output root    (default: ~/src/fa)
#   FA_INSTALL  -- FA game files (FA.EXE, LIBs)    (default: $FA_PROJECT/game)
#   GHIDRA_HOME -- Ghidra install                  (default: newest ~/tools/ghidra_*_PUBLIC, else /opt/ghidra*)
#   JAVA_HOME   -- JDK 21+ with javac              (default: newest ~/tools/jdk-*, else system java)
#   GHIDRA_HEADLESS_MAXMEM -- headless JVM heap    (default: 8G; Ghidra's own default is only 2G)

export FA_PROJECT="${FA_PROJECT:-$HOME/src/fa}"
export FA_INSTALL="${FA_INSTALL:-$FA_PROJECT/game}"

# Ghidra ships a 2G headless heap, which throttles the whole-program decompile
# sweeps (DumpAllFunctions fans out one native decompiler per core via
# ParallelDecompiler). Give the JVM real room; core count is auto-detected by
# Ghidra's thread pools and left uncapped (no -Dcpu.core.limit). Both are
# overridable for smaller benches.
export GHIDRA_HEADLESS_MAXMEM="${GHIDRA_HEADLESS_MAXMEM:-8G}"

if [[ -z "${GHIDRA_HOME:-}" ]]; then
    for d in "$HOME"/tools/ghidra_*_PUBLIC /opt/ghidra*; do
        [[ -x "$d/support/analyzeHeadless" ]] && GHIDRA_HOME="$d"
    done
fi
if [[ ! -x "${GHIDRA_HOME:-}/support/analyzeHeadless" ]]; then
    echo "ERROR: Ghidra not found. Set GHIDRA_HOME or unpack a release under ~/tools/." >&2
    exit 1
fi
export GHIDRA_HOME

if [[ -z "${JAVA_HOME:-}" ]]; then
    for d in "$HOME"/tools/jdk-*; do
        [[ -x "$d/bin/javac" ]] && JAVA_HOME="$d"
    done
fi
if [[ -n "${JAVA_HOME:-}" ]]; then
    export JAVA_HOME
    export PATH="$JAVA_HOME/bin:$PATH"
elif ! command -v javac >/dev/null; then
    echo "ERROR: no JDK found. Set JAVA_HOME or unpack a JDK 21+ under ~/tools/." >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ANALYZE_HEADLESS="$GHIDRA_HOME/support/analyzeHeadless"

# Distinct binaries from db/subsystems.csv (the `binary` column) — the set of
# Ghidra programs the apply/export/type launchers can target. Used by the ALL
# target in those launchers.
fa_binaries() {
    local repo="${1:?repo root required}"
    tail -n +2 "$repo/db/subsystems.csv" | cut -d, -f3 | sort -u
}
