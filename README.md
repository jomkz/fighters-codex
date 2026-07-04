# Fighters Codex (`fx`)

[![CI](https://github.com/jomkz/fighters-codex/actions/workflows/ci.yml/badge.svg)](https://github.com/jomkz/fighters-codex/actions/workflows/ci.yml)
[![CodeQL](https://github.com/jomkz/fighters-codex/actions/workflows/codeql.yml/badge.svg)](https://github.com/jomkz/fighters-codex/actions/workflows/codeql.yml)
[![Docs](https://github.com/jomkz/fighters-codex/actions/workflows/docs.yml/badge.svg)](https://github.com/jomkz/fighters-codex/actions/workflows/docs.yml)
[![Release](https://img.shields.io/github/v/release/jomkz/fighters-codex)](https://github.com/jomkz/fighters-codex/releases)

A reverse-engineering effort to fully understand and document the game engine and file
formats of the "Fighters" combat simulator family. The documentation — format specs,
architecture notes, recovered symbols — is the primary output. The `fx_lib` library,
`fx` CLI, and `fx-gui` are the validation layer: a working, byte-identical codec
implementation is the proof that a format is truly understood.

> **Note:** The tools are validation artifacts for ongoing reverse-engineering work. Areas still being documented may have incomplete or incorrect implementations. Back up your files before using any write operations.

**Supported games:**
- Jane's Fighters Anthology (1998)
- Advanced Tactical Fighters (1996) - Confirmation needed ([verification pass planned](https://github.com/jomkz/fighters-codex/issues/57))
- U.S. Navy Fighters (1994) - Confirmation needed ([verification pass planned](https://github.com/jomkz/fighters-codex/issues/57))

**Documentation and research:**

- **44 documented binary and text formats** — every file type in the game, reverse-engineered from scratch via binary analysis; per-format completeness, codec direction, and test coverage are tracked in the CI-enforced [status matrix](docs/fa/formats/STATUS.md); see [docs/fa/formats/](docs/fa/formats/README.md)
- **Engine architecture notes** — runtime environment, asset pipeline, physics model, renderer, AI bytecode interpreter, network protocol, and Win32 overlay DLL system; see [docs/fa/architecture.md](docs/fa/architecture.md)
- **3,829 recovered C++ symbols** from the game executable and all overlay DLLs — organized by subsystem with demangled names and virtual addresses; see [docs/fa/symbols.md](docs/fa/symbols.md)
- **Game-executable reconstruction program** — a machine-readable symbol database and per-subsystem docs that name and document every function and variable, subsystem by subsystem; progress is tracked in the CI-enforced [reconstruction matrix](docs/fa/reconstruction.md)
- **Modding guides** — step-by-step recipes for textures, aircraft stats, missions, audio, and more; see [docs/fa/modding.md](docs/fa/modding.md)

**Validation tools** *(proving the documentation by implementing it)*:

- A **zero-dependency, statically-linked** `fx.exe` — byte-identical round-trip for the LIB, PAL, SEQ, audio, BRF-family, and mission codecs (PIC re-encodes losslessly for the engine but not yet byte-exact — [#175](https://github.com/jomkz/fighters-codex/issues/175)); inspect/decode support for most other documented formats. Closing the round-trip gap for every format (or documenting why a codec is one-way) is [Phase 4 of the roadmap](docs/roadmap.md)
- A **graphical editor** `fx-gui` (Linux and Windows) — interactive codec validation against real game data; live LIB browser, form-based type editors, image import/export, audio waveform playback, mission and cutscene text editing, pilot identity editing, and screenshot preview
- A **static C++ library** (`fx_lib`) — all codecs in one linkable unit, embeddable in any C++ host (it is what [fa-bridge](https://github.com/fighters-legacy/fa-bridge) builds on)
- An **AI→BI compiler** (`fx ai compile`) — the first working compiler for the Phar Lap PE bytecode format the game's AI interpreter loads; validates the complete AI bytecode spec
- A **BI disassembler** (`fx bi dump`) — disassemble compiled `.BI` AI bytecode back to readable mnemonics, with cross-referenced label annotations and resolved `CALL_BY_NAME` targets

## Why this exists

The goal is a complete, accurate record of how the Fighters Anthology engine works — every format, every subsystem, every recoverable symbol — produced from scratch via binary analysis. Not to replace any existing tool, but to understand the thing deeply enough to document it properly. The lib, CLI, and GUI are how that understanding gets verified: if you can implement a working, byte-identical codec, you genuinely understand the format.

The original FATK (DuoSoft 1998) is a 16-bit app that won't run natively on 64-bit Windows; **[OpenFA](https://gitlab.com/openfa/openfa)** is excellent but has a different focus. Neither is the reason this project exists — they're context.

Reverse-engineering how these simulators squeezed so much out of mid-90s hardware turned out to be exactly the kind of constraint-driven puzzle that makes the work enjoyable. Building the tooling in modern C++ — template metaprogramming, constexpr, RAII, span-based APIs — on a problem domain I actually care about was a bonus, not the point.

## Platform requirements

Pre-built releases are **64-bit Windows binaries** (Windows 7 or later; `fx-gui` needs OpenGL 3.3, so Vista-era GPUs or newer) and **Linux x64 tarballs** (glibc 2.35+ — Ubuntu 22.04, Fedora 36, or newer equivalents; libstdc++ is statically linked). Starting with v0.5.0 all three components ship for both OSes — `fx-gui` runs natively on Linux and Windows via SDL3 + OpenGL 3.3 ([Phase 3 of the roadmap](docs/roadmap.md)); earlier Linux releases (v0.4.0) carried `fx` and `fx_lib` only. Everything also builds from source on Linux with GCC or Clang (see [docs/development.md](docs/development.md)).

Windows XP is not supported for three reasons: the build produces x64 PE only (standard XP is 32-bit); MSVC 2022+ dropped the XP-compatible toolset (`v141_xp`); and `std::filesystem` internally calls Vista-only APIs such as `GetFinalPathNameByHandleW`. Supporting XP would require downgrading to C++14, replacing `std::filesystem` with raw Win32 I/O, and using MSVC 2015 with the XP toolset — a significant regression for a negligible user base.

## Downloads

Pre-built binaries are on the [Releases](https://github.com/jomkz/fighters-codex/releases) page.

| File | For |
|------|-----|
| `fx-vX.X.X-windows-x64.zip` | Modders and scripters — unzip and run `fx.exe` from anywhere |
| `fx-gui-vX.X.X-windows-x64.zip` | Modders — unzip and run `fx-gui.exe` from anywhere |
| `fx-lib-vX.X.X-windows-x64.zip` | C++ developers — static library and headers |
| `fx-vX.X.X-linux-x64.tar.gz` (v0.4.0+) | Modders and scripters on Linux — untar and run `fx` from anywhere |
| `fx-gui-vX.X.X-linux-x64.tar.gz` (v0.5.0+) | Modders on Linux — untar and run `fx-gui` from anywhere |
| `fx-lib-vX.X.X-linux-x64.tar.gz` (v0.4.0+) | C++ developers on Linux — static library and headers |

## Documentation

All documentation is published as a browsable site with navigation and search:
**<https://jomkz.github.io/fighters-codex/>**. The same content is readable
in-repo:

- [docs/roadmap.md](docs/roadmap.md) — phased roadmap to 1.0, with gates and epic index
- [docs/cli.md](docs/cli.md) — full CLI command reference with examples
- [docs/gui.md](docs/gui.md) — fx-gui graphical editor feature reference
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
it will stay with me forever. The fighters-codex is my way of giving back, even if it
is many years later and maybe too late but we will see where it goes.

Special thanks to **[USNRaptor](http://myplace.frontier.com/~usnraptor/)**, **[The Fighters Anthology Resource Center](http://jkpeterson.net/fa/)**, and the
many others who put in the hard work to document formats, create missions, and keep the
game alive long after its time.

**[OpenFA](https://gitlab.com/openfa/openfa)** deserves a special mention — the work is independent, but the motivation is the same.

## License

MIT — see [LICENSE](LICENSE).

`lib/src/blast.cpp` is based on `blast.c` by Mark Adler (zlib/libpng license).
`stb_image` and `stb_image_write` (MIT/Public Domain) are bundled in `lib/vendor/`.

The formats implemented here were determined independently by reverse engineering for interoperability.
Jane's Fighters Anthology and related titles are trademarks of their respective owners.
No copyrighted game content is included.
