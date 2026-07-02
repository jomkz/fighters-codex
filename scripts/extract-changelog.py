#!/usr/bin/env python3
"""Print the CHANGELOG.md section body for one released version to stdout.

Used by release.yml's publish job as the GitHub Release body. Run locally:

    python3 scripts/extract-changelog.py 0.3.0

Exits non-zero if the version has no section or the section is empty.
Stdlib-only; Python 3.8+.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CHANGELOG = ROOT / "CHANGELOG.md"


def extract(text: str, version: str):
    """Return the body under '## [version]', or None if absent.

    The section ends at the next '## [' heading — or, for the oldest
    version in the file, at the link-definition block / end of file.
    """
    pattern = re.compile(
        r"^## \[" + re.escape(version) + r"\][^\n]*\n(.*?)(?=^## \[|\Z)",
        re.S | re.M)
    m = pattern.search(text)
    if m is None:
        return None
    body = m.group(1)
    # The oldest section is trailed by the comparison-link definitions
    body = re.sub(r"^\[[^\]]+\]:\s+\S+\s*$", "", body, flags=re.M)
    return body.strip()


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: extract-changelog.py <version>", file=sys.stderr)
        return 2
    version = sys.argv[1].lstrip("v")
    body = extract(CHANGELOG.read_text(encoding="utf-8-sig"), version)
    if body is None:
        print(f"error: no CHANGELOG section for version {version}", file=sys.stderr)
        return 1
    if not body:
        print(f"error: CHANGELOG section for version {version} is empty", file=sys.stderr)
        return 1
    print(body)
    return 0


if __name__ == "__main__":
    sys.exit(main())
