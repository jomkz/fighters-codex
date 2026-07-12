# Development

This is the full developer reference — build setup, IDE configuration, project
structure, and release workflow. For commit message conventions, see
[CONTRIBUTING.md](https://github.com/jomkz/fighters-codex/blob/main/CONTRIBUTING.md);
branch naming rules are [below](#branch-names).

Primary development happens on Fedora Linux; the Windows bench is kept for the
tasks listed in [What still needs the Windows bench](#what-still-needs-the-windows-bench).

## Prerequisites

On Linux (GCC or Clang):

```bash
sudo dnf install gcc-c++ clang cmake ninja-build python3 git SDL3-devel
```

Any distribution works with the equivalents: a C++17 compiler, CMake 3.21+,
Ninja, Python 3 (release scripts and the real-asset test harness), and Git.
`SDL3-devel` serves the `fxs` build; where no system SDL3 exists, the
build automatically compiles a pinned, checksummed SDL3 from source instead
(see [Vendored Dependencies](#vendored-dependencies) and
[ADR-0001](adr/0001-fx-gui-sdl3-opengl3-miniaudio.md)).

On Windows (MSVC):

- **Visual Studio 2022 or 2026** with the following workloads:
    - Desktop development with C++
    - C++ CMake tools for Windows (installs cmake.exe into the VS directory)
- **Windows 10 or 11** recommended for development (target runtime is Windows 7+)
- **Git**, and **Python 3** for the release scripts and real-asset harness

CMake ships with Visual Studio but is not added to `PATH` by default. Find
`cmake.exe` under your VS install (typically
`Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\`) and add that
directory to your user `PATH`.

## Building

All builds go through [CMake presets](https://github.com/jomkz/fighters-codex/blob/main/CMakePresets.json):

| Preset | OS | Compiler | Config | Binary dir |
|---|---|---|---|---|
| `gcc` | Linux | g++ | Debug | `build/gcc/` |
| `clang` | Linux | clang++ | Debug | `build/clang/` |
| `asan-ubsan` | Linux | clang++ + ASan/UBSan | Debug | `build/asan-ubsan/` |
| `coverage` | Linux | g++ + gcov (`--coverage`) | Debug | `build/coverage/` |
| `fuzz` | Linux | clang++ + libFuzzer/ASan/UBSan | Debug | `build/fuzz/` |
| `release` | Linux | default | Release | `build/release/` |
| `msvc` | Windows | MSVC x64 | multi-config | `build/` |

Linux day-to-day:

```bash
cmake --preset gcc            # configure (first time per preset)
cmake --build --preset gcc    # build everything
ctest --preset gcc            # run the test suite
```

Swap `gcc` for `clang`, `asan-ubsan` (sanitized build), or `release`
(optimized). Single targets: `cmake --build --preset gcc --target fx`.
Binaries land in `build/<preset>/cli/fx`, `build/<preset>/gui/fxs`,
`build/<preset>/lib/libfx_lib.a`, and `build/<preset>/tests/fx_tests`.

Two options steer the GUI build:

- `FX_BUILD_GUI` (default `ON`; `OFF` in the `coverage` and `fuzz` presets) —
  build `fxs` and its tests.
- `FX_SDL3_VENDORED` (default `OFF`) — skip `find_package(SDL3)` and always
  build the pinned FetchContent SDL3 statically; CI and the release workflow
  set it so shipped binaries stay self-contained.

Windows:

```powershell
cmake --preset msvc                 # configure
cmake --build --preset msvc-debug   # Debug build (development)
cmake --build --preset msvc         # Release build (CI/release parity)
ctest --preset msvc                 # run the test suite (Release)
```

The msvc preset uses the installed Visual Studio's default generator, so
artifacts keep their historical multi-config paths: `build\cli\Release\fx.exe`,
`build\gui\Release\fxs.exe`, `build\lib\Release\fx_lib.lib` (swap `Debug`
for debug builds).

Plain `cmake -B build` still works on both OSes for one-off configures, and is
the path embedders use (see [api.md](api.md)).

### Platform notes

- **macOS** is unsupported and ships no release artifacts; the presets are
  deliberately Linux/Windows-only. CI does compile and run the suite on
  macOS as an informational, never-blocking check (a plain `cmake -B`
  build with `continue-on-error` — see the CI table below), so gross
  portability breaks surface early even though the platform stays
  unsupported.
- The `msvc` preset assumes the platform-default generator is Visual Studio;
  a `CMAKE_GENERATOR` environment override (e.g. to Ninja) conflicts with its
  `x64` architecture setting.
- `fx` and `fxs` read arguments through narrow `argv`, but both embed an
  application manifest
  ([cmake/win-utf8.manifest](https://github.com/jomkz/fighters-codex/blob/main/cmake/win-utf8.manifest))
  declaring the UTF-8 active code page, so on Windows 10 1903+ non-ASCII
  file paths arrive and open correctly. On older Windows the manifest entry
  is ignored and paths outside the ANSI code page can still fail to open —
  harmless for FA's own data, which is 8.3 ASCII throughout (#165).
- `fxs.exe` builds as a `WIN32`-subsystem app (no console window), so
  PowerShell launches it detached without waiting. For the headless `--smoke`
  sweep, pipe the output so the shell waits and reports `$LASTEXITCODE` — see
  [gui.md](gui.md#platforms).

## Testing

`ctest --preset <name>` runs several layers:

- **Unit suite** (`fx_tests`): Catch2 codec tests against in-memory data and
  the committed synthetic fixtures under
  [tests/fixtures/](https://github.com/jomkz/fighters-codex/tree/main/tests/fixtures)
  (loaded through `tests/support/fixture.h`).
  Catch2 is fetched with FetchContent on the *first configure of each preset
  directory*, which needs network access; for offline work point
  `FETCHCONTENT_SOURCE_DIR_CATCH2` at an existing Catch2 v3.7.1 checkout.
- **`embed_smoke`**: configures and builds the [api.md](api.md) consumer
  contract as a child project — repo root via `add_subdirectory`, linking
  `fx::lib` into a shared library, offline. The child inherits the parent's
  generator, compiler, and config, but not sanitizer flags (deliberate: the
  test validates the consumer contract, not instrumentation).
- **`cli_e2e_lib`**: round-trips a synthetic archive through the real `fx`
  binary — pack, ls, extract, unpack, patch — byte-comparing every output.
- **GUI tests** (label `gui`): `gui_tests` covers the display-free gui units
  (string helpers, async-dialog completion queue, preview matrix math, and
  the audio player state machine on miniaudio's null backend via
  `FX_AUDIO_NULL=1`) on every leg; `gui_smoke` runs `fxs --smoke` — three
  frames rendered headlessly — on Linux (CI wraps it in `xvfb-run`).
- **Fuzz smoke runs** (`fuzz` preset only, label `fuzz`): each libFuzzer
  harness fuzzes for 60 seconds from its committed seed corpus — see
  [Fuzzing](#fuzzing).
- **Docs checks** (label `docs`): `check_status_selftest` and
  `check_status_docs` run
  [`tools/check_status.py`](https://github.com/jomkz/fighters-codex/blob/main/tools/check_status.py)
  on every preset leg, so a codec change that invalidates a format spec's
  front-matter claims — or leaves the generated
  [status matrix](fa/formats/STATUS.md) stale — fails `ctest` locally, not
  just the CI `docs-status` job. The same checker validates the game-executable
  reconstruction symbol database (`db/`), per-subsystem coverage, and the
  generated [reconstruction matrix](fa/reconstruction.md). See
  [spec-authoring.md](spec-authoring.md) and [db/README.md](https://github.com/jomkz/fighters-codex/blob/main/db/README.md).

Test data follows the **synthetic-first fixture policy**
([tests/fixtures/README.md](https://github.com/jomkz/fighters-codex/blob/main/tests/fixtures/README.md)):
everything committed is synthetic — produced by our own encoders or
hand-assembled from the format specs — and validation against real game data
runs only behind `FX_FA_ROOT` / `FX_FA_DISC1`+`FX_FA_DISC2` (below), under the
ctest label `integration`, on the local benches. CI never sees a byte of game
content.

### Real-asset integration mode (FX_FA_ROOT)

With a licensed FA install available, configure with `-DFX_FA_ROOT` (or set
the `FX_FA_ROOT` environment variable) to register the `fa_extract_manifest`
test:

```bash
cmake --preset gcc -DFX_FA_ROOT="/path/to/Fighters Anthology"
ctest --preset gcc -R fa_extract_manifest
```

The test unpacks every `.LIB` in the install and verifies each extracted
file's SHA-256 against the committed manifest
([tests/integration/fa-extract.sha256](https://github.com/jomkz/fighters-codex/blob/main/tests/integration/fa-extract.sha256)).
A manifest generated on one OS and verified on the other proves the extraction
pipeline is byte-identical across platforms. To regenerate after an intended
output change:

```bash
python3 tests/integration/fa_manifest.py generate \
  --fx build/release/cli/fx --fa-root "$FX_FA_ROOT" \
  --out tests/integration/fa-extract.sha256 \
  --work-dir build/release/fa-extract-work
```

Hashes are facts about the game data; the assets themselves must never enter
the repository (`*.LIB`, `*.PIC`, `*.PAL`, … are gitignored — keep it that way).

### Real-media install mode (FX_FA_DISC1/FX_FA_DISC2)

`FX_FA_ROOT` proves the pipeline that *reads* an installed tree. The retail discs
prove the one that *writes* it — the `ESA` installer archive and the `fx install`
engine. Point both variables at the two disc roots (an ISO mount, or a copy of
each) to register the `fa_disc_install` test:

```bash
udisksctl loop-setup -r -f disc1.iso && udisksctl mount -b /dev/loop0   # etc.
cmake --preset gcc \
  -DFX_FA_DISC1=/run/media/you/FA_1_00F1 -DFX_FA_DISC2=/run/media/you/FA_1_00F
ctest --preset gcc -R fa_disc_install
```

Both discs are needed: disc 1 carries the installer archive and no LIBs, disc 2
the reverse. Which is which is decided by content — the volume labels are
identical, and a Linux mount hands them out in either order, so the test asserts
the plan comes out the same whichever way round they are named. It checks the
plan for both scripts, extracts every `SETUP.ESA` entry against the committed
manifest ([tests/integration/fa-esa.sha256](https://github.com/jomkz/fighters-codex/blob/main/tests/integration/fa-esa.sha256)),
repacks the 110 MB archive byte-for-byte, and runs a real minimal install (73 MiB)
that it then verifies back against the disc. Regenerate the manifest exactly as
for `fa_manifest.py`, with `fa_disc.py generate --out …`.

Three options extend it:

- **`-DFX_FA_DISC_FULL=ON`** also *executes* the full 989 MiB install. The plan
  for it is checked either way — a plan is pure — so this only adds the copy.
- **`-DFX_FA_ROOT=…` set alongside** enables the **cross-build oracle**: a fresh
  install off the 1.00F discs is compared file-by-file against that 1.02F tree,
  and must differ in exactly the set the patch rewrites (`FA.EXE`, `FA.SMS`,
  `FA_1.LIB`, `FA_2.LIB`) and no other. That check is the executable statement of
  the gap the RTPatch codec closes: the discs ship 1.00F, `db/` describes 1.02F.
- **`-DFX_FA_PATCH=…` (the updater `fae102.exe`) set alongside** closes that gap:
  the harness installs 1.00F, runs `fx install --patch`, and checks the four
  rebuilt game files against the committed 1.02F manifest. Where the cross-build
  oracle asserts *which* files differ, this proves the patch *produces* the 1.02F
  tree. (`FX_FA_PATCH` also registers a standalone `fa_patch_apply` test — see
  below.)

The three self-oracles here need no committed hash at all: the four entries that
sit *both* inside `SETUP.ESA` and loose on disc 1 (`README.TXT`, `IP.EXE`,
`IP.CFG`, `EAHELP.HLP` — three of them PKWare-compressed) must extract to the
same bytes as the loose copies. The disc checks itself.

### Real-media RTPatch mode (FX_FA_PATCH)

`FX_FA_PATCH` names the 1.02F updater `fae102.exe`. Given the 1.00F originals —
from Disc 1's `SETUP.ESA` (`FX_FA_DISC1`) or an explicit `FX_FA_PATCH_SOURCE`
directory — the `fa_patch_apply` test applies the patch with `fx patch apply` and
checks each rebuilt file's SHA-256 against the committed
[fa-patch.sha256](https://github.com/jomkz/fighters-codex/blob/main/tests/integration/fa-patch.sha256):

```bash
cmake --preset gcc -DFX_FA_PATCH=/path/to/fae102.exe -DFX_FA_DISC1=/run/media/you/FA_1_00F1
ctest --preset gcc -R fa_patch_apply
```

`FA.SMS`, `FA_1.LIB`, `FA_2.LIB` and the **official** `FA.EXE` all reconstruct
byte-for-byte to 1.02F. Regenerate the manifest with `fa_patch.py generate
--out …` after an intended output change. Set `FX_FA_PATCH` alongside the disc
variables (above) to also drive the full install-then-`--patch` pipeline.

The committed `FA.EXE` hash is the **pristine official** 1.02F byte; a licensed
install's copy may differ by one byte if it carries a no-CD crack (a `JNZ`→`JZ`
flip in the CD check) — a property of that install, not of the patch.

## Fuzzing

libFuzzer harnesses live in [fuzz/](https://github.com/jomkz/fighters-codex/tree/main/fuzz) and build only under the `fuzz`
preset (Clang; the whole tree gets coverage instrumentation plus ASan/UBSan):

```bash
cmake --preset fuzz
cmake --build --preset fuzz --target fuzzers
ctest --preset fuzz              # 60-second smoke run per harness
```

For a longer local session, run a harness directly against its seed corpus:

```bash
build/fuzz/fuzz/fuzz_ealib -max_total_time=600 \
    -dict=fuzz/fuzz_ealib.dict fuzz/corpus/fuzz_ealib
```

To add a harness (the Phase 4 rollout, [epic #51](https://github.com/jomkz/fighters-codex/issues/51)):
create `fuzz/<name>.cpp` implementing `LLVMFuzzerTestOneInput`, add
`fx_add_fuzzer(<name>)` to `fuzz/CMakeLists.txt`, and commit tiny **synthetic**
seeds under `fuzz/corpus/<name>/` — never game assets, and name them
`seed-*.bin` (`*.LIB` and friends are gitignored by design). An optional
`fuzz/<name>.dict` is picked up automatically. The ctest smoke run and the CI
fuzz job need no further wiring.

Fuzzing runs in two CI tiers (#119):

- **Per-PR smoke** (`fuzz-smoke`, in ci.yml): 60 seconds per harness over its
  committed seed corpus — cheap enough to run every harness on every PR, so
  a parser regression on malformed input fails the PR that introduces it.
- **Weekly deep run**
  ([fuzz-deep.yml](https://github.com/jomkz/fighters-codex/blob/main/.github/workflows/fuzz-deep.yml),
  Tuesdays + `workflow_dispatch`): 30 minutes per harness with the same
  limits and dictionaries, deep enough to reach states the smoke never will.

**Finding policy:** findings are written as `crash-*`/`oom-*`/`timeout-*`
reproducers (gitignored; in CI they upload as the `fuzz-findings` /
`fuzz-deep-findings` artifacts). The deep run also fails red and auto-files —
or extends — an open `fuzzing`-labeled issue pointing at the run and its
reproducer artifact. Minimize with the harness's `-minimize_crash=1`, then
fix and add a Catch2 regression test in the same PR as the fix.

## Continuous Integration

Every PR to `main` (and every push to it) runs the
[CI workflow](https://github.com/jomkz/fighters-codex/blob/main/.github/workflows/ci.yml): a matrix that runs
`cmake --preset <p>`, `cmake --build --preset <p>`, `ctest --preset <p>` per leg.

| Check | Runner | Proves |
|---|---|---|
| `gcc` | ubuntu-latest | Linux GCC build + full test suite |
| `clang` | ubuntu-latest | Linux Clang build + full test suite |
| `asan-ubsan` | ubuntu-latest | Full suite under AddressSanitizer + UBSan — memory errors and UB in the binary parsers fail the PR |
| `msvc` | windows-latest | Windows MSVC build + full test suite |
| `macos (informational)` | macos-latest | AppleClang build + suite as an early-warning signal; `continue-on-error` — never blocks a PR |
| `fuzz-smoke` | ubuntu-latest | 60-second libFuzzer run per harness over its seed corpus — parser crashes on malformed input fail the PR |
| Fuzz (deep) | ubuntu-latest | Weekly scheduled [30-minute-per-harness run](https://github.com/jomkz/fighters-codex/blob/main/.github/workflows/fuzz-deep.yml); findings upload as reproducer artifacts and auto-file a `fuzzing` issue — see [Fuzzing](#fuzzing) |
| `docs-status` | ubuntu-latest | [`tools/check_status.py`](https://github.com/jomkz/fighters-codex/blob/main/tools/check_status.py) `--self-test` + `--check`: format-spec front-matter and template conformance ([spec-authoring.md](spec-authoring.md)), encoding and link hygiene across all markdown — relative links resolve case-exactly, links in `docs/` stay inside the docs tree, repo `blob`/`tree` URLs point at real `main` paths — front-matter claims vs. `lib/`+`cli/`+`tests/`+`fuzz/` reality, and currency of the generated [status matrix](fa/formats/STATUS.md) — a stale matrix fails the PR |
| `coverage` | ubuntu-latest | gcov line coverage over `lib/` + `cli/`, gcovr summary on the run's summary page + HTML artifact; enforces a floor that only ratchets **up** (raised by epic [#50](https://github.com/jomkz/fighters-codex/issues/50), never lowered) |
| CodeQL | ubuntu-latest | Static analysis ([security-extended](https://github.com/jomkz/fighters-codex/blob/main/.github/codeql/codeql-config.yml)) over all first-party C++; also runs weekly against refreshed query packs |
| Docs | ubuntu-latest | [`mkdocs build --strict`](https://github.com/jomkz/fighters-codex/blob/main/.github/workflows/docs.yml) over the whole docs tree — a broken link, broken anchor, or page missing from the site nav fails the PR; on push to `main` it also deploys the [published site](https://fighterscodex.com/) (runs only when docs or site config change) |

Every `uses:` in the workflows is pinned to a commit SHA (with the version in a
trailing comment); [Dependabot](https://github.com/jomkz/fighters-codex/blob/main/.github/dependabot.yml)
keeps the pins current. Test presets are configured with `noTestsAction: error`,
so a leg that discovers zero tests fails instead of passing vacuously.

## Documentation Site

The `docs/` tree is published as <https://fighterscodex.com/> —
an mkdocs-material site built from the same markdown sources GitHub renders.
The [Docs workflow](https://github.com/jomkz/fighters-codex/blob/main/.github/workflows/docs.yml)
builds the site with `mkdocs build --strict` on every PR that touches docs or
site config, and deploys to GitHub Pages on push to `main`. Strict mode plus
the `validation:` block in
[mkdocs.yml](https://github.com/jomkz/fighters-codex/blob/main/mkdocs.yml)
means a broken link, a broken `#anchor`, or a page missing from the nav fails
the build — site health is CI-enforced, not aspirational.

The site is served from the custom domain `fighterscodex.com`. Because Pages is
deployed from a workflow artifact rather than a branch, GitHub does not inject
the domain for us: `docs/CNAME` carries it, mkdocs copies that file verbatim
into `site/`, and the artifact is what Pages serves. **Deleting `docs/CNAME`
un-sets the custom domain on the next deploy**, so leave it in place. The apex
resolves via A/AAAA records to GitHub's Pages IPs, `www` is a CNAME to
`jomkz.github.io`, and `fighterscodex.org` is a registrar-level 301 redirect to
the `.com`.

To preview locally (the site toolchain is the repo's only pip dependency,
pinned in
[requirements-docs.txt](https://github.com/jomkz/fighters-codex/blob/main/requirements-docs.txt)):

```bash
python3 -m venv ~/.venvs/fx-docs
~/.venvs/fx-docs/bin/pip install -r requirements-docs.txt
~/.venvs/fx-docs/bin/mkdocs serve    # live preview at http://127.0.0.1:8000/
~/.venvs/fx-docs/bin/mkdocs build --strict   # what CI runs
```

Two conventions keep GitHub and the site rendering identically: heading
anchors use GitHub-style slugs (configured via `pymdownx.slugs.slugify`), and
links from `docs/` to files outside the docs tree (source, workflows, repo
meta) are written as absolute `github.com/...` URLs — `check_status.py`
verifies those URLs point at real paths, so they can't silently rot.

The site also exposes a **Print / PDF** page (mkdocs-print-site-plugin, last
in the `plugins:` list by requirement): every page concatenated into a single
document with a cover and table of contents — use the browser's print-to-PDF
for an offline copy of the whole knowledge base.

## IDE Setup

### VS Code (Linux and Windows)

Recommended extensions are declared in `.vscode/extensions.json`: C/C++,
CMake Tools, and Hex Editor (useful for inspecting binary game assets). CMake
Tools reads `CMakePresets.json` natively — pick the preset from the status
bar. IntelliSense works from the `compile_commands.json` each Linux preset
exports (clangd users: `--compile-commands-dir=${workspaceFolder}/build/gcc`).

Tasks in `.vscode/tasks.json` are preset-based and pick the right commands per
OS (gcc preset on Linux, msvc on Windows):

| Task | Shortcut | Action |
|---|---|---|
| Configure | — | `cmake --preset gcc` / `cmake --preset msvc` |
| Build all | `Ctrl+Shift+B` | `cmake --build --preset gcc` / `--preset msvc-debug` |
| Build fx (CLI) | — | Build all, restricted to the `fx` target |
| Build fx_tests | — | Build the test binary |
| Run tests | — | `ctest --preset gcc` / `ctest --preset msvc` |
| Run fxs | — | Launches the GUI (both OSes) |

If cmake is not in `PATH` on Windows, add it via `terminal.integrated.env.windows`
in your user `settings.json`.

### Visual Studio

Open the generated solution directly (`build\fighters-codex.sln` after
`cmake --preset msvc`), or use **File → Open → CMake…** on the root
`CMakeLists.txt` — VS configures the project automatically. Set the startup
project to `fxs` for F5 debugging.

## Project Structure

```
fighters-codex/
├── lib/                    # fx_lib static library (all codecs, no platform deps)
│   ├── include/fx/         # public headers
│   ├── src/                # codec implementations
│   └── vendor/             # stb (vendored)
├── cli/                    # fx CLI frontend
├── gui/                    # fxs ImGui frontend (SDL3 + OpenGL 3.3, Linux + Windows)
│   ├── src/
│   │   ├── main.cpp        # SDL3 + GL host, event loop, window placement, ImGui init
│   │   ├── app.h / app.cpp # App class, session management, menu bar
│   │   ├── platform/       # window, texture, dialogs, theme, fonts, audio player
│   │   ├── panels/         # lib_browser, editor_host, preview
│   │   └── editors/        # per-format editors (audio, mission, brf, pic, ...)
│   └── vendor/             # Dear ImGui, glad, miniaudio (vendored)
├── tests/                  # Catch2 suite, embed smoke, CLI e2e, FA integration
├── tools/                  # dll_info and other RE utilities
├── scripts/                # release tooling, Ghidra headless scripts
├── db/                     # game-executable reconstruction symbol database (epic #209)
└── docs/                   # documentation (the primary output)
```

### Adding a new editor

1. Add a new `EditorKind` enum value in `gui/src/app.h`
2. Wire the file extension to the new kind in `App::OpenEntry()` (`app.cpp`)
3. Create `gui/src/editors/<format>_editor.h` and `<format>_editor.cpp`
4. Call `Draw<Format>Editor(app)` from `DrawEditorHost()` in `gui/src/panels/editor_host.cpp`
5. Add the `.cpp` to `gui/CMakeLists.txt`

Editors never touch SDL or GL directly — file dialogs, GPU textures, and
audio go through `gui/src/platform/`. Dialogs are asynchronous: the
continuation runs frames later, so capture inputs by value at request time
and re-validate `app.editor` state on arrival (see
`gui/src/platform/dialogs.h`).

## What still needs the Windows bench

- **fxs interactive verification on Windows** — CI compiles it and runs
  the display-free `gui_tests`, but Windows runners expose no GL 3.3, so
  rendering, native dialogs, and theming need eyes on the bench
- **Release packaging** verification for the Windows zips (the Linux tarballs
  are exercised by the release workflow's Linux leg and its dry-run mode)
- **`re-gameplay` work**: anything requiring the running game, batched into
  bench campaigns (epic #56)
- The **Windows-side `fa_extract_manifest` verify run** that closes the
  cross-platform byte-identity loop for epic #42

## Branch Names

```
<type>/<short-kebab-description>
```

The type prefix matches the Conventional Commit type of the work
(see [CONTRIBUTING.md](https://github.com/jomkz/fighters-codex/blob/main/CONTRIBUTING.md)):

| Prefix | Use for | Example |
|---|---|---|
| `feat/` | New functionality | `feat/seq-codec` |
| `fix/` | Bug fixes | `fix/pic-palette-index` |
| `docs/` | Documentation-only changes | `docs/lib-format-spec` |
| `refactor/` | Restructuring without behavior change | `refactor/split-cmd-lib` |
| `build/` | Build system and tooling | `build/cmake-presets` |
| `test/` | Test-only additions | `test/fnt-fixtures` |
| `chore/` | Maintenance, releases, CI | `chore/release-v0.5.0` |

Rules: lowercase kebab-case after the slash, keep it short, one logical change
per branch. Branches merge to `main` via PR.

## Releasing

0. Optionally draft changelog entries from conventional commits since the last tag:

```bash
python3 scripts/draft-changelog.py
```

Review and edit `CHANGELOG.md` (the script drafts; you refine), then commit any
changes before releasing. See
[CONTRIBUTING.md](https://github.com/jomkz/fighters-codex/blob/main/CONTRIBUTING.md)
for the commit message format that drives this.

1. Ensure `CHANGELOG.md` has the desired content under `## [Unreleased]`.
2. When ready to ship, run the release script with the new version — from
   `main` (the script creates the `chore/release-vX.Y.Z` branch) or from that
   release branch if the changelog curation already lives there:

```bash
python3 scripts/release.py 0.5.0
```

This will:
- Switch to `chore/release-v0.5.0` when starting from `main`
- Bump the version in `CMakeLists.txt`
- Rotate `CHANGELOG.md` — promotes `[Unreleased]` to the new version with today's date and updates the comparison links
- Commit both files as `chore: release v0.5.0`

The script never tags: `main` is protected, so the release commit lands via
PR squash-merge, and the local commit's SHA never appears on `main` — a tag
created now would point at an unreachable commit (the v0.4.0 misfire).

3. Review the commit (`git show --stat HEAD`), push the branch, and open the
   PR:

```bash
git push -u origin chore/release-v0.5.0
gh pr create --fill
```

4. After CI is green, squash-merge the PR, then tag the squash commit:

```bash
git switch main && git pull
git log -1 --oneline   # must show: chore: release v0.5.0 (#<PR>)
git tag v0.5.0 && git push origin v0.5.0
```

5. After the release workflow publishes, verify all six artifacts (`fx`,
   `fxs`, and `fx-lib` — one Windows zip and one Linux tarball each) and
   bump fa-bridge's `extern/fx_lib` submodule to the new tag when the release
   changed `fx_lib`.

Pushing the tag triggers the GitHub Actions release workflow: it builds **and
tests** on both OSes, packages the Windows zips and Linux tarballs, extracts
the release body with `scripts/extract-changelog.py` (run it locally with a
version argument; the `changelog_extract` ctest exercises it on every run),
and publishes the GitHub Release.

To validate packaging without tagging, run the workflow manually
(**Actions → Release → Run workflow**): the dry run builds, tests, and
uploads the packages as workflow artifacts but skips the publish job.

## Vendored Dependencies

Runtime dependencies are checked in — the library, CLI, and GUI build without
a package manager. Two network fetches exist: Catch2 for the test suite (see
[Testing](#testing)), and SDL3 for the GUI *only when no system SDL3 is
found* — `find_package(SDL3)` first, then a pinned, SHA-256-checksummed
release tarball built statically (the decision record is
[ADR-0001](adr/0001-fx-gui-sdl3-opengl3-miniaudio.md); the pin lives in
[gui/CMakeLists.txt](https://github.com/jomkz/fighters-codex/blob/main/gui/CMakeLists.txt)).

| Library | Location | License |
|---|---|---|
| Dear ImGui (+ SDL3/OpenGL3 backends) | `gui/vendor/imgui/` | MIT |
| glad (GL 3.3 core loader) | `gui/vendor/glad/` | CC0 / Apache-2.0 (generated) |
| miniaudio | `gui/vendor/miniaudio/` | MIT-0 / Public Domain |
| stb_image | `lib/vendor/` | MIT / Public Domain |
| stb_image_write | `lib/vendor/` | MIT / Public Domain |
| blast (PKWare DCL) | `lib/src/blast.cpp` | zlib/libpng (Mark Adler) |
