#!/usr/bin/env python3
"""Real-media install harness for the retail FA discs (FX_FA_DISC1/FX_FA_DISC2 mode).

Where fa_manifest.py proves the extraction pipeline against an *installed* tree,
this proves the pipeline that *produces* one: the ESA installer archive and the
`fx install` engine, driven against the user's own discs. Hashes are facts about
the media; the media itself never enters the repository.

Six checks, cheapest first:

  plan      `fx install plan --json` for both scripts. The media fingerprints as
            1.00F, the two scripts differ by exactly FA_4B.LIB, the CD-resident
            LIBs are the loose ones the ESA does not supply, the two
            INSTALL_SYSFILES entries are recorded-not-written, and the plan is
            identical whichever order the discs are named in (disc identity is
            content-based, never the volume label -- the labels are in fact
            *swapped* on a Linux mount, which is what makes this worth asserting).
  self      L1 self-oracle: the four ESA entries that also sit loose on disc 1
            (README.TXT, IP.EXE, IP.CFG, EAHELP.HLP) must extract byte-identical
            to those loose copies. Three are PKWA, so this proves the decoder
            against the disc alone -- no committed hash involved.
  manifest  sha256 of every extracted ESA entry vs the committed fa-esa.sha256.
  repack    L4: `fx esa repack` reproduces the 110 MB archive byte-for-byte.
  install   e2e: a minimal, no-CD-resident `install run` (19 files, 73 MiB) into
            the build tree, `install verify`d against the disc, then the
            SKIP_ON_REMOVE clobber guard is exercised -- EXAMPLE.MT survives even
            `--overwrite`, while an unguarded file is restored by it.
  cross     L5 cross-build oracle, only when --fa-root names a 1.02F install: the
            files a fresh 1.00F install writes must differ from that install in
            exactly the set the 1.02F patch rewrites, and no other. This is the
            executable statement of the gap the RTPatch codec closes; when it
            lands, the expected-difference set here goes empty.

The full 989 MiB install (CD-resident LIBs included) is planned but not executed
unless --full is passed; a plan is pure, so its byte total is checked for free.
Stdlib-only, like the rest of tests/integration.
"""

import argparse
import filecmp
import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

# --- Facts about the 1.00F retail media (both discs). ------------------------

MEDIA_BUILD = "1.00F"
ESA_ENTRY_COUNT = 23

# The one entry the full script adds over the minimal one.
SCRIPT_DELTA = "FA_4B.LIB"
# Loose on a disc root, not supplied by the ESA: CD-resident by rule, not by list.
CD_RESIDENT = {"FA_3.LIB", "FA_4C.LIB", "FA_7.LIB", "FA_10.LIB", "FA_10B.LIB",
               "FA_11.LIB", "FA_11B.LIB"}
# INSTALL_SYSFILES: bound for the Windows system directory. Recorded, never written.
SYSFILES = {"EAREMOVE.EXE", "EAEXEC.EXE"}
# SKIP_ON_REMOVE: shipped by the script *and* guarded against clobber. The sharp case.
GUARDED = "EXAMPLE.MT"

MINIMAL = {"script": "minstall.ssf", "copy": 19, "bytes": 76_517_762, "unhonored": 10}
FULL = {"script": "finstall.ssf", "copy": 27, "bytes": 1_036_798_285, "unhonored": 10}

# Entries that are also loose on disc 1 -- the self-oracle set (L1).
SELF_ORACLE = ["README.TXT", "IP.EXE", "IP.CFG", "EAHELP.HLP"]

# What the 1.00F -> 1.02F patch rewrites. Everything else a minimal install writes
# must already be byte-identical in a 1.02F tree. (msapi.dll is *delivered* by the
# patch, so it is on no disc and cannot appear here.) Emptied when RTPatch lands.
PATCH_REWRITES = {"FA.EXE", "FA.SMS", "FA_1.LIB", "FA_2.LIB"}
# Shipped by the installer but absent from a real 1.02F install: nothing to compare.
NOT_INSTALLED = {"README.TXT"}

SANITIZED = set('&*?"<>|/\\:')

failures: list[str] = []


def check(ok: bool, msg: str) -> bool:
    print(f"  {'ok  ' if ok else 'FAIL'}  {msg}")
    if not ok:
        failures.append(msg)
    return ok


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def safe_name(name: str) -> str:
    """Mirror of fx::esa_safe_name -- the on-disk name `fx esa unpack` writes."""
    return "".join("_" if c in SANITIZED else c for c in name)


def run_fx(fx: str, *args: str, want: int = 0) -> subprocess.CompletedProcess:
    proc = subprocess.run([fx, *args], capture_output=True, text=True)
    if want is not None and proc.returncode != want:
        sys.exit(f"fx {' '.join(args)} exited {proc.returncode} (wanted {want}):\n"
                 f"{proc.stdout}\n{proc.stderr}")
    return proc


