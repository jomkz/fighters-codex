# Development

This is the full developer reference — build setup, IDE configuration, project
structure, and release workflow. For commit message and branch naming conventions,
see [CONTRIBUTING.md](../CONTRIBUTING.md).

## Prerequisites

- **Visual Studio 2022 or 2026** (MSVC) with the following workloads:
  - Desktop development with C++
  - C++ CMake tools for Windows (installs cmake.exe into the VS directory)
- **Git**
- **Windows 10 or 11** recommended for development (target runtime is Windows 7+)

CMake ships with Visual Studio but is not added to `PATH` by default. The easiest fix is to add it manually — find `cmake.exe` under your VS install (typically `Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\`) and add that directory to your user `PATH`, or use the `$cmake` variable pattern shown below.

## Building

### Configure (first time only)

```powershell
cmake -B build -G "Visual Studio 17 2022"   # VS 2022
cmake -B build -G "Visual Studio 18 2026"   # VS 2026
```

If cmake is not in `PATH`:

```powershell
# Adjust <version> (2022/2026) and <edition> (Community/Professional/Enterprise) for your install
$cmake = "$env:ProgramFiles\Microsoft Visual Studio\<version>\<edition>\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $cmake -B build -G "Visual Studio 17 2022"
```

### Build targets

```powershell
cmake --build build --config Debug              # all targets
cmake --build build --target fx-gui --config Debug    # GUI only
cmake --build build --target fx     --config Debug    # CLI only
cmake --build build --config Release            # release build
```

Output locations:

| Target | Debug | Release |
|---|---|---|
| `fx-gui.exe` | `build\gui\Debug\fx-gui.exe` | `build\gui\Release\fx-gui.exe` |
| `fx.exe` | `build\cli\Debug\fx.exe` | `build\cli\Release\fx.exe` |
| `fx_lib.lib` | `build\lib\Debug\fx_lib.lib` | `build\lib\Release\fx_lib.lib` |

## IDE Setup

### VS Code

VS Code works well for editing and building. CMake configuration is done once from a terminal; after that the provided tasks handle the build/run cycle.

**Recommended extensions:**
- C/C++ (Microsoft)
- CMake Tools (Microsoft)
- Hex Editor (Microsoft) — useful for inspecting binary game assets

**Build and run tasks** are pre-configured in `.vscode/tasks.json`:

| Task | Shortcut | Action |
|---|---|---|
| Build fx-gui | `Ctrl+Shift+B` | `cmake --build build --target fx-gui --config Debug` |
| Run fx-gui | — | Builds then launches `build\gui\Debug\fx-gui.exe` |

If cmake is not in `PATH`, add it via VS Code's `terminal.integrated.env.windows` setting in your user `settings.json`:

```json
"terminal.integrated.env.windows": {
    "PATH": "C:\\Program Files\\Microsoft Visual Studio\\<version>\\<edition>\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin;${env:PATH}"
}
```

Replace `<version>` (e.g. `2022`, `2026`) and `<edition>` (e.g. `Community`, `Professional`) to match your install.

### Visual Studio

Open the generated solution directly:

```
build\fighters-codex.sln
```

Or use **File â†’ Open â†’ CMakeâ€¦** to open the root `CMakeLists.txt` — VS will configure the project automatically. Set the startup project to `fx-gui` for F5 debugging.

## Project Structure

```
fighters-codex/
â”œâ”€â”€ lib/                    # fx_lib static library (all codecs, no platform deps)
â”‚   â”œâ”€â”€ include/fx/         # public headers
â”‚   â””â”€â”€ src/                # codec implementations
â”œâ”€â”€ cli/                    # fx.exe CLI frontend
â”‚   â””â”€â”€ src/
â”œâ”€â”€ gui/                    # fx-gui.exe ImGui/DX11 frontend
â”‚   â”œâ”€â”€ src/
â”‚   â”‚   â”œâ”€â”€ main.cpp        # Win32 + DX11 host, window placement, ImGui init
â”‚   â”‚   â”œâ”€â”€ app.h / app.cpp # App class, session management, menu bar
â”‚   â”‚   â”œâ”€â”€ panels/         # lib_browser, editor_host, preview
â”‚   â”‚   â””â”€â”€ editors/        # per-format editors (audio, mission, brf, pic, â€¦)
â”‚   â””â”€â”€ vendor/             # Dear ImGui (vendored)
â””â”€â”€ docs/                   # documentation
```

### Adding a new editor

1. Add a new `EditorKind` enum value in `gui/src/app.h`
2. Wire the file extension to the new kind in `App::OpenEntry()` (`app.cpp`)
3. Create `gui/src/editors/<format>_editor.h` and `<format>_editor.cpp`
4. Call `Draw<Format>Editor(app)` from `DrawEditorHost()` in `gui/src/panels/editor_host.cpp`
5. Add the `.cpp` to `gui/CMakeLists.txt`

## Releasing

0. Optionally draft changelog entries from conventional commits since the last tag:

```powershell
.\scripts\draft-changelog.ps1
```

Review and edit `CHANGELOG.md` (the script drafts; you refine), then commit any
changes before releasing. See [CONTRIBUTING.md](../CONTRIBUTING.md) for the commit
message format that drives this.

1. Ensure `CHANGELOG.md` has the desired content under `## [Unreleased]`.
2. When ready to ship, run the release script with the new version:

```powershell
.\scripts\release.ps1 0.2.0
```

This will:
- Bump the version in `CMakeLists.txt`
- Rotate `CHANGELOG.md` — promotes `[Unreleased]` to the new version with today's date and updates the comparison links
- Commit both files as `chore: release v0.2.0`
- Create the tag `v0.2.0`

3. Review the commit (`git log --oneline -2`, `git diff HEAD~1`), then push:

```powershell
git push origin main --tags
```

Pushing the tag triggers the GitHub Actions release workflow, which builds the artifacts and publishes the GitHub Release using the new CHANGELOG entry as the release body.

## Vendored Dependencies

All dependencies are checked in — no package manager or internet access required to build.

| Library | Location | License |
|---|---|---|
| Dear ImGui | `gui/vendor/imgui/` | MIT |
| stb_image | `lib/vendor/` | MIT / Public Domain |
| stb_image_write | `lib/vendor/` | MIT / Public Domain |
| blast (PKWare DCL) | `lib/src/blast.cpp` | zlib/libpng (Mark Adler) |
