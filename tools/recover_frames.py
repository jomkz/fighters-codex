#!/usr/bin/env python3
"""Recover signatures for the #453 TAIL from callee-side frame evidence.

The tail is what the earlier passes could not reach:

  #452  the FA.SMS decorations spell out a signature -- 816 signed, no RE at all.
  #453  a cdecl caller pops its own arguments after the CALL, and cdecl passes nothing
        in registers, so that byte count IS the arity -- 154 signed from call sites.

What is left has neither: no decoration to read, and callers that clean up nothing
visible (MSVC merges or defers the pop). So the evidence has to come from the CALLEE.

    ret_imm     the RET's actual operand -- an encoded fact, not Ghidra's inferred
                getStackPurgeSize (which disagreed with the real operand 179 times).
    reads_ecx   whether ECX/EDX are read as INPUTS before being written.
    /reads_edx  Matched on the BASE register, so CX and CL count: a fastcall callee
                taking a ushort receives it in CX, and an exact-name match sees no ECX.

RULE (the only one that survived measurement):

    ret_imm > 0                     the callee cleans its own stack -> callee-cleans
    AND no ECX/EDX read as input    ...and takes nothing in registers -> stdcall
    => stdcall, arity = ret_imm / 4

Why the register test only ever DISQUALIFIES: every attempt to infer arity *from*
register use failed (7.6-16% wrong -- you cannot distinguish an incoming register
argument from a scratch use). The converse is sound, and it is all this needs: a
function that reads ECX/EDX as an input MIGHT be taking arguments there, so its RET
operand is not the whole arity -- refuse it. Refusing on a maybe costs recall.
Counting on a maybe costs correctness.

MEASURED against the 970 signatures the decorations prove independently:

    144 / 144 correct, 0 false positives
    fires on 0 of 242 known __fastcall functions   <- the counterexample class

That last line is the point. A rule validated only on the class it was designed for
measures nothing: "no cleanup + no pushes => 0 args" scored 18/18 on known cdecl and
misfired on 144 of 178 known fastcall. Always hunt the counterexample.

REJECTED, and recorded so they are not tried again (see db/types/README.md):

  - arity from the callee's [EBP + N] argument reads. FA.EXE is built with frame-pointer
    omission: only 26 of 733 unsigned functions have an EBP frame at all.
  - arity from Ghidra's normalized stack references (which do survive FPO): 9 false
    positives / 221. It UNDERCOUNTS, because a callee that never touches its trailing
    argument leaves no reference to it -- true arity 10, predicted 1.
  - arity from counting the caller's PUSHes: 74.6% precision. MSVC stages arguments with
    `SUB ESP,N` + `MOV [ESP+k]` and pushes non-argument values, so the count is not the
    arity.
  - the two above, gated on AGREEING with each other: 0 false positives, but it fires on
    just 6 functions in the tail-like domain -- six samples cannot establish a precision,
    and an unproven rule is not a rule. Left unapplied on purpose.

Usage:
    python3 tools/recover_frames.py            # report what the rule proves
    python3 tools/recover_frames.py --write    # write it into db/symbols/*.csv
"""
import argparse
import collections
import csv
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DB = os.path.join(ROOT, "db")
INV = os.path.join(DB, "inventory")

MAX_ARITY = 16  # a RET operand implying more is a misread frame, not a 17-argument function


def load_frames():
    """(binary, va) -> frame observations.

    Keyed by BINARY as well as address, and that is not pedantry: the comms DLLs all load at
    0x10000000 and IP.EXE collides with FA.EXE at 0x00400000, so an address alone does not name
    a function. Keying on the VA alone silently merges evidence from different binaries -- it
    typed five comms-DLL functions from the arity of whatever happened to sit at the same
    address in another module (`ser_rs232_block` took a 1-argument signature from a function
    whose real RET operand is 12). A wrong signature is worse than none, and this is exactly
    how one gets written.
    """
    frames = {}
    for path in glob.glob(os.path.join(INV, "*", "frames.csv")):
        binary = os.path.basename(os.path.dirname(path))
        with open(path, newline="", encoding="utf-8") as fh:
            for r in csv.DictReader(fh):
                frames[(binary, int(r["va"], 16))] = {
                    "ret": int(r["ret_imm"]),
                    "ecx": r["reads_ecx"] == "true",
                    "edx": r["reads_edx"] == "true",
                }
    return frames


