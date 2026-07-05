#!/usr/bin/env python3
"""Full-tree container repack for a licensed FA install (FX_FA_ROOT mode).

Runs `fx lib repack` over every .LIB in the FA root and byte-compares each
rebuilt archive against the original: the container serializer must
reproduce every shipped archive exactly — directory layout, the all-zero
terminator entry, the contiguous payload region — with offsets recomputed
from scratch. This is the project's definition of proof, mechanized at the
container level (#115). Entry payloads stay compressed, so the result is
independent of any per-format codec.

Stdlib-only, like fa_manifest.py. Each rebuilt archive is deleted before
the next one, so peak disk stays at one archive.
"""

import argparse
import filecmp
import subprocess
import sys
from pathlib import Path


def discover_libs(fa_root: Path) -> list[Path]:
    libs = [p for p in fa_root.iterdir()
            if p.is_file() and p.suffix.lower() == ".lib"]
    return sorted(libs, key=lambda p: p.name.upper())


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--fx", required=True, help="path to the fx executable")
    ap.add_argument("--fa-root", required=True, help="FA install directory")
    ap.add_argument("--work-dir", required=True,
                    help="scratch directory (under the build tree, not /tmp)")
    args = ap.parse_args()

    fa_root = Path(args.fa_root)
    if not fa_root.is_dir():
        sys.exit(f"FA root is not a directory: {fa_root}")
    libs = discover_libs(fa_root)
    if not libs:
        sys.exit(f"no .LIB archives found in {fa_root}")
    work_dir = Path(args.work_dir)
    work_dir.mkdir(parents=True, exist_ok=True)

    failures = 0
    for lib in libs:
        rebuilt = work_dir / lib.name.upper()
        rebuilt.unlink(missing_ok=True)
        proc = subprocess.run([args.fx, "lib", "repack", str(lib), str(rebuilt)],
                              capture_output=True, text=True)
        if proc.returncode != 0:
            print(f"REPACK-FAIL {lib.name}: {proc.stderr.strip()}")
            failures += 1
            continue
        if filecmp.cmp(lib, rebuilt, shallow=False):
            print(f"IDENTICAL   {lib.name} ({lib.stat().st_size} bytes)")
        else:
            print(f"DIFFERS     {lib.name}")
            failures += 1
        rebuilt.unlink(missing_ok=True)

    if failures:
        print(f"FAIL: {failures} of {len(libs)} archives did not round-trip")
        return 1
    print(f"OK: all {len(libs)} archives repack byte-identically")
    return 0


if __name__ == "__main__":
    sys.exit(main())
