#!/usr/bin/env python3
"""Recover cdecl signatures from call-site evidence (#453).

#452 materialized the 816 signatures the FA.SMS decoration *proves*. The remaining 1055 func
rows encode nothing in their name -- 380 cdecl names with no arity (`_CTVarDiff`) and 675
names we coined ourselves (`CTLoadProgram`) -- so their signature has to come from the code.

    python3 tools/recover_signatures.py --write       # fill the type column
    python3 tools/recover_signatures.py --check       # currency + re-validation
    python3 tools/recover_signatures.py --self-test   # unit tests, no db/ needed

THE ONE RULE, AND WHY IT IS THE ONLY ONE
----------------------------------------
A cdecl caller pops its own arguments right after the CALL (`ADD ESP, N`, or `POP ECX` for a
single dword). Two things make that byte count trustworthy where nothing else was:

  1. Observing caller cleanup PROVES the convention is caller-cleans, i.e. cdecl. A stdcall
     or fastcall callee cleans up itself; its callers never would.
  2. cdecl passes NO arguments in registers. So the cleanup is the FULL arity -- there is
     nothing hidden that we could be undercounting.

Validated against the functions whose true arity the C++ mangling already gives us:
**75 correct, 0 wrong.**

Every other route was measured and REJECTED, because each one has to guess whether arguments
arrived in ECX/EDX, and none can:

  | approach                                   | error rate |
  |--------------------------------------------|------------|
  | caller-side ECX/EDX, unanimous across sites | 7.6%  (undercounts -- the value was already in ECX, so no load is emitted) |
  | caller-side ECX/EDX, max across sites       | 15%   (overcounts -- EDX is a common scratch register) |
  | callee-side "reads ECX before writing it"   | 11.6% |
  | Ghidra's own decompiler parameter list      | ~16%  (vs the `ret N` the instruction actually carries) |

A `ret N` in the callee looks like proof but is not: it pins the N *stack* bytes, while a
fastcall callee may take 1-2 further arguments in registers on top of them. So it cannot
settle an arity either, unless the convention is already known.

Writing any of those into db/ would corrupt whatever the fxe generator emits downstream, at
precisely the rate that is hardest to notice. #451's rule stands: a wrong datatype is worse
than none. The rest of #453 is the per-subsystem, docs-corroborated grind the issue describes.

Types stay `undefined4`: the cleanup proves how many arguments there are, never what they
are. Sharpening them is expected and allowed (see gen_signatures.py).
"""

import argparse
import csv
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from fa_demangle import demangle_cpp  # noqa: E402

REPO = pathlib.Path(__file__).resolve().parent.parent
DB = REPO / "db"
HEADER = ["va", "kind", "name", "display", "source", "confidence", "notes", "type"]

# A cleanup larger than this is a stack-frame teardown that happens to follow the CALL, not
# argument cleanup. (Unguarded, one such site claimed a function took 889 arguments.)
MAX_CLEANUP_BYTES = 64


def load_manifest(db=DB):
    """slug -> owning binary, from db/subsystems.csv."""
    out = {}
    with open(db / "subsystems.csv", newline="") as fh:
        for r in csv.DictReader(fh):
            out[r["slug"]] = r["binary"]
    return out


def load_callsites(db=DB):
    """binary -> {callee_va: [cleanup_bytes]}.

    Keyed BY BINARY, never by VA alone: a VA is unique only within a binary (IP.EXE bases at
    the same 0x00400000 as FA.EXE), so a flat VA map would attribute one binary's call sites
    to another's functions.

    Returns None when no export exists -- the inventory is a local-only Ghidra export (#342),
    so its absence downgrades the checks rather than failing them, exactly as coverage does.
    """
    inv = db / "inventory"
    if not inv.is_dir():
        return None
    sites = {}
    for path in sorted(inv.glob("*/callsites.csv")):
        binary = path.parent.name
        per = sites.setdefault(binary, {})
        with open(path, newline="") as fh:
            for r in csv.DictReader(fh):
                per.setdefault(int(r["callee_va"], 16), []).append(int(r["cleanup_bytes"]))
    return sites or None