def find(root: Path, name: str) -> Path | None:
    """Case-insensitive lookup: the mount decides the case, not us."""
    for p in root.iterdir():
        if p.is_file() and p.name.upper() == name.upper():
            return p
    return None


def plan(fx: str, discs: list[Path], *flags: str) -> dict:
    """`fx install plan --json`. Under --json, stdout is the plan and nothing else."""
    proc = run_fx(fx, "install", "plan", *[str(d) for d in discs], "--json", *flags)
    return json.loads(proc.stdout)


# --- plan --------------------------------------------------------------------

def check_plan(fx: str, discs: list[Path]) -> dict:
    print("plan:")
    minimal = plan(fx, discs, "--minimal", "--no-cd-resident")
    full = plan(fx, discs, "--full")
    reversed_ = plan(fx, list(reversed(discs)), "--minimal", "--no-cd-resident")

    check(minimal["build"] == MEDIA_BUILD, f"media fingerprints as {MEDIA_BUILD}")

    for name, p, want in (("minimal", minimal, MINIMAL), ("full", full, FULL)):
        copy = [i for i in p["items"] if i["status"] == "copy"]
        skip = {i["dest"] for i in p["items"] if i["status"] == "skip"}
        unhonored = sum(1 for d in p["directives"] if not d["honored"])
        check(p["script"] == want["script"], f"{name}: script is {want['script']}")
        check(len(copy) == want["copy"], f"{name}: {want['copy']} files to copy")
        check(p["bytes"] == want["bytes"], f"{name}: {want['bytes']:,} bytes")
        check(skip == SYSFILES, f"{name}: sysfiles recorded, not written: {sorted(SYSFILES)}")
        check(unhonored == want["unhonored"],
              f"{name}: {want['unhonored']} directives reported unhonored")

    mins = {i["dest"] for i in minimal["items"]}
    fulls = {i["dest"] for i in full["items"]}
    check(fulls - mins == CD_RESIDENT | {SCRIPT_DELTA},
          f"full adds exactly {SCRIPT_DELTA} + the {len(CD_RESIDENT)} CD-resident LIBs")
    loose = {i["dest"] for i in full["items"] if i["origin"] == "loose"}
    check(loose == CD_RESIDENT, "CD-resident set == the loose LIBs the ESA does not supply")

    key = lambda p: sorted((i["dest"], i["status"], i["bytes"]) for i in p["items"])
    check(key(reversed_) == key(minimal) and reversed_["bytes"] == minimal["bytes"],
          "plan is identical with the discs named in either order")
    return minimal


# --- self-oracle, manifest, repack -------------------------------------------

def check_self_oracle(fx: str, esa: Path, disc1: Path, work: Path) -> None:
    print("self-oracle (ESA entry vs the same file loose on disc 1):")
    out = work / "esa-self"
    shutil.rmtree(out, ignore_errors=True)
    run_fx(fx, "esa", "extract", str(esa), *SELF_ORACLE, "-o", str(out))
    for name in SELF_ORACLE:
        got, want = out / safe_name(name), find(disc1, name)
        if not check(got.is_file(), f"{name}: extracted"):
            continue
        if want is None:
            check(False, f"{name}: no loose copy on disc 1 to compare against")
            continue
        check(filecmp.cmp(got, want, shallow=False), f"{name}: byte-identical to the loose copy")
    shutil.rmtree(out, ignore_errors=True)


def esa_manifest(fx: str, esa: Path, work: Path) -> dict[str, tuple[int, str]]:
    names = [line.split("  ")[0].strip()
             for line in run_fx(fx, "esa", "ls", str(esa)).stdout.splitlines()[1:]
             if line.strip() and not line.startswith(("Name", "-")) and "file(s)" not in line]
    out = work / "esa-unpack"
    shutil.rmtree(out, ignore_errors=True)
    run_fx(fx, "esa", "unpack", str(esa), "-o", str(out))
    entries = {}
    for name in names:
        p = out / safe_name(name)
        if not p.is_file():
            sys.exit(f"esa unpack wrote nothing for {name}")
        entries[name] = (p.stat().st_size, sha256_file(p))
    shutil.rmtree(out, ignore_errors=True)
    return entries


def check_repack(fx: str, esa: Path, work: Path) -> None:
    print("repack:")
    out = work / "repacked.esa"
    proc = run_fx(fx, "esa", "repack", str(esa), str(out))
    check("byte-identical" in proc.stdout and sha256_file(out) == sha256_file(esa),
          f"esa repack reproduces {esa.name} byte-for-byte ({esa.stat().st_size:,} bytes)")
    out.unlink(missing_ok=True)


