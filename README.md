# Fighters Toolkit (`ft`)

A native C++17 library, CLI, and GUI for reading, editing, converting, and repacking
game assets from the "Fighters" combat simulator family.

**Supported games:**
- Jane's Fighters Anthology (1998)
- Advanced Tactical Fighters (1996)
- U.S. Navy Fighters (1994)

> WARNING: This project's tools are a work in progress and may not function as intended, especially in areas that are not yet fully understood. This could lead to loss of data, so back up your files first and use at your own risk!

## Why this exists

The original FATK (DuoSoft 1998) is a 16-bit app that won't run natively on 64-bit Windows and had some useability issues by modern standards.
OpenFA's `ofa-tools` is excellent but Rust-only and has a different focus.

Fighters Toolkit provides:

- A **zero-dependency, statically-linked** `ft.exe` — drop it anywhere and run it; covers LIB archive management, image/audio/mission/shape conversion, and type-definition inspection from the command line
- A **graphical editor** `ft-gui.exe` — modern replacement for FATK with a live LIB browser, form-based type editors, image import/export, audio waveform playback, mission and cutscene text editing, pilot identity editing, and screenshot preview
- A **static C++ library** (`ft_lib`) — all codecs in one linkable unit; embed in any host, script via ctypes, or link from C#

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

- [docs/](docs/README.md) — file format specs and verification results
- [docs/cli.md](docs/cli.md) — full CLI command reference with examples
- [docs/gui.md](docs/gui.md) — ft-gui graphical editor feature reference
- [docs/modding.md](docs/modding.md) — modding recipes (textures, stats, missions, models)
- [docs/api.md](docs/api.md) — C++ library API
- [docs/development.md](docs/development.md) — building, IDE setup, project structure

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
game alive long after its time. That body of community knowledge is what the
**[OpenFA](https://gitlab.com/openfa/openfa)** project built upon to
produce the definitive reverse-engineering reference that fighters-toolkit used to get started.
All credit for the original reverse engineering goes to OpenFA and the
generous community that came before it.

## License

MIT — see [LICENSE](LICENSE).

`lib/src/blast.cpp` is based on `blast.c` by Mark Adler (zlib/libpng license).
`stb_image` and `stb_image_write` (MIT/Public Domain) are bundled in `lib/vendor/`.

The formats implemented here were determined by reverse engineering for interoperability,
with significant reference to OpenFA (GPLv3). Jane's Fighters Anthology and related titles
are trademarks of their respective owners. No copyrighted game content is included.
