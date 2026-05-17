# Music Playlist / Sequencer (.MUS)

FA_2.LIB contains 9 `.MUS` files (e.g. `M_AIR.MUS`). These control background music playback. Each is a **Win32 PE DLL** loaded at runtime via `LoadLibrary`.

## Format

Win32 PE DLL. All observed `.MUS` files decompressed to **4608 bytes**. String analysis of `M_AIR.MUS` yields only the standard PE header strings — no embedded `.XMI` track names are visible as plain text, suggesting XMI references are encoded or resolved at runtime by the engine rather than embedded in the DLL data section.

The `.XMI` files in FA_2.LIB are the actual audio sequences. Each `.MUS` file is a **bytecode script** (not compiled x86 code) that sequences XMI track playback. XMI indices in the observed files exceed 78 — at least 127 unique track indices were seen across the 9 MUS files.

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 9 |

## Bytecode Script Format (Confirmed)

MUS files use **Phar Lap PE format** (signature `PL\0\0`). The CODE section is a bytecode script — no imports, no x86 code. The engine interprets the opcodes directly.

### Opcode table

| Opcode | Length | Meaning |
|--------|--------|---------|
| `FF <name\0>` | variable | Playlist identifier string (e.g. `"air"`) |
| `FA <sub> <u32>` | 6 bytes | Setup/config (volume, tempo, fade; `sub` selects parameter) |
| `FB 50 <idx> F9` | 4 bytes | Play XMI track `<idx>` |
| `FE <u32>` | 5 bytes | Conditional branch (game-state test) |
| `FD <u24>` | 4 bytes | Loop / jump |

### M_AIR.MUS decoded

Playlist ID: `"air"` (in-flight music)

Setup opcodes:
- `FA 21 0x48` — likely fade-in time
- `FA 21 0x7E` — likely fade-out time
- `FA 32 0x30` — likely tempo/volume

XMI track sequence (20 tracks): `4 6 107 108 109 18 110 116 117 118 119 24 29 21 121 122 123 125 126 127`

One `FE 0x48` conditional separates the sequence into two groups, likely switching between low-intensity and high-intensity music.

### RE next steps

1. Decode `FA` sub-opcodes by comparing against audio parameter symbols in FA.SMS (search for `MUS`, `fade`, `volume`).
2. Map XMI indices to file names: `ft lib ls FA_2.LIB | grep .XMI` and sort by insertion order to get index 0=first, 1=second, etc.
3. Decode `FE`/`FD` branching — the argument likely encodes a game-state enum value. Cross-reference with FA.SMS symbols.
4. Decode all 9 MUS files and compare track sets across game states.

## Toolkit Roadmap

- New `cli/cmd_mus.cpp` — `ft mus dump <file.MUS>` prints decoded opcode stream and XMI track list
- No lib codec needed — MUS is pure bytecode; the dump walks the opcode stream

## TODO — Deep Dive

- Decode `FA` sub-opcode meanings (volume, fade, tempo)
- Map XMI track indices to file names by cross-referencing FA_2.LIB insertion order
- Map all 9 MUS playlists to their corresponding game states
- Decode `FE`/`FD` branch conditions

## Related

- [XMI.md](XMI.md) — Extended MIDI audio tracks played by the music system
- [SEQ.md](SEQ.md) — cutscene sequencer, which may also trigger music state changes
