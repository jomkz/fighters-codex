#!/usr/bin/env python3
"""Prepare a release: bump the version in CMakeLists.txt, rotate CHANGELOG.md,
and commit both files on a chore/release-vX.Y.Z branch.

main is protected, so the release commit lands via PR (squash-merge) and the
tag goes on the squash commit afterwards — this script never tags. Run it from
main (it creates the release branch) or from the release branch itself; it
refuses to commit anywhere else.

Usage:
    python3 scripts/release.py 0.5.0

Stdlib-only; runs anywhere git and Python 3.8+ exist.
"""
import datetime
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CMAKE = ROOT / "CMakeLists.txt"
CHANGELOG = ROOT / "CHANGELOG.md"
REPO_URL = "https://github.com/jomkz/fighters-codex"


def git(*args: str) -> subprocess.CompletedProcess:
    return subprocess.run(["git", "-C", str(ROOT), *args],
                          capture_output=True, text=True, check=False)


def read_text(path: Path) -> tuple[str, str]:
    """Return (text, encoding) preserving a UTF-8 BOM if present."""
    raw = path.read_bytes()
    enc = "utf-8-sig" if raw.startswith(b"\xef\xbb\xbf") else "utf-8"
    return raw.decode(enc), enc


def fail(msg: str) -> int:
    print(msg, file=sys.stderr)
    return 1


def main() -> int:
    if len(sys.argv) != 2 or not re.fullmatch(r"\d+\.\d+\.\d+", sys.argv[1]):
        return fail("Usage: release.py X.Y.Z  (e.g. release.py 0.3.0)")
    version = sys.argv[1]
    tag = f"v{version}"

    if git("status", "--porcelain", "--untracked-files=no").stdout.strip():
        return fail("Working tree has uncommitted changes. "
                    "Commit or stash them before releasing.")
    if git("tag", "--list", tag).stdout.strip():
        return fail(f"Tag '{tag}' already exists. If it is a leftover from an "
                    f"aborted release, delete it: git tag -d {tag}")
    if f"## [{version}]" in read_text(CHANGELOG)[0]:
        return fail(f"CHANGELOG.md already has a [{version}] section — "
                    f"this release looks prepared. Re-running would rotate "
                    f"the changelog twice.")

    # main is protected: the release commit must land via PR, so work happens
    # on the release branch — created here when starting from main.
    branch = git("rev-parse", "--abbrev-ref", "HEAD").stdout.strip()
    release_branch = f"chore/release-{tag}"
    if branch == "main":
        if git("rev-parse", "--verify", "--quiet", release_branch).returncode == 0:
            return fail(f"Branch '{release_branch}' already exists. Switch to "
                        f"it or delete it before re-running.")
        r = git("switch", "-c", release_branch)
        if r.returncode != 0:
            return fail(r.stderr.strip() or "git switch -c failed")
    elif branch != release_branch:
        return fail(f"On branch '{branch}'. Releases are cut from main or "
                    f"from '{release_branch}'.")

    today = datetime.date.today().isoformat()

    # --- CMakeLists.txt ---
    cmake, cmake_enc = read_text(CMAKE)
    new_cmake, n = re.subn(r"VERSION \d+\.\d+\.\d+", f"VERSION {version}",
                           cmake, count=1)
    if n == 0:
        return fail("Could not find 'VERSION x.y.z' in CMakeLists.txt")
    CMAKE.write_bytes(new_cmake.encode(cmake_enc))

    # --- CHANGELOG.md ---
    chlog, chlog_enc = read_text(CHANGELOG)
    new_chlog, n = re.subn(
        r"## \[Unreleased\]",
        f"## [Unreleased]\n\n## [{version}] - {today}",
        chlog, count=1)
    if n == 0:
        return fail("CHANGELOG.md does not contain an '## [Unreleased]' section.")
    new_chlog = re.sub(r"\[Unreleased\]: .+",
                       f"[Unreleased]: {REPO_URL}/compare/{tag}...HEAD",
                       new_chlog, count=1)
    # Insert the new release link before the first existing version link only
    # (the PowerShell predecessor replaced globally, duplicating the link once
    # more than one prior release existed).
    new_chlog = re.sub(r"(?m)^(\[\d+\.\d+\.\d+\]:)",
                       f"[{version}]: {REPO_URL}/releases/tag/{tag}\n\\1",
                       new_chlog, count=1)
    CHANGELOG.write_bytes(new_chlog.encode(chlog_enc))

    # --- Commit (no tag: under squash-merge this commit's SHA never lands
    # on main, so the tag goes on the squash commit after the PR merges) ---
    for cmd in (["add", str(CMAKE), str(CHANGELOG)],
                ["commit", "-m", f"chore: release {tag}"]):
        r = git(*cmd)
        if r.returncode != 0:
            return fail(r.stderr.strip() or f"git {cmd[0]} failed")

    print(f"""
Release {tag} prepared on '{release_branch}'. Review the commit
(git show --stat HEAD), then land it via PR:

  git push -u origin {release_branch}
  gh pr create --fill

After CI is green, squash-merge the PR, then tag the squash commit:

  git switch main && git pull
  git log -1 --oneline   # must show: chore: release {tag} (#<PR>)
  git tag {tag} && git push origin {tag}

Post-release checklist:
  - Verify the Release workflow published all six artifacts
  - Bump fa-bridge's extern/fx_lib submodule to {tag} (when fx_lib changed)
""")
    return 0


if __name__ == "__main__":
    sys.exit(main())