def arity_from_callsites(cleanups, convention_known):
    """The cdecl arity every informative call site agrees on, or None.

    None is the common answer and the correct one: a callee with no informative site, with
    sites that disagree (MSVC merged the cleanup of consecutive calls), or with an
    implausible cleanup, gets no signature at all.

    A caller that cleans up NOTHING is *not* evidence of a zero-argument function: a fastcall
    callee taking its arguments in ECX/EDX looks identical. That mistake would have hit 144 of
    178 known fastcall functions, so absence of cleanup is treated as silence.

    `convention_known` says whether the NAME already proves the function is cdecl, and it
    decides how much corroboration the evidence needs -- the strictness scales with what is
    actually being inferred:

      * Convention proven (a `_name` decoration): we are only reading the ARITY off the
        cleanup. A cdecl function's caller cleanup *is* its argument list, so one unanimous
        site is enough. Measured on the functions whose arity the C++ mangling gives us
        independently: 75 correct, 0 wrong.

      * Convention NOT proven (a name we coined): the cleanup has to establish the convention
        too, and a lone `ADD ESP` after a CALL can be something else entirely -- a stack-frame
        teardown, or a discarded temporary. Unguarded, that misread 8 known callee-cleans
        functions as cdecl (@PutCurObj@0 shows cleanup at 1 of its 108 call sites). So it must
        be corroborated: at least 2 call sites, and cleanup visible at a majority of them.
        That gate admits 0 false positives across all 673 known callee-cleans functions.
    """
    informative = [c for c in cleanups if 0 <= c <= MAX_CLEANUP_BYTES and c % 4 == 0]
    if not informative:
        return None
    if len(set(informative)) != 1:
        return None          # sites disagree -> a finding, not something to average
    if not convention_known:
        if len(cleanups) < 2 or len(informative) / len(cleanups) < 0.5:
            return None
    return informative[0] // 4


def name_convention(name):
    """What the MSVC decoration proves about the convention: 'cdecl', 'callee', or None.

    `_name@N` is stdcall and `@name@N` is fastcall -- both callee-cleans, both already signed
    by #452. `_name` with no `@N` is cdecl. A name we coined ourselves proves nothing.
    """
    if re.match(r"^[_@][A-Za-z_]\w*@\d+$", name) or name.startswith("?"):
        return "callee" if not _mangled_cdecl(name) else "cdecl"
    if re.match(r"^_[A-Za-z_]\w*$", name):
        return "cdecl"
    return None


def _mangled_cdecl(name):
    if not name.startswith("?"):
        return False
    g = demangle_cpp(name)
    return bool(g) and g[1] == "__cdecl"


def proto_name(name):
    """The identifier to spell in the prototype: undecorate a `_name` cdecl symbol."""
    return name[1:] if name.startswith("_") and len(name) > 1 else name


def build(name, arity):
    args = ", ".join(["undefined4"] * arity) if arity else "void"
    return "undefined4 __cdecl %s(%s)" % (proto_name(name), args)


def process(write, sites):
    stats = {"recovered": 0, "filled": 0, "already": 0, "no_evidence": 0}
    errs = []
    manifest = load_manifest()
    for path in sorted((DB / "symbols").glob("*.csv")):
        rel = path.relative_to(REPO)
        per_binary = sites.get(manifest.get(path.stem), {})
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
            if kind != "func" or source == "waiver":
                continue

            conv = name_convention(name)
            if conv == "callee":
                # The decoration already PROVES a callee-cleans convention and #452 signed it
                # from that proof. Call-site cleanup is the weaker witness here -- it misreads
                # such functions 1.2% of the time -- so it does not get a vote.
                continue

            arity = arity_from_callsites(per_binary.get(int(va, 16), []),
                                         convention_known=(conv == "cdecl"))
            if arity is None:
                if not ctype.strip():
                    stats["no_evidence"] += 1
                continue
            stats["recovered"] += 1
            proto = build(name, arity)

            if not ctype.strip():
                if write:
                    r[7] = proto
                    dirty = True
                    stats["filled"] += 1
                else:
                    errs.append("%s:%d: %s -- call sites prove a %d-argument cdecl signature "
                                "but the type column is empty; run "
                                "`python3 tools/recover_signatures.py --write`"
                                % (rel, i, name, arity))
                continue

            # Already signed -- by #452's decoration pass, or sharpened by hand. The call
            # sites are still ground truth about the ARITY, so they must not disagree.
            stored = ctype.strip()
            got = _arity_of(stored)
            if got is not None and got != arity:
                errs.append("%s:%d: %s -- stored signature takes %d argument(s) but its call "
                            "sites clean %d dword(s) off the stack"
                            % (rel, i, name, got, arity))
            else:
                stats["already"] += 1
        if write and dirty:
            with open(path, "w", newline="") as fh:
                w = csv.writer(fh, lineterminator="\n")
                w.writerow(header)
                w.writerows(body)
    return stats, errs


def _arity_of(proto):
    """Argument count of a stored prototype, or None. Paren-aware: a function-pointer
    parameter carries commas of its own."""
    i, j = proto.find("("), proto.rfind(")")
    if i < 0 or j < i:
        return None
    body, depth, n = proto[i + 1:j], 0, 0
    if not body.strip() or body.strip() == "void":
        return 0
    for ch in body:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        elif ch == "," and depth == 0:
            n += 1
    return n + 1