# --- install e2e -------------------------------------------------------------

def check_install(fx: str, discs: list[Path], minimal: dict, dest: Path) -> None:
    print("install (minimal, no CD-resident):")
    shutil.rmtree(dest, ignore_errors=True)
    dest.mkdir(parents=True)
    args = [*[str(d) for d in discs], "-d", str(dest), "--minimal", "--no-cd-resident"]
    run_fx(fx, "install", "run", *args, "--verify")
    check(True, "install run --verify: every installed byte matches the disc")

    want = {i["dest"]: i["bytes"] for i in minimal["items"] if i["status"] == "copy"}
    on_disk = {p.name: p.stat().st_size for p in dest.iterdir() if p.is_file()}
    check(on_disk == want, f"{len(want)} files written, every size as planned")

    run_fx(fx, "install", "verify", *args)
    check(True, "install verify (planned against an empty tree) passes on its own")


def check_install_patch(fx: str, discs: list[Path], patch: Path, manifest: Path,
                        dest: Path) -> None:
    """The chained pipeline: install 1.00F from the discs, then `--patch` up to
    1.02F, and check the four rebuilt game files against the committed 1.02F
    hashes. This is the cross-build oracle made byte-exact — where check_cross_build
    only asserts *which* files a 1.00F install differs from a 1.02F tree in, this
    proves the patch actually produces that 1.02F tree."""
    print("install + patch (1.00F -> 1.02F):")
    expected = {}
    with open(manifest) as f:
        for line in f:
            digest, name = line.rstrip("\n").split("  ", 1)
            expected[name] = digest
    shutil.rmtree(dest, ignore_errors=True)
    dest.mkdir(parents=True)
    run_fx(fx, "install", "run", *[str(d) for d in discs], "-d", str(dest),
           "--minimal", "--no-cd-resident", "--patch", str(patch))
    for name, digest in expected.items():
        p = dest / name
        if not check(p.is_file(), f"{name}: present after patch"):
            continue
        check(sha256_file(p) == digest, f"{name}: reconstructed to the 1.02F build")
    shutil.rmtree(dest, ignore_errors=True)


def check_clobber_guard(fx: str, discs: list[Path], dest: Path) -> None:
    """SKIP_ON_REMOVE is a clobber guard: EXAMPLE.MT is shipped by the script *and*
    never overwritten, not even by --overwrite. An unguarded file is.

    This leaves GUARDED corrupted by construction -- the guard is what stops the
    reinstall from repairing it -- so it runs last, after anything that reads the
    installed tree.
    """
    print("clobber guard:")
    args = [*[str(d) for d in discs], "-d", str(dest), "--minimal", "--no-cd-resident"]
    junk = b"clobbered by fa_disc.py\n"
    for name in (GUARDED, "CHAT.TXT"):
        (dest / name).write_bytes(junk)
    run_fx(fx, "install", "run", *args, "--overwrite")
    check((dest / GUARDED).read_bytes() == junk,
          f"{GUARDED} (SKIP_ON_REMOVE) survives --overwrite")
    check((dest / "CHAT.TXT").read_bytes() != junk,
          "CHAT.TXT (unguarded) is restored by --overwrite")


# --- L5 cross-build oracle ---------------------------------------------------

def check_cross_build(dest: Path, fa_root: Path) -> None:
    print(f"cross-build oracle (1.00F install vs the {fa_root.name} tree):")
    differ, absent, same = set(), set(), set()
    for p in sorted(dest.iterdir()):
        if not p.is_file():
            continue
        theirs = find(fa_root, p.name)
        if theirs is None:
            absent.add(p.name)
        elif filecmp.cmp(p, theirs, shallow=False):
            same.add(p.name)
        else:
            differ.add(p.name)

    check(differ == PATCH_REWRITES,
          f"exactly the patch's file set differs: {sorted(PATCH_REWRITES)}")
    check(absent == NOT_INSTALLED,
          f"shipped but not present in a real install: {sorted(NOT_INSTALLED)}")
    check(len(same) == MINIMAL["copy"] - len(PATCH_REWRITES) - len(NOT_INSTALLED),
          f"the remaining {len(same)} files are byte-identical across builds")
    if differ - PATCH_REWRITES:
        print(f"        unexpected differences: {sorted(differ - PATCH_REWRITES)}")
    if PATCH_REWRITES - differ:
        print(f"        expected to differ but did not: {sorted(PATCH_REWRITES - differ)}")


