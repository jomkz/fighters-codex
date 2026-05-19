# Fighters Toolkit (`ft`)

A native C++17 library, CLI, and GUI for reading, editing, converting, and repacking
game assets from the "Fighters" combat simulator family.

> WARNING: This project's tools are a work in progress and may not function as intended, especially in areas that are not yet fully understood. This could lead to loss of data, so back up your files first and use at your own risk!

**Supported games:**
- Jane's Fighters Anthology (1998)
- Advanced Tactical Fighters (1996) - Confirmation needed
- U.S. Navy Fighters (1994) - Confirmation needed

**Fighters Toolkit provides:**

- A **zero-dependency, statically-linked** `ft.exe` — drop it anywhere and run it; covers all the basics the original FATK handled (LIB archives, images, audio, missions, shapes, type definitions) plus dozens of formats FATK never touched
- A **graphical editor** `ft-gui.exe` — modern replacement for FATK with a live LIB browser, form-based type editors, image import/export, audio waveform playback, mission and cutscene text editing, pilot identity editing, and screenshot preview
- A **static C++ library** (`ft_lib`) — all codecs in one linkable unit; embed in any host, script via ctypes, or link from C#
- An **AI→BI compiler** (`ft ai compile`) — compile plain-text `.AI` flight-AI scripts directly to the Phar Lap PE bytecode format the game loads; the first working compiler for this format
- A **BI disassembler** (`ft bi dump`) — disassemble compiled `.BI` AI bytecode back to readable mnemonics, with cross-referenced label annotations and resolved `CALL_BY_NAME` targets
- **Screenshot and video extraction** — convert `.RAW` in-game screenshots to PNG (`ft raw unpack`) and extract individual frames from `.CB8` FMV video sequences (`ft cb8 frames`)
- **Font, HUD, sky, and palette tooling** — unpack `.FNT` bitmap fonts to individual glyph PNGs, inspect `.HUD` layout files, export `.LAY` sky-gradient strips to PNG, and dump `.PAL` palettes with colour swatches
- **Pilot save, aircraft sheet, and terrain inspection** — decode `.P` pilot save files, dump `.INF` aircraft tech sheets, and inspect `.T2` terrain map metadata
- **Symbol map and music tooling** — dump the `.SMS` symbol table used by the in-game debug overlay, and inspect `.MUS` music playlist files
- A **deep reverse-engineering documentation suite** — 44 fully-documented binary formats, plus architecture notes covering the game's runtime, asset pipeline, physics model, renderer, network protocol, and AI bytecode interpreter; all produced from scratch via binary analysis of FA.EXE and the overlay DLLs

## Why this exists

The original FATK (DuoSoft 1998) is a 16-bit app that won't run natively on 64-bit Windows and had some useability issues by modern standards.
OpenFA's `ofa-tools` is excellent but Rust-only and has a different focus.

More than anything, this project started as a vehicle to practice modern C++ — template metaprogramming, constexpr, RAII, span-based APIs, CMake tooling — on a problem domain I actually care about. Reverse-engineering how these simulators squeezed so much out of mid-90s hardware turned out to be exactly the kind of constraint-driven puzzle that makes that kind of practice enjoyable.

## Platform requirements

Both `ft.exe` and `ft-gui.exe` are **64-bit Windows binaries** and require Windows 7 or later.

Windows XP is not supported for three reasons: the build produces x64 PE only (standard XP is 32-bit); MSVC 2022+ dropped the XP-compatible toolset (`v141_xp`); and `std::filesystem` internally calls Vista-only APIs such as `GetFinalPathNameByHandleW`. Supporting XP would require downgrading to C++14, replacing `std::filesystem` with raw Win32 I/O, and using MSVC 2015 with the XP toolset — a significant regression for a negligible user base.

## Downloads

Pre-built Windows x64 binaries are on the [Releases](https://github.com/jomkz/fighters-toolkit/releases) page.

| File | For |
|------|-----|
| `ft-vX.X.X-windows-x64.zip` | Modders and scripters — unzip and run `ft.exe` from anywhere |
| `ft-gui-vX.X.X-windows-x64.zip` | Modders — unzip and run `ft-gui.exe` from anywhere |
| `ft-lib-vX.X.X-windows-x64.zip` | C++ developers — static library and headers |

## Documentation

- [docs/cli.md](docs/cli.md) — full CLI command reference with examples
- [docs/gui.md](docs/gui.md) — ft-gui graphical editor feature reference
- [docs/fa/modding.md](docs/fa/modding.md) — modding recipes (textures, stats, missions, models)
- [docs/api.md](docs/api.md) — C++ library API
- [docs/development.md](docs/development.md) — building, IDE setup, project structure
- [docs/README.md](docs/README.md) — full documentation index including 44 format specs and FA reverse-engineering notes

## Acknowledgements

Playing Jane's Fighters Anthology in the 90s was a major factor in getting me into software
development. Learning how to build basic mods for Fighters Anthology — nothing ever
released, just personal tinkering — was my first real experience of taking something
apart to understand how it worked, and that curiosity never left me.

The community of people I met through Fighters Anthology and the many hours spent flying
it will stay with me forever. The fighters-toolkit is my way of giving back, even if it
is many years later and maybe too late but we will see where it goes.

Special thanks to **[USNRaptor](http://myplace.frontier.com/~usnraptor/)**, **[The Fighters Anthology Resource Center](http://jkpeterson.net/fa/)**, and the
many others who put in the hard work to document formats, create missions, and keep the
game alive long after its time.

**[OpenFA](https://gitlab.com/openfa/openfa)** deserves a special mention — seeing that project exist and seeing someone
else care enough about these old sims to do something serious with them is part of what sparked the curiosity to build this.
The work is independent, but the motivation is the same.

## License

MIT — see [LICENSE](LICENSE).

`lib/src/blast.cpp` is based on `blast.c` by Mark Adler (zlib/libpng license).
`stb_image` and `stb_image_write` (MIT/Public Domain) are bundled in `lib/vendor/`.

The formats implemented here were determined independently by reverse engineering for interoperability.
Jane's Fighters Anthology and related titles are trademarks of their respective owners.
No copyrighted game content is included.
