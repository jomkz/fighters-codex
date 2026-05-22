# Development

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
cmake --build build --target ft-gui --config Debug    # GUI only
cmake --build build --target ft     --config Debug    # CLI only
cmake --build build --config Release            # release build
```

Output locations:

| Target | Debug | Release |
|---|---|---|
| `ft-gui.exe` | `build\gui\Debug\ft-gui.exe` | `build\gui\Release\ft-gui.exe` |
| `ft.exe` | `build\cli\Debug\ft.exe` | `build\cli\Release\ft.exe` |
| `ft_lib.lib` | `build\lib\Debug\ft_lib.lib` | `build\lib\Release\ft_lib.lib` |

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
| Build ft-gui | `Ctrl+Shift+B` | `cmake --build build --target ft-gui --config Debug` |
| Run ft-gui | — | Builds then launches `build\gui\Debug\ft-gui.exe` |

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
build\fighters-toolkit.sln
```

Or use **File → Open → CMake…** to open the root `CMakeLists.txt` — VS will configure the project automatically. Set the startup project to `ft-gui` for F5 debugging.

## Project Structure

```
fighters-toolkit/
├── lib/                    # ft_lib static library (all codecs, no platform deps)
│   ├── include/ft/         # public headers
│   └── src/                # codec implementations
├── cli/                    # ft.exe CLI frontend
│   └── src/
├── gui/                    # ft-gui.exe ImGui/DX11 frontend
│   ├── src/
│   │   ├── main.cpp        # Win32 + DX11 host, window placement, ImGui init
│   │   ├── app.h / app.cpp # App class, session management, menu bar
│   │   ├── panels/         # lib_browser, editor_host, preview
│   │   └── editors/        # per-format editors (audio, mission, brf, pic, …)
│   └── vendor/             # Dear ImGui (vendored)
└── docs/                   # documentation
```

### Adding a new editor

1. Add a new `EditorKind` enum value in `gui/src/app.h`
2. Wire the file extension to the new kind in `App::OpenEntry()` (`app.cpp`)
3. Create `gui/src/editors/<format>_editor.h` and `<format>_editor.cpp`
4. Call `Draw<Format>Editor(app)` from `DrawEditorHost()` in `gui/src/panels/editor_host.cpp`
5. Add the `.cpp` to `gui/CMakeLists.txt`

## Commit Messages

This project follows [Conventional Commits](https://www.conventionalcommits.org/). The
format matters because `scripts/draft-changelog.ps1` parses commit messages to
auto-populate `CHANGELOG.md` before each release.

### Format

```
<type>[(<scope>)][!]: <description>
```

### Types and changelog impact

| Type | Changelog section | Semver impact |
|---|---|---|
| `feat` | `### Added` | Minor bump |
| `fix` | `### Fixed` | Patch bump |
| `docs` | `### Changed` | — |
| `refactor`, `perf` | `### Changed` | Patch bump |
| `chore`, `ci`, `build`, `test`, `style` | *(omitted)* | — |
| `feat!` or `BREAKING CHANGE:` footer | `### Changed` (prominent) | Major bump |

### Scopes

Use a scope when the change is isolated to one component:

| Scope | Targets |
|---|---|
| `ft-lib` | `lib/` static library |
| `ft-cli` | `cli/` command-line tool |
| `ft-gui` | `gui/` GUI application |

Scoped entries get a bold qualifier in the changelog:
```markdown
- **ft-gui** Add dark/light theming toggle
```

### Cross-cutting commits

When a change genuinely spans multiple components, **omit the scope** rather than
listing multiple scopes:

```
# Good — scope omitted for a cross-cutting change
feat: add RAW to PNG conversion support

# Better when the work is separable — two focused commits
feat(ft-lib): implement RAW decoder
feat(ft-cli): add 'ft convert' subcommand for RAW to PNG
```

Prefer splitting when the components are independently releasable. Omit scope when the
change is inseparable or applies repo-wide.

### Breaking changes

Append `!` to the type/scope, or add a `BREAKING CHANGE:` footer in the commit body:

```
feat(ft-lib)!: rename Lib::extract() to Lib::unpack()
```

Breaking commits sort to the top of the changelog and signal a semver major version bump.

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
