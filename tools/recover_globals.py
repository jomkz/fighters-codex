#!/usr/bin/env python3
"""Type the globals from the width the instructions actually touch (#455).

`db/symbols/*.csv` carries 4979 `data` rows and, before this, 32 of them had a type. The fxe
generator (#280) is meant to emit a *typed* globals registry; an untyped one is a list of
addresses.

    python3 tools/recover_globals.py --write       # fill the data type column
    python3 tools/recover_globals.py --check       # currency + re-validation
    python3 tools/recover_globals.py --self-test   # unit tests, no db/ needed

THE EVIDENCE
------------
An instruction's operand size *proves* how wide the access is: `MOV AL,[x]` touches one byte,
`MOV AX,[x]` two, `MOV EAX,[x]` four. That is a fact encoded in the instruction, not an
inference about it, and `ExportInventory` records it per address (globals.csv `widths`).

Validated against the 32 globals typed by hand: **24 comparable, 24 agree, 0 mismatches.**

WHAT THE WIDTH PROVES, AND WHAT IT DOES NOT
-------------------------------------------
It proves the SIZE. It says nothing about the SEMANTICS: a 4-byte global is equally consistent
with a `u32` counter and a `T_HANDLE *`, and 12 of the 32 hand-typed globals are pointers. So a
width of 4 becomes `undefined4` -- "4 bytes, type not recovered" -- the same honest idiom #452
used for the arguments an `@N` decoration counts but does not describe. Guessing `u32` there
would be a claim the evidence does not support, and pointers would be silently flattened into
integers.

Sharpening is expected: an `undefined4` becoming a real `entity *` as the type is recovered.

WHAT IS REFUSED
---------------
- **Indexed accesses** (`[base + reg]`) -- the address is an ARRAY base. Its width is the
  ELEMENT width, so typing it as a scalar would hand the port an object of the wrong size.
  40 rows.
- **Conflicting widths** -- the same address touched as both a byte and a dword is not a plain
  scalar; that is a finding for #454, not something to average.
- **No width evidence** -- the address is only ever taken, never dereferenced directly.

Struct *layouts* are #454's job. This column types an ADDRESS: interiors of an aggregate are
already separate rows under the referenced-globals rule (the "interior of X" waivers).
"""

import argparse
import bisect
import csv
import pathlib
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
DB = REPO / "db"
HEADER = ["va", "kind", "name", "display", "source", "confidence", "notes", "type"]

# Only these widths correspond to a scalar the port can name. Anything else (an 8-byte FPU
# load, say) is not something to type from width alone.
WIDTH_TYPE = {1: "undefined1", 2: "undefined2", 4: "undefined4"}

# Sizes of the types already in db/, so --check can re-prove the rule against them.
TYPE_SIZE = {
    "u8": 1, "s8": 1, "char": 1, "undefined1": 1,
    "u16": 2, "s16": 2, "undefined2": 2,
    "u32": 4, "s32": 4, "fixed24": 4, "undefined4": 4,
    "int": 4, "unsigned int": 4, "unsigned long": 4, "long": 4,
}


def sizeof(ctype):
    """Byte size of a db/ type string, or 0 if we cannot say."""
    t = ctype.strip()
    if t.endswith("*"):
        return 4               # 32-bit target: every pointer is a dword
    return TYPE_SIZE.get(t, 0)


def load_manifest(db=DB):
    with open(db / "subsystems.csv", newline="") as fh:
        return {r["slug"]: r["binary"] for r in csv.DictReader(fh)}