def binary_of_slug():
    """subsystem slug -> the binary it lives in (db/subsystems.csv)."""
    with open(os.path.join(DB, "subsystems.csv"), newline="", encoding="utf-8") as fh:
        return {r["slug"]: r["binary"] for r in csv.DictReader(fh)}


def parse_sig(sig):
    """('stdcall', 3) from a stored signature string, for the validation oracle."""
    m = re.search(r"__(cdecl|stdcall|fastcall)", sig)
    conv = m.group(1) if m else "cdecl"
    args = sig[sig.index("(") + 1:sig.rindex(")")].strip()
    return conv, (0 if args in ("", "void") else len(args.split(",")))


def predict(f):
    """The rule. Returns (conv, arity) only when the evidence PROVES it."""
    if f is None:
        return None
    if f["ret"] <= 0:          # caller-cleans (or unknown): the RET says nothing about arity
        return None
    if f["ecx"] or f["edx"]:   # might take arguments in registers -> the RET is not the whole story
        return None
    if f["ret"] % 4:           # not a whole number of dword slots: refuse rather than round
        return None
    arity = f["ret"] // 4
    if arity > MAX_ARITY:
        return None
    return ("stdcall", arity)


def signature(name, arity):
    """`undefined4 __stdcall name(undefined4, ...)` -- the honest shape.

    undefined4 says "4 bytes, semantics not recovered" (#452/#455). The RET operand proves
    how many dword slots there are and nothing whatever about what is in them, so writing
    `int` or `char *` here would invent a fact the evidence never carried.
    """
    ident = re.match(r"^[_@]?([A-Za-z_]\w*?)(@\d+)?$", name)
    ident = ident.group(1) if ident else name
    params = ", ".join(["undefined4"] * arity) if arity else "void"
    return "undefined4 __stdcall %s(%s)" % (ident, params)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true", help="write signatures into db/symbols/")
    args = ap.parse_args()

    if not glob.glob(os.path.join(INV, "*", "frames.csv")):
        print("no db/inventory/*/frames.csv -- run scripts/ghidra/export_inventory.sh ALL",
              file=sys.stderr)
        return 1

    frames = load_frames()
    binaries = binary_of_slug()

    # --- validate against the decorations, every run. The rule does not get to be
    # trusted on the strength of having been measured once, in a session nobody can see.
    ok = bad = 0
    fastcall_fires = 0
    for path in sorted(glob.glob(os.path.join(DB, "symbols", "*.csv"))):
        binary = binaries[os.path.basename(path)[:-4]]
        with open(path, newline="", encoding="utf-8") as fh:
            for r in csv.DictReader(fh):
                if r["kind"] != "func" or r["source"] == "waiver" or not r["type"].strip():
                    continue
                p = predict(frames.get((binary, int(r["va"], 16))))
                if p is None:
                    continue
                truth = parse_sig(r["type"])
                if p == truth:
                    ok += 1
                else:
                    bad += 1
                    print("  FALSE POSITIVE %s: known %r, rule says %r"
                          % (r["va"], truth, p), file=sys.stderr)
                if truth[0] == "fastcall":
                    fastcall_fires += 1

    print("oracle: %d/%d correct; fired on %d known __fastcall (must be 0)"
          % (ok, ok + bad, fastcall_fires))
    if bad or fastcall_fires:
        print("REFUSING TO WRITE: the rule no longer validates.", file=sys.stderr)
        return 1

    # --- apply to the tail
    total = 0
    per_sub = collections.Counter()
    for path in sorted(glob.glob(os.path.join(DB, "symbols", "*.csv"))):
        slug = os.path.basename(path)[:-4]
        binary = binaries[slug]
        with open(path, newline="", encoding="utf-8") as fh:
            rows = list(csv.DictReader(fh))
            cols = rows[0].keys() if rows else []
        changed = False
        for r in rows:
            if r["kind"] != "func" or r["source"] == "waiver" or r["type"].strip():
                continue
            p = predict(frames.get((binary, int(r["va"], 16))))
            if p is None:
                continue
            r["type"] = signature(r["name"], p[1])
            per_sub[slug] += 1
            total += 1
            changed = True
        if changed and args.write:
            with open(path, "w", newline="", encoding="utf-8") as fh:
                w = csv.DictWriter(fh, fieldnames=list(cols))
                w.writeheader()
                w.writerows(rows)

    for slug, n in per_sub.most_common():
        print("  %-18s %d" % (slug, n))
    print("%s %d signatures from the RET operand"
          % ("wrote" if args.write else "would write", total))
    if not args.write:
        print("(re-run with --write to apply)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
