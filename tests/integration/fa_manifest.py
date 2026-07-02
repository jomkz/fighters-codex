#!/usr/bin/env python3
"""Real-asset extraction manifest for a licensed FA install (FX_FA_ROOT mode).

Runs `fx lib unpack` over every .LIB in the FA root, hashes every extracted
file, and either writes a manifest (generate) or compares against a committed
one (verify). A manifest generated on one OS and verified on another proves
the extraction pipeline is byte-identical across platforms — the Phase 1 exit
criterion (epic #42). Hashes are facts about the game data; the data itself
never enters the repository.

Also covered per run:
- an `fx lib extract` spot-check with a case-folded entry name, byte-compared
  against the unpack output (extract and unpack share one data path);
- `fx sms dump FA.SMS -o ...` when FA.SMS is present — the only runtime text
  artifact, guarding the LF-everywhere CSV contract.

Stdlib-only, like the rest of scripts/. Each lib's unpack directory is
deleted before the next one, so peak disk stays at one decompressed archive.
`fx lib unpack` exits 1 when entries with unimplemented flags (lzss/pxpk) are
skipped; skips are data-driven and platform-identical, so a nonzero exit is
logged but not fatal — the manifest of written files is the ground truth.
Duplicate entry names within one archive overwrite deterministically
(last wins) on every platform.
"""

import argparse
import hashlib
import shutil
import subprocess
import sys
from pathlib import Path

SANITIZED = set('&*?"<>|/\\:')


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def run_fx(fx: str, *args: str) -> subprocess.CompletedProcess:
    proc = subprocess.run([fx, *args], capture_output=True, text=True)
    if proc.returncode not in (0, 1):
        sys.exit(f"fx {' '.join(args)} exited {proc.returncode}:\n{proc.stderr}")
    return proc


def discover_libs(fa_root: Path) -> list[Path]:
    libs = [p for p in fa_root.iterdir()
            if p.is_file() and p.suffix.lower() == ".lib"]
    return sorted(libs, key=lambda p: p.name.upper())


def extract_spot_check(fx: str, lib: Path, unpack_dir: Path, work_dir: Path) -> None:
    """Extract one entry by its case-folded name; must byte-match unpack output."""
    ls = run_fx(fx, "lib", "ls", str(lib))
    candidate = None
    for line in ls.stdout.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2].isdigit() and not parts[0].startswith(("Name", "-")):
            name = parts[0]
            if not set(name) & SANITIZED:
                candidate = name
                break
    if candidate is None:
        print(f"  spot-check: no sanitize-free entry in {lib.name}, skipped")
        return
    spot_dir = work_dir / "extract-spot"
    shutil.rmtree(spot_dir, ignore_errors=True)
    run_fx(fx, "lib", "extract", str(lib), candidate.lower(), "-o", str(spot_dir))
    extracted = spot_dir / candidate
    unpacked = unpack_dir / candidate
    if not extracted.is_file():
        sys.exit(f"spot-check: extract '{candidate.lower()}' wrote nothing")
    if sha256_file(extracted) != sha256_file(unpacked):
        sys.exit(f"spot-check: extract and unpack bytes differ for {candidate}")
    shutil.rmtree(spot_dir)
    print(f"  spot-check: extract '{candidate.lower()}' == unpack '{candidate}'")


def build_manifest(fx: str, fa_root: Path, work_dir: Path) -> dict[str, tuple[int, str]]:
    libs = discover_libs(fa_root)
    if not libs:
        sys.exit(f"no .LIB archives found in {fa_root}")
    work_dir.mkdir(parents=True, exist_ok=True)

    entries: dict[str, tuple[int, str]] = {}
    for i, lib in enumerate(libs):
        unpack_dir = work_dir / lib.name.upper()
        shutil.rmtree(unpack_dir, ignore_errors=True)
        proc = run_fx(fx, "lib", "unpack", str(lib), str(unpack_dir))
        skips = [l for l in proc.stderr.splitlines() if "SKIP" in l]
        files = sorted(p for p in unpack_dir.iterdir() if p.is_file())
        for p in files:
            entries[f"{lib.name.upper()}/{p.name}"] = (p.stat().st_size, sha256_file(p))
        print(f"  {lib.name}: {len(files)} files"
              + (f", {len(skips)} skipped ({skips[0].strip()} ...)" if skips else ""))
        if i == 0:
            extract_spot_check(fx, lib, unpack_dir, work_dir)
        shutil.rmtree(unpack_dir)

    sms = next((p for p in fa_root.iterdir()
                if p.is_file() and p.name.upper() == "FA.SMS"), None)
    if sms:
        csv = work_dir / "sms-dump.csv"
        run_fx(fx, "sms", "dump", str(sms), "-o", str(csv))
        entries[f"{sms.name.upper()}/sms-dump.csv"] = (csv.stat().st_size, sha256_file(csv))
        csv.unlink()
        print(f"  {sms.name}: sms-dump.csv hashed")
    return entries


def write_manifest(entries: dict[str, tuple[int, str]], out: Path) -> None:
    with open(out, "w", newline="\n") as f:
        for key in sorted(entries):
            size, digest = entries[key]
            f.write(f"{digest}  {size}  {key}\n")


def read_manifest(path: Path) -> dict[str, tuple[int, str]]:
    entries = {}
    with open(path) as f:
        for line in f:
            digest, size, key = line.rstrip("\n").split("  ", 2)
            entries[key] = (int(size), digest)
    return entries


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("mode", choices=["generate", "verify"])
    ap.add_argument("--fx", required=True, help="path to the fx executable")
    ap.add_argument("--fa-root", required=True, help="FA install directory")
    ap.add_argument("--out", help="manifest to write (generate)")
    ap.add_argument("--manifest", help="manifest to compare against (verify)")
    ap.add_argument("--work-dir", required=True,
                    help="scratch directory (under the build tree, not /tmp)")
    args = ap.parse_args()

    fa_root = Path(args.fa_root)
    if not fa_root.is_dir():
        sys.exit(f"FA root is not a directory: {fa_root}")

    entries = build_manifest(args.fx, fa_root, Path(args.work_dir))
    print(f"{len(entries)} artifacts hashed")

    if args.mode == "generate":
        if not args.out:
            sys.exit("generate requires --out")
        write_manifest(entries, Path(args.out))
        print(f"wrote {args.out}")
        return 0

    if not args.manifest:
        sys.exit("verify requires --manifest")
    expected = read_manifest(Path(args.manifest))
    missing = sorted(expected.keys() - entries.keys())
    extra = sorted(entries.keys() - expected.keys())
    mismatched = sorted(k for k in expected.keys() & entries.keys()
                        if expected[k] != entries[k])
    for k in missing:
        print(f"MISSING   {k}")
    for k in extra:
        print(f"EXTRA     {k}")
    for k in mismatched:
        print(f"MISMATCH  {k}: expected {expected[k]}, got {entries[k]}")
    if missing or extra or mismatched:
        print(f"FAIL: {len(missing)} missing, {len(extra)} extra, "
              f"{len(mismatched)} mismatched (of {len(expected)} expected)")
        return 1
    print(f"OK: all {len(expected)} artifacts byte-identical")
    return 0


if __name__ == "__main__":
    sys.exit(main())