def load_globals(db=DB):
    """binary -> {va: {"widths": [int], "indexed": bool}}, or None if not exported.

    Keyed BY BINARY: a VA is unique only within one (IP.EXE bases at the same 0x00400000 as
    FA.EXE). The inventory is a local-only Ghidra export (#342), so its absence downgrades the
    checks rather than failing them.
    """
    inv = db / "inventory"
    if not inv.is_dir():
        return None
    out = {}
    for path in sorted(inv.glob("*/globals.csv")):
        with open(path, newline="") as fh:
            rdr = csv.DictReader(fh)
            if "widths" not in (rdr.fieldnames or []):
                continue      # a pre-#455 export; re-run export_inventory.sh
            per = {}
            for r in rdr:
                per[int(r["va"], 16)] = {
                    "widths": sorted({int(x) for x in r["widths"].split("|") if x}),
                    "indexed": r["indexed"] == "true",
                }
        out[path.parent.name] = per
    return out or None


def load_code_bodies(db=DB):
    """binary -> sorted [(lo, hi)] of function bodies, from the inventory export.

    Some `data` rows name an address that lies INSIDE a function -- MSVC CRT labels like
    `__NLG_Return2` are branch targets in the middle of code, not globals. Ghidra refuses to
    lay data over instructions, and it is right to: a "global" there is a misclassification,
    and typing it would put a fictional variable into the generator's registry.
    """
    inv = db / "inventory"
    out = {}
    if not inv.is_dir():
        return out
    for path in sorted(inv.glob("*/functions.csv")):
        spans = []
        with open(path, newline="") as fh:
            for r in csv.DictReader(fh):
                lo = int(r["va"], 16)
                spans.append((lo, lo + int(r["size"])))
        spans.sort()
        out[path.parent.name] = spans
    return out


def in_code(spans, va):
    """Is this address inside a function body? (bisect over sorted, disjoint spans)"""
    i = bisect.bisect_right(spans, (va, float("inf"))) - 1
    return i >= 0 and spans[i][0] <= va < spans[i][1]


def type_from_evidence(ev, is_code=False):
    """The type the access widths prove, or None. None is a legitimate answer."""
    if ev is None:
        return None
    if is_code:
        return None            # a label inside a function body is code, not a global
    if ev["indexed"]:
        return None            # array base: the width is the ELEMENT width, not the object's
    w = ev["widths"]
    if len(w) != 1:
        return None            # no evidence, or contradictory widths -> not a plain scalar
    return WIDTH_TYPE.get(w[0])


def process(write, glob_ev):
    stats = {"typed": 0, "filled": 0, "already": 0, "refused": 0, "in_code": 0}
    errs = []
    manifest = load_manifest()
    bodies = load_code_bodies()
    for path in sorted((DB / "symbols").glob("*.csv")):
        rel = path.relative_to(REPO)
        binary = manifest.get(path.stem)
        per = glob_ev.get(binary, {})
        spans = bodies.get(binary, [])
        with open(path, newline="") as fh:
            rows = list(csv.reader(fh))
        header, body = rows[0], rows[1:]
        if header != HEADER:
            errs.append("%s: unexpected header" % rel)
            continue
        dirty = False
        for i, r in enumerate(body, start=2):
            if len(r) != len(HEADER):
                continue
            va, kind, name, display, source, conf, notes, ctype = r
            if kind != "data" or source == "waiver":
                continue

            ev = per.get(int(va, 16))
            code = in_code(spans, int(va, 16))
            if code:
                stats["in_code"] += 1
            want = type_from_evidence(ev, is_code=code)
            if want is None:
                if not ctype.strip():
                    stats["refused"] += 1
                continue
            stats["typed"] += 1

            if not ctype.strip():
                if write:
                    r[7] = want
                    dirty = True
                    stats["filled"] += 1
                else:
                    errs.append("%s:%d: %s -- accessed as %d byte(s); the type column is empty; "
                                "run `python3 tools/recover_globals.py --write`"
                                % (rel, i, name, ev["widths"][0]))
                continue

            # Already typed -- by hand, or sharpened from a previous undefinedN. The width is
            # still ground truth about the SIZE, so a stored type must not contradict it.
            have = sizeof(ctype.strip())
            if have and have != ev["widths"][0]:
                errs.append("%s:%d: %s -- typed %r (%d bytes) but the code accesses it %d byte(s) "
                            "at a time" % (rel, i, name, ctype.strip(), have, ev["widths"][0]))
            else:
                stats["already"] += 1
        if write and dirty:
            with open(path, "w", newline="") as fh:
                w = csv.writer(fh, lineterminator="\n")
                w.writerow(header)
                w.writerows(body)
    return stats, errs


