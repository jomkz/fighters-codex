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
| `FA <sub> <u32>` | 6 bytes | Setup/config; confirmed `sub` values: `0x21`, `0x32`, `0x50`, `0x19` |
| `FB <mode> <idx> F9` | 4 bytes | Play XMI track `<idx>`; confirmed `mode` values: `0x50`, `0x5A`, `0x32`, `0x19` |
| `FB <mode> <idx>` | 3 bytes | Play XMI track — short form (no `F9` terminator); appears in M_LAUNCH context |
| `FC` | 1 byte | Shuffle/loop marker; followed by state dispatch block `01 02 03 02 01 02 03 02 01` |
| `FE <u32>` | 5 bytes | Conditional branch (game-state test) |
| `FD <u24>` | 4 bytes | Loop / jump |

The `01 02 03 02 01 02 03 02 01` byte pattern immediately following `FC` is a **state machine dispatch table** — the same pattern appears in DLG CODE sections just before JMP thunks, identifying it as a shared engine construct.

### All 9 playlists decoded

| File | Playlist ID | Track count | Notes |
|------|-------------|-------------|-------|
| `M_AIR.MUS` | `"air"` | 20 | In-flight music; two groups separated by `FE` conditional |
| `M_DECK.MUS` | `"deck"` | ~8 | Carrier deck / on-ground state |
| `M_LAUNCH.MUS` | `"launch"` | ~4 | Launch sequence; uses 3-byte `FB` form |
| `M_VALK.MUS` | `"valk"` | ~6 | Valkyrie / dogfight state |
| `M_EJECT.MUS` | `"eject"` | 1–2 | Ejection event — minimal (1–2 opcodes) |
| `M_SUCC.MUS` | `"succ"` | 1–2 | Mission success — minimal |
| `M_HOME.MUS` | `"home"` | 1–2 | Return to base — minimal |
| `M_BRIEF.MUS` | `"brief"` | ~5 | Mission briefing screen |
| `M_MENU.MUS` | `"menu"` | ~6 | Main menu |

### M_AIR.MUS decoded (detailed)

Playlist ID: `"air"` (in-flight music)

Setup opcodes:
- `FA 21 0x48` — likely fade-in time
- `FA 21 0x7E` — likely fade-out time
- `FA 32 0x30` — likely tempo/volume

XMI track sequence (20 tracks): `4 6 107 108 109 18 110 116 117 118 119 24 29 21 121 122 123 125 126 127`

One `FE 0x48` conditional separates the sequence into two groups, likely switching between low-intensity and high-intensity music.

## Toolkit Roadmap

- New `cli/cmd_mus.cpp` — `ft mus dump <file.MUS>` prints decoded opcode stream and XMI track list
- No lib codec needed — MUS is pure bytecode; the dump walks the opcode stream

## TODO — Deep Dive

- Decode `FA` sub-opcode meanings (volume, fade, tempo) — confirmed sub-values: `0x19`, `0x21`, `0x32`, `0x50`; semantics need FA.SMS symbol cross-reference
- Map XMI track indices to file names by cross-referencing FA_2.LIB insertion order (`ft lib ls FA_2.LIB` sorted)
- Decode `FE`/`FD` branch conditions — argument likely encodes game-state enum; cross-reference FA.SMS
- Clarify `FB` 3-byte vs 4-byte form — does mode byte determine terminator presence?

## Related

- [XMI.md](XMI.md) — Extended MIDI audio tracks played by the music system
- [SEQ.md](SEQ.md) — cutscene sequencer, which may also trigger music state changes
