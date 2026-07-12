#!/usr/bin/env python3
"""Real-media RTPatch pipeline: apply fae102.exe to the 1.00F originals and verify
the reconstructed 1.02F game files (FX_FA_PATCH mode).

The 1.02F updater `fae102.exe` carries a Pocket Soft .RTPatch payload. This drives
`fx patch apply` over the four modified game files, sourcing the 1.00F originals
from Disc 1's SETUP.ESA, and checks each reconstructed file's SHA-256 against the
committed manifest. Hashes are facts about the 1.02F build; neither the patcher
nor the game bytes enter the repository.

Note on FA.EXE: the committed hash is the **official** 1.02F executable that the
patch produces. A licensed install's FA.EXE may differ by a byte or two if it
carries a no-CD crack (a JNZ→JZ flip in the CD check) — that is a property of the
installed copy, not of the patch, so this test validates against the pristine
patch output, not any particular install.

Stdlib-only, like the rest of tests/integration.
"""

import argparse
import hashlib
import shutil
import subprocess
import sys
from pathlib import Path

# The four modified game files plus the added msapi.dll (delivered whole). Each
# reconstructs to a 1.02F build that matches a licensed install byte-for-byte —
# except the official FA.EXE (see the module docstring). README.TXT, ealtest.exe,
# and the system-directory EAEXEC.EXE also reconstruct but have no independent
# 1.02F oracle here, so they are not pinned.
FILES = ["FA.EXE", "FA.SMS", "FA_1.LIB", "FA_2.LIB", "msapi.dll"]
# The 1.00F originals the modify records patch against (msapi.dll is added whole,
# so it has no source and is not extracted).
SOURCE_FILES = ["FA.EXE", "FA.SMS", "FA_1.LIB", "FA_2.LIB"]


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def run_fx(fx: str, *args: str) -> subprocess.CompletedProcess:
    proc = subprocess.run([fx, *args], capture_output=True, text=True)
    if proc.returncode not in (0, 1):
        sys.exit(f"fx {' '.join(args)} exited {proc.returncode}:\n{proc.stdout}\n{proc.stderr}")
    return proc


def find(root: Path, name: str) -> Path | None:
    for p in root.iterdir():
        if p.is_file() and p.name.upper() == name.upper():
            return p
    return None


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("mode", choices=["generate", "verify"])
    ap.add_argument("--fx", required=True)
    ap.add_argument("--patch", required=True, help="fae102.exe (the 1.02F updater)")
    ap.add_argument("--disc1", help="FA Disc 1 (holds SETUP.ESA with the 1.00F originals)")
    ap.add_argument("--source", help="a directory of 1.00F originals (instead of --disc1)")
    ap.add_argument("--manifest", help="committed hash manifest (verify)")
    ap.add_argument("--out", help="manifest to write (generate)")
    ap.add_argument("--work-dir", required=True)
    args = ap.parse_args()

    work = Path(args.work_dir)
    work.mkdir(parents=True, exist_ok=True)

    # Source the 1.00F originals: a given dir, or extracted from the disc's ESA.
    src = work / "src"
    shutil.rmtree(src, ignore_errors=True)
    src.mkdir()
    if args.source:
        for name in SOURCE_FILES:
            p = find(Path(args.source), name)
            if not p:
                sys.exit(f"source is missing {name}")
            shutil.copy(p, src / name)
    else:
        if not args.disc1:
            sys.exit("need --disc1 or --source for the 1.00F originals")
        esa = find(Path(args.disc1), "SETUP.ESA")
        if not esa:
            sys.exit(f"no SETUP.ESA on {args.disc1}")
        run_fx(args.fx, "esa", "extract", str(esa), *SOURCE_FILES, "-o", str(src))

    # Apply the patch. The ESA originals pass the §10 source checksum, so run the
    # checksummed path (no --no-checksum) — that also exercises source validation.
    out = work / "out"
    shutil.rmtree(out, ignore_errors=True)
    proc = run_fx(args.fx, "patch", "apply", args.patch,
                  "--source", str(src), "--out", str(out))
    print(proc.stdout.rstrip())

    digests = {}
    for name in FILES:
        p = out / name
        if not p.is_file():
            sys.exit(f"patch did not produce {name}")
        digests[name] = sha256(p)

    if args.mode == "generate":
        if not args.out:
            sys.exit("generate requires --out")
        with open(args.out, "w", newline="\n") as f:
            for name in FILES:
                f.write(f"{digests[name]}  {name}\n")
        print(f"wrote {args.out}")
        return 0

    if not args.manifest:
        sys.exit("verify requires --manifest")
    expected = {}
    with open(args.manifest) as f:
        for line in f:
            d, name = line.rstrip("\n").split("  ", 1)
            expected[name] = d
    bad = [n for n in FILES if digests[n] != expected.get(n)]
    for n in bad:
        print(f"MISMATCH {n}: expected {expected.get(n)}, got {digests[n]}")
    shutil.rmtree(src, ignore_errors=True)
    shutil.rmtree(out, ignore_errors=True)
    if bad:
        print(f"FAIL: {len(bad)} of {len(FILES)} files differ")
        return 1
    print(f"OK: all {len(FILES)} files reconstruct to the 1.02F build byte-for-byte")
    return 0


if __name__ == "__main__":
    sys.exit(main())
