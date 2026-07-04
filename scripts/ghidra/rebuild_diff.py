#!/usr/bin/env python3
"""Diff a committed db/inventory against a freshly-rebuilt one (reproducibility audit).

Usage: rebuild_diff.py <committed_dir> <rebuilt_dir>

Reports, for functions and referenced globals: counts, VAs present in only one
side, and name mismatches at shared VAs. A clean diff means the whole pipeline
(FA.EXE + FA.SMS + db/symbols -> project -> inventory) is reproducible from
scratch and db/ holds no hidden state. Stdlib-only.
"""
import csv
import sys
from pathlib import Path


def load_funcs(path):
    out = {}
    if path.exists():
        for r in csv.DictReader(path.open(encoding="utf-8")):
            out[r["va"]] = r["name"]
    return out


def load_globals(path):
    out = {}
    if path.exists():
        for r in csv.DictReader(path.open(encoding="utf-8")):
            out[r["va"]] = r["name"]
    return out


def diff_map(committed, rebuilt, label, lines):
    only_c = sorted(set(committed) - set(rebuilt))
    only_r = sorted(set(rebuilt) - set(committed))
    mism = sorted(va for va in set(committed) & set(rebuilt)
                  if committed[va] != rebuilt[va])
    lines.append("## %s" % label)
    lines.append("")
    lines.append("- committed: %d   rebuilt: %d   shared: %d"
                 % (len(committed), len(rebuilt),
                    len(set(committed) & set(rebuilt))))
    lines.append("- only in committed: %d" % len(only_c))
    lines.append("- only in rebuilt (newly discovered): %d" % len(only_r))
    lines.append("- name mismatches at shared VAs: %d" % len(mism))
    for va in only_c[:40]:
        lines.append("  - only-committed %s %s" % (va, committed[va]))
    for va in only_r[:40]:
        lines.append("  - only-rebuilt   %s %s" % (va, rebuilt[va]))
    for va in mism[:40]:
        lines.append("  - mismatch %s: committed %r vs rebuilt %r"
                     % (va, committed[va], rebuilt[va]))
    lines.append("")
    return len(only_c) + len(only_r) + len(mism)


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    committed, rebuilt = Path(sys.argv[1]), Path(sys.argv[2])
    lines = ["# Reconstruction reproducibility audit", "",
             "Clean rebuild (fresh project + FA.SMS + db/symbols) vs committed "
             "`db/inventory/`.", ""]
    total = 0
    total += diff_map(load_funcs(committed / "functions.csv"),
                      load_funcs(rebuilt / "functions.csv"), "Functions", lines)
    total += diff_map(load_globals(committed / "globals.csv"),
                      load_globals(rebuilt / "globals.csv"),
                      "Referenced globals", lines)
    verdict = ("REPRODUCIBLE — inventory rebuilds byte-for-identical from db/."
               if total == 0 else
               "%d differences — see above (Ghidra analysis variance or hidden "
               "state)." % total)
    lines.insert(3, "**Verdict: %s**" % verdict)
    lines.insert(4, "")
    print("\n".join(lines))
    return 0


if __name__ == "__main__":
    sys.exit(main())
