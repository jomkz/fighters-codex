# Fighters Toolkit (`ft`)

A native C++17 library and CLI for reading, converting, and repacking game assets from
the Jane's combat simulator family.

**Supported games:**
- Jane's Fighters Anthology (1998)
- Advanced Tactical Fighters (1996)
- U.S. Navy Fighters (1994)

## Why this exists

The original FATK (DuoSoft 1998) is a 16-bit app that won't run natively on 64-bit Windows and had some useability issues by modern standards.
OpenFA's `ofa-tools` is excellent but Rust-only and has a different focus.

Fighters Toolkit provides:

- A **zero-dependency, statically-linked** `ft.exe` — drop it anywhere and run it
- A **static C++ library** (`ft_lib`) — embed in any GUI, script via ctypes, link from C#

More than anything, this utility serves as an archaeology tool for me to learn about how these combat flight simulators were built back when the computer's resources were much more constrained and the developers had to focus more on playability and not rely on flashy graphics. 

## Quick reference

```
ft lib   ls / unpack / pack / patch    # .LIB archive management
ft pic   info / unpack / pack          # .PIC images (dense, sparse, JPEG)
ft seq   dump / unpack / pack          # .SEQ cutscene timelines
ft audio info / unpack / pack          # .11K / .5K raw PCM audio
ft ot    info / unpack / pack          # object type definitions
ft pt    info / unpack / pack          # plane (aircraft) type definitions
ft nt / jt / see / ecm / gas  ...      # other type definitions
ft mission info / unpack / pack        # .M / .MM mission and map files
ft sh    info / unpack                 # .SH 3D shapes → Wavefront OBJ
```

## Documentation

- [docs/](docs/README.md) — file format specs and verification results
- [docs/cli.md](docs/cli.md) — full command reference with examples
- [docs/modding.md](docs/modding.md) — modding recipes (textures, stats, missions, models)
- [docs/api.md](docs/api.md) — C++ library API

## Building

Requires Visual Studio 2022 or 2026 (MSVC). CMake ships with VS but isn't in PATH:

```powershell
$cmake = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $cmake -B build -G "Visual Studio 18 2026"
& $cmake --build build --config Release
# Output: build\cli\Release\ft.exe
```

## Acknowledgements

Playing the Jane's simulations in the 90s was a major factor in getting me into software
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
