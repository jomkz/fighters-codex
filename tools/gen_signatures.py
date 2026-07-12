#!/usr/bin/env python3
"""Materialize the signatures encoded in the FA.SMS names into db/ (#452).

`db/types/README.md` used to note that the func `type` column is "usually left empty
because Ghidra already demangles the FA.SMS name into a full signature". That knowledge is
real -- but it lives inside the Ghidra project, where neither the fxe generator (#280) nor
CI (which has no Ghidra) can see it. This tool writes it into `db/symbols/*.csv`, where it
becomes a reviewable, checkable fact.

    python3 tools/gen_signatures.py --write       # fill the type column
    python3 tools/gen_signatures.py --check       # CI: currency + agreement
    python3 tools/gen_signatures.py --self-test   # unit tests, no db/ needed

WHAT THE DECORATION PROVES, AND WHAT IT DOES NOT
------------------------------------------------
A C++-mangled name (`?COLFlatGround@@YIDJPAUF24_POINT3@@00@Z`) proves the complete
signature. A C decoration (`_FMFuelConsumption@4`, `@FMBurnNPCFuel@4`) proves the calling
convention and the argument byte count -- and nothing about the argument types, which is
why those get `undefined4` parameters rather than invented ones.

So the derivation is a FLOOR, not a ceiling. The per-subsystem recovery in #453 is expected
to *sharpen* these -- replacing an `undefined4` with a real `entity *` as the type is
recovered. `--check` therefore does not demand byte-equality with the derivation. It
enforces the part the name actually proves:

    a stored signature may sharpen a type, but it may never contradict the calling
    convention or the argument count the decoration establishes.

A hand-written signature that disagrees with the decoration is not a style difference; it
is a bug in the recovery, and this check is how we catch it.
"""

import argparse
import csv
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from fa_demangle import demangle_c, demangle_cpp, prototype  # noqa: E402

REPO = pathlib.Path(__file__).resolve().parent.parent
SYMBOLS = REPO / "db" / "symbols"
HEADER = ["va", "kind", "name", "display", "source", "confidence", "notes", "type"]

CONVS = ("__cdecl", "__stdcall", "__fastcall", "__pascal", "__thiscall")


def split_params(s):
    """Split a parameter list on top-level commas.

    Function-pointer parameters carry their own parenthesised argument lists, so a naive
    `split(",")` would over-count the arity -- exactly the number this tool is here to
    police.
    """
    out, depth, cur = [], 0, ""
    for ch in s:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


def parse_prototype(proto):
    """Read back a stored prototype -> (convention, arity), or None if unparseable."""
    i = proto.find("(")
    j = proto.rfind(")")
    if i < 0 or j < i:
        return None
    head, body = proto[:i], proto[i + 1:j]
    conv = next((c for c in CONVS if c in head), None)
    if conv is None:
        return None
    params = split_params(body)
    if params == ["void"] or not params:
        return conv, 0
    return conv, len(params)


def derive(name):
    """The signature the decorated name proves, or None if it proves no signature."""
    return prototype(name)


def iter_rows(path):
    with open(path, encoding="utf-8", newline="") as fh:
        rows = list(csv.reader(fh))
    return rows[0], rows[1:]


def write_rows(path, header, rows):
    with open(path, "w", encoding="utf-8", newline="") as fh:
        w = csv.writer(fh, lineterminator="\n")
        w.writerow(header)
        w.writerows(rows)


def process(write):
    """Fill (or verify) the type column across db/symbols/. Returns (stats, errors)."""
    stats = {"derived": 0, "filled": 0, "already": 0, "sharpened": 0, "no_signature": 0}
    errs = []
    for path in sorted(SYMBOLS.glob("*.csv")):
        rel = path.relative_to(REPO)
        header, rows = iter_rows(path)
        if header != HEADER:
            errs.append("%s: unexpected header" % rel)
            continue
        dirty = False
        for i, r in enumerate(rows, start=2):
            if len(r) != len(HEADER):
                continue
            va, kind, name, display, source, conf, notes, ctype = r
            if kind != "func" or source == "waiver":
                continue

            proto = derive(name)
            if proto is None:
                stats["no_signature"] += 1
                continue
            stats["derived"] += 1

            if not ctype.strip():
                if write:
                    r[7] = proto
                    dirty = True
                    stats["filled"] += 1
                else:
                    errs.append(
                        "%s:%d: %s -- the name encodes a signature but the type column is "
                        "empty; run `python3 tools/gen_signatures.py --write`" % (rel, i, name))
                continue

            # A signature is already stored. It may sharpen the derivation, but it must
            # not contradict what the decoration proves: convention and arity.
            want = parse_prototype(proto)
            got = parse_prototype(ctype.strip())
            if got is None:
                errs.append("%s:%d: %s -- type column is not a parseable prototype: %r"
                            % (rel, i, name, ctype))
                continue
            if got[0] != want[0]:
                errs.append("%s:%d: %s -- stored signature says %s but the name proves %s"
                            % (rel, i, name, got[0], want[0]))
            elif got[1] != want[1]:
                errs.append(
                    "%s:%d: %s -- stored signature takes %d argument(s) but the name proves "
                    "%d (%s)" % (rel, i, name, got[1], want[1],
                                 "@N byte count" if not name.startswith("?")
                                 else "C++ mangled parameter list"))
            elif ctype.strip() == proto:
                stats["already"] += 1
            else:
                stats["sharpened"] += 1
        if write and dirty:
            write_rows(path, header, rows)
    return stats, errs


