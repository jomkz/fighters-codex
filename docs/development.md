# Development

This is the full developer reference — build setup, IDE configuration, project
structure, and release workflow. For commit message conventions, see
[CONTRIBUTING.md](../CONTRIBUTING.md); branch naming rules are
[below](#branch-names).

Primary development happens on Fedora Linux; the Windows bench is kept for the
tasks listed in [What still needs the Windows bench](#what-still-needs-the-windows-bench).

## Prerequisites

On Linux (GCC or Clang):

```bash
sudo dnf install gcc-c++ clang cmake ninja-build python3 git
```

Any distribution works with the equivalents: a C++17 compiler, CMake 3.21+,
Ninja, Python 3 (release scripts and the real-asset test harness), and Git.

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

All builds go through [CMake presets](../CMakePresets.json):

| Preset | OS | Compiler | Config | Binary dir |
|---|---|---|---|---|
| `gcc` | Linux | g++ | Debug | `build/gcc/` |
| `clang` | Linux | clang++ | Debug | `build/clang/` |
| `asan-ubsan` | Linux | clang++ + ASan/UBSan | Debug | `build/asan-ubsan/` |
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
Binaries land in `build/<preset>/cli/fx`, `build/<preset>/lib/libfx_lib.a`,
and `build/<preset>/tests/fx_tests`. `fx-gui` is Windows-only until the
Phase 3 port ([epic #46](https://github.com/jomkz/fighters-codex/issues/46))
and is skipped on non-Windows configures.

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

- **macOS** is untested and unsupported; the presets are deliberately
  Linux/Windows-only. A plain `cmake -B build` may work but is unverified.
- The `msvc` preset assumes the platform-default generator is Visual Studio;
  a `CMAKE_GENERATOR` environment override (e.g. to Ninja) conflicts with its
  `x64` architecture setting.
- `fx` reads arguments through narrow `argv`, so non-ASCII file paths on
  Windows depend on the active code page. FA's data is 8.3 ASCII throughout,
  so this doesn't bite in practice.

## Testing

`ctest --preset <name>` runs three layers:

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
([tests/integration/fa-extract.sha256](../tests/integration/fa-extract.sha256)).
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
| Run fx-gui | — | Launches the GUI (Windows; prints a Phase 3 note on Linux) |

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
├── gui/                    # fx-gui ImGui/DX11 frontend (Windows until Phase 3)
│   ├── src/
│   │   ├── main.cpp        # Win32 + DX11 host, window placement, ImGui init
│   │   ├── app.h / app.cpp # App class, session management, menu bar
│   │   ├── panels/         # lib_browser, editor_host, preview
│   │   └── editors/        # per-format editors (audio, mission, brf, pic, ...)
│   └── vendor/             # Dear ImGui (vendored)
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

## What still needs the Windows bench

- **fx-gui** build, run, and debugging (Win32/DX11 until epic #46)
- **Release packaging** verification (`release.yml` artifacts are Windows x64)
- **`re-gameplay` work**: anything requiring the running game, batched into
  bench campaigns (epic #56)
- The **Windows-side `fa_extract_manifest` verify run** that closes the
  cross-platform byte-identity loop for epic #42

## Branch Names

```
<type>/<short-kebab-description>
```

The type prefix matches the Conventional Commit type of the work
(see [CONTRIBUTING.md](../CONTRIBUTING.md)):

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
changes before releasing. See [CONTRIBUTING.md](../CONTRIBUTING.md) for the commit
message format that drives this.

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

Pushing the tag triggers the GitHub Actions release workflow, which builds the
artifacts and publishes the GitHub Release using the new CHANGELOG entry as the
release body.

## Vendored Dependencies

All runtime dependencies are checked in — the library, CLI, and GUI build
without a package manager. The only network fetch is Catch2 for the test
suite (see [Testing](#testing)).

| Library | Location | License |
|---|---|---|
| Dear ImGui | `gui/vendor/imgui/` | MIT |
| stb_image | `lib/vendor/` | MIT / Public Domain |
| stb_image_write | `lib/vendor/` | MIT / Public Domain |
| blast (PKWare DCL) | `lib/src/blast.cpp` | zlib/libpng (Mark Adler) |
