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
`SDL3-devel` serves the `fx-gui` build; where no system SDL3 exists, the
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
Binaries land in `build/<preset>/cli/fx`, `build/<preset>/gui/fx-gui`,
`build/<preset>/lib/libfx_lib.a`, and `build/<preset>/tests/fx_tests`.

Two options steer the GUI build:

- `FX_BUILD_GUI` (default `ON`; `OFF` in the `coverage` and `fuzz` presets) —
  build `fx-gui` and its tests.
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
`build\gui\Release\fx-gui.exe`, `build\lib\Release\fx_lib.lib` (swap `Debug`
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
- `fx` reads arguments through narrow `argv`, so non-ASCII file paths on
  Windows depend on the active code page. FA's data is 8.3 ASCII throughout,
  so this doesn't bite in practice.

## Testing

`ctest --preset <name>` runs several layers:

- **Unit suite** (`fx_tests`): Catch2 codec tests against in-memory fixtures.
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
  `FX_AUDIO_NULL=1`) on every leg; `gui_smoke` runs `fx-gui --smoke` — three
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
  just the CI `docs-status` job. See [spec-authoring.md](spec-authoring.md).

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

Findings are written as `crash-*`/`oom-*`/`timeout-*` reproducers (gitignored;
in CI they upload as the `fuzz-findings` artifact). Minimize with the
harness's `-minimize_crash=1`, then fix and add a Catch2 regression test
before merging.

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
| `docs-status` | ubuntu-latest | [`tools/check_status.py`](https://github.com/jomkz/fighters-codex/blob/main/tools/check_status.py) `--self-test` + `--check`: format-spec front-matter and template conformance ([spec-authoring.md](spec-authoring.md)), encoding and link hygiene across all markdown — relative links resolve case-exactly, links in `docs/` stay inside the docs tree, repo `blob`/`tree` URLs point at real `main` paths — front-matter claims vs. `lib/`+`cli/`+`tests/`+`fuzz/` reality, and currency of the generated [status matrix](fa/formats/STATUS.md) — a stale matrix fails the PR |
| `coverage` | ubuntu-latest | gcov line coverage over `lib/` + `cli/`, gcovr summary on the run's summary page + HTML artifact; enforces a floor that only ratchets **up** (raised by epic [#50](https://github.com/jomkz/fighters-codex/issues/50), never lowered) |
| CodeQL | ubuntu-latest | Static analysis ([security-extended](https://github.com/jomkz/fighters-codex/blob/main/.github/codeql/codeql-config.yml)) over all first-party C++; also runs weekly against refreshed query packs |
| Docs | ubuntu-latest | [`mkdocs build --strict`](https://github.com/jomkz/fighters-codex/blob/main/.github/workflows/docs.yml) over the whole docs tree — a broken link, broken anchor, or page missing from the site nav fails the PR; on push to `main` it also deploys the [published site](https://jomkz.github.io/fighters-codex/) (runs only when docs or site config change) |

Every `uses:` in the workflows is pinned to a commit SHA (with the version in a
trailing comment); [Dependabot](https://github.com/jomkz/fighters-codex/blob/main/.github/dependabot.yml)
keeps the pins current. Test presets are configured with `noTestsAction: error`,
so a leg that discovers zero tests fails instead of passing vacuously.

## Documentation Site

The `docs/` tree is published as <https://jomkz.github.io/fighters-codex/> —
an mkdocs-material site built from the same markdown sources GitHub renders.
The [Docs workflow](https://github.com/jomkz/fighters-codex/blob/main/.github/workflows/docs.yml)
builds the site with `mkdocs build --strict` on every PR that touches docs or
site config, and deploys to GitHub Pages on push to `main`. Strict mode plus
the `validation:` block in
[mkdocs.yml](https://github.com/jomkz/fighters-codex/blob/main/mkdocs.yml)
means a broken link, a broken `#anchor`, or a page missing from the nav fails
the build — site health is CI-enforced, not aspirational.

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
| Run fx-gui | — | Launches the GUI (both OSes) |

If cmake is not in `PATH` on Windows, add it via `terminal.integrated.env.windows`
in your user `settings.json`.

### Visual Studio

Open the generated solution directly (`build\fighters-codex.sln` after
`cmake --preset msvc`), or use **File → Open → CMake…** on the root
`CMakeLists.txt` — VS configures the project automatically. Set the startup
project to `fx-gui` for F5 debugging.

## Project Structure

```
fighters-codex/
├── lib/                    # fx_lib static library (all codecs, no platform deps)
│   ├── include/fx/         # public headers
│   ├── src/                # codec implementations
│   └── vendor/             # stb (vendored)
├── cli/                    # fx CLI frontend
├── gui/                    # fx-gui ImGui frontend (SDL3 + OpenGL 3.3, Linux + Windows)
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

- **fx-gui interactive verification on Windows** — CI compiles it and runs
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
| `chore/` | Maintenance, releases, CI | `chore/release-0-4-0` |

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
2. When ready to ship, run the release script with the new version:

```bash
python3 scripts/release.py 0.4.0
```

This will:
- Bump the version in `CMakeLists.txt`
- Rotate `CHANGELOG.md` — promotes `[Unreleased]` to the new version with today's date and updates the comparison links
- Commit both files as `chore: release v0.4.0`
- Create the tag `v0.4.0`

3. Review the commit (`git log --oneline -2`, `git diff HEAD~1`), then push:

```bash
git push origin main --tags
```

4. After the release workflow publishes, bump fa-content's `extern/fx_lib`
   submodule to the new tag.

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