# --- self-test ---------------------------------------------------------------------
CASES = [
    # C++ mangling -- the complete signature. Oracle: llvm-undname.
    ("?COLFlatGround@@YIDJPAUF24_POINT3@@00@Z",
     "char __fastcall COLFlatGround(long, F24_POINT3 *, F24_POINT3 *, F24_POINT3 *)"),
    ("?IntersectT@@YAJPAUF24_POINT@@JJ@Z",
     "long __cdecl IntersectT(F24_POINT *, long, long)"),
    ("?COLDrawInfo@@YGXGF@Z",
     "void __stdcall COLDrawInfo(unsigned short, short)"),
    ("?RepairTime@@YIJJ@Z", "long __fastcall RepairTime(long)"),
    ("?CheckForEvents1@@YGXXZ", "void __stdcall CheckForEvents1(void)"),
    # Back-reference `0` repeats the first COMPOUND argument -- the whole pointer type,
    # not the struct it points at.
    ("?DitherCode@@YAXPAEPAK0@Z",
     "void __cdecl DitherCode(unsigned char *, unsigned long *, unsigned char *)"),
    # Enum return, via the `?A` by-value prefix.
    ("?GetJoystickType@@YA?AW4JOYRESULT@@K@Z",
     "JOYRESULT __cdecl GetJoystickType(unsigned long)"),
    # Function-pointer parameter (P6): its inner comma must not inflate the arity.
    ("?_ValidateExecute@@YAHP6GHXZ@Z",
     "int __cdecl _ValidateExecute(int (__stdcall *)(void))"),
    # C decoration: convention + argument bytes, no types.
    ("_FMFuelConsumption@4", "undefined4 __stdcall FMFuelConsumption(undefined4)"),
    ("@FMBurnNPCFuel@4", "undefined4 __fastcall FMBurnNPCFuel(undefined4)"),
    ("_CTInit@0", "undefined4 __stdcall CTInit(void)"),
    # cdecl with no @N proves a convention but no arity -- a prototype would have to lie
    # about the argument count, so we yield nothing and leave it to #453.
    ("_CTVarDiff", None),
    # A name we coined ourselves proves nothing at all.
    ("CTLoadProgram", None),
]


def self_test():
    fails = []
    for name, want in CASES:
        got = derive(name)
        if got != want:
            fails.append("  %s\n    want: %s\n    got : %s" % (name, want, got))

    # split_params must not be fooled by a function pointer's inner commas.
    if split_params("int, void (__stdcall *)(int, char), long") != [
            "int", "void (__stdcall *)(int, char)", "long"]:
        fails.append("  split_params: function-pointer commas leaked into the arity")

    if parse_prototype("int __cdecl f(int (__stdcall *)(int, char), long)") != ("__cdecl", 2):
        fails.append("  parse_prototype: wrong arity for a function-pointer argument")
    if parse_prototype("void __stdcall f(void)") != ("__stdcall", 0):
        fails.append("  parse_prototype: (void) must read as arity 0")

    # A malformed decoration must yield nothing rather than a rounded guess.
    if demangle_c("_Weird@6") != ("Weird", "__stdcall", 6) or derive("_Weird@6") is not None:
        fails.append("  a non-multiple-of-4 @N must be refused, not rounded")
    if demangle_cpp("?Unhandled@@YAX_Q@Z") is not None:
        fails.append("  an unknown type code must yield None, not a guess")

    if fails:
        print("gen_signatures self-test FAILED:\n" + "\n".join(fails))
        return 1
    print("gen_signatures self-test: %d cases OK" % (len(CASES) + 5))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--write", action="store_true", help="fill the type column in db/symbols/")
    g.add_argument("--check", action="store_true", help="verify currency + agreement (CI)")
    g.add_argument("--self-test", action="store_true", help="run unit tests")
    args = ap.parse_args()

    if args.self_test:
        return self_test()

    stats, errs = process(write=args.write)
    for e in errs:
        print("error: %s" % e)
    if args.write:
        print("gen_signatures: %d derivable, %d filled, %d already current, %d sharpened by hand"
              % (stats["derived"], stats["filled"], stats["already"], stats["sharpened"]))
        print("                %d func rows encode no signature (left to #453)"
              % stats["no_signature"])
    else:
        print("gen_signatures --check: %d derivable signatures, %d current, %d sharpened, "
              "%d errors" % (stats["derived"], stats["already"], stats["sharpened"], len(errs)))
    return 1 if errs else 0


if __name__ == "__main__":
    sys.exit(main())
