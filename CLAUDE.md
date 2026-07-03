# CLAUDE.md

## What this project is

A reverse-engineering effort for Jane's Fighters Anthology (1998). **The documentation is
the primary output** — format specs (docs/fa/formats/), architecture notes (docs/fa/),
recovered symbols. The `fx_lib` library, `fx` CLI, and `fx-gui` are the **validation
layer**: a byte-identical codec is the proof that a format is understood. Never treat the
tools as the product and the docs as an afterthought.

Work is planned in [docs/roadmap.md](docs/roadmap.md): milestones = phases, `epic`-labeled
issues + native sub-issues = work breakdown. New work belongs under the epic it serves.

## Related repositories

- **fighters-legacy/fighters-legacy** — clean-room GPL engine reimplementation (separate
  roadmap; no codex work is tracked there).
- **fighters-legacy/fa-content** — FA bridge plugin; consumes `fx_lib` as the
  `extern/fx_lib` submodule. RE documentation lives *here*; implementations that consume it
  live *there*. Bump its submodule after each release.
- **License boundary:** this repo is MIT; OpenFA and fa-content are GPL. Document facts
  with attribution; never transcribe code across the boundary.

## Environments

- **Primary development: Fedora Linux.** Configure with `cmake --preset gcc`, build with
  `cmake --build --preset gcc`, run tests with `ctest --preset gcc` (presets: gcc, clang,
  asan-ubsan, release on Linux; msvc on Windows — see docs/development.md).
- **Windows bench** (kept for validation): has the licensed FA install. Anything needing
  the *running game* is labeled `re-gameplay` and batched for bench campaigns (see epic
  #56). Real-game asset validation on either bench uses `FX_FA_ROOT` integration mode:
  configure with `-DFX_FA_ROOT=<FA install>` (or the env var) to register the
  `fa_extract_manifest` CTest, which checks every extracted byte against
  tests/integration/fa-extract.sha256 (hashes are facts; the assets stay outside git).
  Never commit game assets (`*.LIB`, `*.PIC`, `*.PAL`, …).
- Ghidra work runs on Fedora once #120 (workbench migration) lands; headless scripts are
  in `scripts/ghidra/`.

## Conventions

- Conventional Commits + branch names per [CONTRIBUTING.md](CONTRIBUTING.md)
  (`<type>/<kebab-description>`; scopes `fx-lib`/`fx-cli`/`fx-gui`).
- Releases: `python3 scripts/draft-changelog.py` to draft, curate CHANGELOG.md, then
  `python3 scripts/release.py X.Y.Z` and push with tags. Then bump fa-content's submodule.
- C++17, zero external runtime dependencies; vendored libs only (`lib/vendor/`,
  `gui/vendor/`).

## The docs-currency rule

Every codec/CLI/GUI change updates, **in the same PR**: the format spec it validates,
the relevant reference doc (docs/cli.md, docs/api.md, docs/gui.md), and the generated
per-format status matrix (docs/fa/formats/STATUS.md — regenerate with
`python3 tools/check_status.py --write-matrix`). Specs follow docs/spec-authoring.md;
`tools/check_status.py --check` enforces the template, the front-matter claims, and
matrix currency in CI (`docs-status` job) and in ctest (label `docs`); the rule covers
the rest. The docs tree is also published as a site
(https://jomkz.github.io/fighters-codex/ — mkdocs.yml, `Docs` CI job): the strict build
fails on broken links/anchors or pages missing from the nav, and links from docs/ to
files outside the docs tree must be repo blob/tree URLs (also checked). The truth-pass
lesson: claims that aren't mechanically checked drift — don't add unchecked claims to
README.