# --- main --------------------------------------------------------------------

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("mode", choices=["generate", "verify"])
    ap.add_argument("--fx", required=True, help="path to the fx executable")
    ap.add_argument("--disc1", required=True, help="FA disc 1 (mounted, or a copy of the root)")
    ap.add_argument("--disc2", required=True, help="FA disc 2")
    ap.add_argument("--fa-root", help="a 1.02F install; enables the cross-build oracle")
    ap.add_argument("--patch", help="the 1.02F updater (fae102.exe); enables the install+patch check")
    ap.add_argument("--patch-manifest", help="committed 1.02F hash manifest (fa-patch.sha256)")
    ap.add_argument("--manifest", help="ESA manifest to compare against (verify)")
    ap.add_argument("--out", help="ESA manifest to write (generate)")
    ap.add_argument("--full", action="store_true",
                    help="also execute the full 989 MiB install (plan-checked either way)")
    ap.add_argument("--work-dir", required=True,
                    help="scratch directory (under the build tree, not /tmp)")
    args = ap.parse_args()

    discs = [Path(args.disc1), Path(args.disc2)]
    for d in discs:
        if not d.is_dir():
            sys.exit(f"not a directory: {d}")
    work = Path(args.work_dir)
    work.mkdir(parents=True, exist_ok=True)

    # Disc 1 is the one holding the installer archive -- content, not label.
    disc1 = next((d for d in discs if find(d, "SETUP.ESA")), None)
    if disc1 is None:
        sys.exit(f"neither disc holds a SETUP.ESA: {discs}")
    esa = find(disc1, "SETUP.ESA")
    print(f"disc 1: {disc1}\ndisc 2: {[d for d in discs if d != disc1][0]}\n")

    entries = esa_manifest(args.fx, esa, work)
    if args.mode == "generate":
        if not args.out:
            sys.exit("generate requires --out")
        with open(args.out, "w", newline="\n") as f:
            for name in sorted(entries):
                size, digest = entries[name]
                f.write(f"{digest}  {size}  {name}\n")
        print(f"wrote {args.out} ({len(entries)} entries)")
        return 0

    if not args.manifest:
        sys.exit("verify requires --manifest")

    minimal = check_plan(args.fx, discs)

    print("manifest:")
    check(len(entries) == ESA_ENTRY_COUNT, f"{ESA_ENTRY_COUNT} entries in the archive")
    expected = {}
    with open(args.manifest) as f:
        for line in f:
            digest, size, name = line.rstrip("\n").split("  ", 2)
            expected[name] = (int(size), digest)
    missing = sorted(expected.keys() - entries.keys())
    extra = sorted(entries.keys() - expected.keys())
    bad = sorted(k for k in expected.keys() & entries.keys() if expected[k] != entries[k])
    for k in missing:
        print(f"        MISSING  {k}")
    for k in extra:
        print(f"        EXTRA    {k}")
    for k in bad:
        print(f"        MISMATCH {k}: expected {expected[k]}, got {entries[k]}")
    check(not (missing or extra or bad),
          f"all {len(expected)} extracted entries match the committed manifest")

    check_self_oracle(args.fx, esa, disc1, work)
    check_repack(args.fx, esa, work)

    dest = work / "install"
    check_install(args.fx, discs, minimal, dest)

    if args.full:
        print("install (full, CD-resident included):")
        full_dest = work / "install-full"
        shutil.rmtree(full_dest, ignore_errors=True)
        full_dest.mkdir(parents=True)
        run_fx(args.fx, "install", "run", *[str(d) for d in discs],
               "-d", str(full_dest), "--full", "--verify")
        written = sum(p.stat().st_size for p in full_dest.iterdir() if p.is_file())
        check(written == FULL["bytes"], f"{FULL['bytes']:,} bytes written and verified")
        shutil.rmtree(full_dest, ignore_errors=True)

    if args.fa_root:
        fa_root = Path(args.fa_root)
        if not fa_root.is_dir():
            sys.exit(f"--fa-root is not a directory: {fa_root}")
        check_cross_build(dest, fa_root)

    if args.patch:
        patch, pman = Path(args.patch), Path(args.patch_manifest)
        if not patch.is_file():
            sys.exit(f"--patch is not a file: {patch}")
        if not pman.is_file():
            sys.exit(f"--patch-manifest is not a file: {pman}")
        check_install_patch(args.fx, discs, patch, pman, work / "install-patch")

    check_clobber_guard(args.fx, discs, dest)  # corrupts the tree; must come last
    shutil.rmtree(dest, ignore_errors=True)

    print()
    if failures:
        print(f"FAIL: {len(failures)} check(s) failed")
        for f in failures:
            print(f"  - {f}")
        return 1
    print("OK: the discs install, verify, and round-trip as documented")
    return 0


if __name__ == "__main__":
    sys.exit(main())