# --- self-test ---------------------------------------------------------------------
def self_test():
    fails = []
    cases = [
        ({"widths": [1], "indexed": False}, "undefined1", "a byte access proves one byte"),
        ({"widths": [2], "indexed": False}, "undefined2", "a word access proves two"),
        ({"widths": [4], "indexed": False}, "undefined4", "a dword access proves four"),
        # A 4-byte global may be a u32 or a pointer -- the width cannot tell them apart, and
        # 12 of the 32 hand-typed globals are pointers. undefined4 says the honest thing.
        ({"widths": [4], "indexed": True}, None, "an ARRAY base is not a scalar -> refuse"),
        ({"widths": [1, 4], "indexed": False}, None, "conflicting widths -> refuse"),
        ({"widths": [], "indexed": False}, None, "address taken but never read -> refuse"),
        ({"widths": [8], "indexed": False}, None, "an 8-byte access is not a scalar we name"),
        (None, None, "no inventory row -> refuse"),
    ]
    for ev, want, label in cases:
        got = type_from_evidence(ev)
        if got != want:
            fails.append("  %r: want %r, got %r  (%s)" % (ev, want, got, label))

    for t, want in [("u8", 1), ("s16", 2), ("u32", 4), ("T_HANDLE *", 4), ("SEQGR *", 4),
                    ("entity *", 4), ("bogus_t", 0)]:
        if sizeof(t) != want:
            fails.append("  sizeof(%s): want %d, got %d" % (t, want, sizeof(t)))

    # An address inside a function body is a code label (MSVC's __NLG_Return2), not a global:
    # Ghidra will not lay data over instructions, and typing it would put a fictional variable
    # into the generator's registry.
    if type_from_evidence({"widths": [4], "indexed": False}, is_code=True) is not None:
        fails.append("  a label inside a function body must not be typed as a global")
    spans = [(0x1000, 0x1010), (0x2000, 0x2004)]
    for va, want in [(0x1000, True), (0x100F, True), (0x1010, False), (0x1FFF, False),
                     (0x2002, True), (0x3000, False)]:
        if in_code(spans, va) != want:
            fails.append("  in_code(0x%X): want %s" % (va, want))

    if fails:
        print("recover_globals self-test FAILED:\n" + "\n".join(fails))
        return 1
    print("recover_globals self-test: %d cases OK" % (len(cases) + 7))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--write", action="store_true")
    g.add_argument("--check", action="store_true")
    g.add_argument("--self-test", action="store_true")
    args = ap.parse_args()

    if args.self_test:
        return self_test()

    ev = load_globals()
    if ev is None:
        print("recover_globals: no db/inventory/*/globals.csv with width evidence — that is a "
              "local-only Ghidra export (#342); skipping (export with "
              "scripts/ghidra/export_inventory.sh)")
        return 0

    stats, errs = process(write=args.write, glob_ev=ev)
    for e in errs:
        print("error: %s" % e)

    if args.write:
        print("recover_globals: %d globals typed from access width, %d filled, %d already current"
              % (stats["typed"], stats["filled"], stats["already"]))
        print("                 %d untyped rows have no usable width evidence (array bases, "
              "conflicting widths, address-taken-only)" % stats["refused"])
        if stats["in_code"]:
            print("                 %d `data` rows name an address INSIDE a function body — "
                  "code labels, not globals (e.g. MSVC's __NLG_Return2)" % stats["in_code"])
    else:
        print("recover_globals --check: %d typed from width, %d current, %d errors"
              % (stats["typed"], stats["already"], len(errs)))
    return 1 if errs else 0


if __name__ == "__main__":
    sys.exit(main())