def revalidate(sites):
    """Re-prove the rule against the functions whose true arity the mangling already gives.

    This is the check that keeps the export honest: if a Ghidra upgrade or an analysis change
    ever perturbs the call-site evidence, the rule's accuracy must be re-demonstrated, not
    assumed. A single disagreement here means the evidence is no longer trustworthy and the
    signatures derived from it are suspect.
    """
    ok = bad = 0
    fails = []
    manifest = load_manifest()
    for path in sorted((DB / "symbols").glob("*.csv")):
        per_binary = sites.get(manifest.get(path.stem), {})
        with open(path, newline="") as fh:
            for r in csv.DictReader(fh):
                if r["kind"] != "func" or not r["name"].startswith("?"):
                    continue
                g = demangle_cpp(r["name"])
                if not g or g[1] != "__cdecl":
                    continue
                arity = arity_from_callsites(per_binary.get(int(r["va"], 16), []),
                                             convention_known=True)
                if arity is None:
                    continue
                if arity == len(g[3]):
                    ok += 1
                else:
                    bad += 1
                    fails.append("%s: mangling proves %d argument(s), call sites say %d"
                                 % (g[0], len(g[3]), arity))
    return ok, bad, fails


# --- self-test ---------------------------------------------------------------------
def self_test():
    fails = []
    # convention_known=True: the name proves cdecl, so we are only reading the arity.
    cases = [
        ([8, 8, 8], True, 2, "unanimous cleanup -> arity"),
        ([4], True, 1, "one unanimous site is enough when the convention is proven"),
        ([4, 8], True, None, "sites disagree (merged cleanup) -> refuse"),
        ([-1, -1], True, None, "no cleanup is NOT proof of 0 args (fastcall looks identical)"),
        ([], True, None, "no call sites -> refuse"),
        ([3556], True, None, "a frame teardown is not argument cleanup -> refuse"),
        ([6], True, None, "a cleanup that is not a whole number of dwords -> refuse"),
        ([-1, 8, 8], True, 2, "uninformative sites are ignored, not treated as 0"),
        # convention_known=False: the cleanup must establish the convention too, so it needs
        # corroboration -- this is the gate that stopped @PutCurObj@0 (cleanup at 1 of 108
        # sites) from being misread as cdecl.
        ([8], False, None, "a lone site cannot also establish the convention"),
        ([8] + [-1] * 107, False, None, "cleanup at 1 of 108 sites is noise, not a signature"),
        ([8, 8], False, 2, "two corroborating sites, all informative -> accept"),
        ([8, 8, -1, -1], False, 2, "majority informative -> accept"),
        ([8, -1, -1, -1], False, None, "minority informative -> refuse"),
    ]
    for cleanups, known, want, label in cases:
        got = arity_from_callsites(cleanups, convention_known=known)
        if got != want:
            fails.append("  %s (known=%s): want %r, got %r  (%s)"
                         % (cleanups, known, want, got, label))

    if build("_CTVarDiff", 2) != "undefined4 __cdecl CTVarDiff(undefined4, undefined4)":
        fails.append("  build: cdecl prototype/undecoration wrong")
    if build("CTLoadProgram", 0) != "undefined4 __cdecl CTLoadProgram(void)":
        fails.append("  build: zero-arg prototype must read (void)")
    if _arity_of("int __cdecl f(int (__stdcall *)(int, char), long)") != 2:
        fails.append("  _arity_of: a function-pointer argument inflated the count")
    if _arity_of("void __cdecl f(void)") != 0:
        fails.append("  _arity_of: (void) must read as arity 0")

    for nm, want in [("_CTVarDiff", "cdecl"), ("_SetupOT@4", "callee"),
                     ("@FMGear@4", "callee"), ("CTLoadProgram", None),
                     ("?IntersectT@@YAJPAUF24_POINT@@JJ@Z", "cdecl"),
                     ("?COLDrawInfo@@YGXGF@Z", "callee")]:
        if name_convention(nm) != want:
            fails.append("  name_convention(%s): want %r, got %r"
                         % (nm, want, name_convention(nm)))

    if fails:
        print("recover_signatures self-test FAILED:\n" + "\n".join(fails))
        return 1
    print("recover_signatures self-test: %d cases OK" % (len(cases) + 4))
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

    sites = load_callsites()
    if sites is None:
        print("recover_signatures: no db/inventory/*/callsites.csv — call-site evidence is a "
              "local-only Ghidra export (#342); skipping (export with "
              "scripts/ghidra/export_inventory.sh)")
        return 0

    ok, bad, vfails = revalidate(sites)
    for f in vfails:
        print("error: rule re-validation failed: %s" % f)

    stats, errs = process(write=args.write, sites=sites)
    for e in errs:
        print("error: %s" % e)

    print("recover_signatures: rule re-validated on %d known-arity cdecl functions "
          "(%d disagreements)" % (ok + bad, bad))
    if args.write:
        print("                    %d recovered from call sites, %d filled, %d already current"
              % (stats["recovered"], stats["filled"], stats["already"]))
        print("                    %d func rows have no usable call-site evidence (left to the "
              "per-subsystem pass)" % stats["no_evidence"])
    else:
        print("recover_signatures --check: %d recovered, %d current, %d errors"
              % (stats["recovered"], stats["already"], len(errs) + bad))
    return 1 if (errs or bad) else 0


if __name__ == "__main__":
    sys.exit(main())
