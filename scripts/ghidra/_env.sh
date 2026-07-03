# Shared environment for the Linux Ghidra headless suite.
# Sourced by every launcher; override any variable by exporting it first.
#
#   FA_PROJECT  -- Ghidra project + output root    (default: ~/src/fa)
#   FA_INSTALL  -- FA game files (FA.EXE, LIBs)    (default: $FA_PROJECT/game)
#   GHIDRA_HOME -- Ghidra install                  (default: newest ~/tools/ghidra_*_PUBLIC, else /opt/ghidra*)
#   JAVA_HOME   -- JDK 21+ with javac              (default: newest ~/tools/jdk-*, else system java)

export FA_PROJECT="${FA_PROJECT:-$HOME/src/fa}"
export FA_INSTALL="${FA_INSTALL:-$FA_PROJECT/game}"

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
