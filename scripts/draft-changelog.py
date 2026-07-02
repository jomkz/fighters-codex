#!/usr/bin/env python3
"""Draft CHANGELOG.md entries under [Unreleased] from conventional commits
since the last git tag.

Usage:
    python3 scripts/draft-changelog.py [--since v0.1.0]

Stdlib-only; runs anywhere git and Python 3.8+ exist.
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CHANGELOG = ROOT / "CHANGELOG.md"

SECTION_MAP = {
    "feat": "Added",
    "fix": "Fixed",
    "docs": "Changed",
    "refactor": "Changed",
    "perf": "Changed",
}
OMIT_TYPES = {"chore", "ci", "build", "test", "style", "revert"}
SUBJECT_RE = re.compile(
    r"^(?P<type>[a-z]+)(\((?P<scope>[^)]+)\))?(?P<breaking>!)?:\s+(?P<desc>.+)$"
)
# Insertion order under [Unreleased]
SECTION_ORDER = ["Added", "Fixed", "Changed"]


def git(*args: str) -> str:
    return subprocess.run(
        ["git", "-C", str(ROOT), *args],
        capture_output=True, text=True, check=False,
    ).stdout.strip()


def read_text(path: Path) -> tuple[str, str]:
    """Return (text, encoding) preserving a UTF-8 BOM if present."""
    raw = path.read_bytes()
    enc = "utf-8-sig" if raw.startswith(b"\xef\xbb\xbf") else "utf-8"
    return raw.decode(enc), enc


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--since", default="",
                    help="base tag/ref (default: last tag via git describe)")
    args = ap.parse_args()

    since = args.since or git("describe", "--tags", "--abbrev=0")
    if not since:
        print("No tags found and --since not provided.", file=sys.stderr)
        return 1

    print(f"Drafting changelog entries since {since} ...")
    subjects = git("log", f"{since}..HEAD", "--pretty=format:%s")
    if not subjects:
        print(f"No commits found since {since}. Nothing to draft.")
        return 0

    entries: dict[str, list[str]] = {s: [] for s in SECTION_ORDER}
    for subject in (s.strip() for s in subjects.splitlines()):
        m = SUBJECT_RE.match(subject)
        if not m:
            continue  # non-conventional commits are silently skipped
        if m["type"] in OMIT_TYPES:
            continue
        qualifier = f"**{m['scope']}** " if m["scope"] else ""
        if m["breaking"]:
            entries["Changed"].insert(0, f"- **BREAKING** {qualifier}{m['desc']}")
        else:
            section = SECTION_MAP.get(m["type"], "Added")
            entries[section].append(f"- {qualifier}{m['desc']}")

    if not any(entries.values()):
        print(f"No conventional commits found since {since}. Nothing to draft.")
        print("(Non-conventional commits are skipped — see docs/development.md)")
        return 0

    content, enc = read_text(CHANGELOG)
    # Match only up to the next "## [" heading; keep the heading's newline out
    # of the greedy part so an empty section can't swallow the next release.
    block = re.search(r"(?ms)^(## \[Unreleased\][ \t]*\n)(.*?)(?=^## \[|\Z)",
                      content)
    if not block:
        print("Could not find '## [Unreleased]' in CHANGELOG.md", file=sys.stderr)
        return 1

    heading, body = block.group(1), block.group(2).strip("\n")
    counts: dict[str, int] = {}
    for section in SECTION_ORDER:
        # Idempotency: skip entries whose description already appears
        new = [e for e in entries[section]
               if re.sub(r"^- (\*\*[^*]+\*\* )*", "", e) not in body]
        if not new:
            continue
        counts[section] = len(new)
        joined = "\n".join(new)
        header_re = re.compile(rf"(^### {section}[ \t]*\n)", re.M)
        if header_re.search(body):
            body = header_re.sub(rf"\g<1>{joined}\n", body, count=1)
        else:
            body = f"### {section}\n{joined}\n\n{body}" if body else \
                   f"### {section}\n{joined}"

    if not counts:
        print("All drafted entries already present. Nothing to do.")
        return 0

    content = content.replace(block.group(0), f"{heading}\n{body}\n\n")
    CHANGELOG.write_bytes(content.encode(enc))

    print("\nDone. Entries drafted into [Unreleased]:")
    for section, n in counts.items():
        print(f"  ### {section}: {n}")
    print("\nReview CHANGELOG.md, edit as needed, then commit before releasing.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
