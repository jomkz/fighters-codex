# Music Playlist / Sequencer (.MUS)

FA_2.LIB contains 9 `.MUS` files (e.g. `M_AIR.MUS`). These control background music playback. Each is a **Win32 PE DLL** loaded at runtime via `LoadLibrary`.

## Format

Win32 PE DLL. All observed `.MUS` files decompress to **4608 bytes**. String analysis of `M_AIR.MUS` yields only the standard PE header strings — no embedded `.XMI` track names are visible as plain text, confirming XMI references are encoded as integer indices resolved at runtime.

The `.XMI` files in FA_2.LIB are the actual audio sequences. Each `.MUS` file is a **bytecode script** (not compiled x86 code) that sequences XMI track playback.

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 9 |

## File Structure

Each file contains a standard **DOS MZ stub** (128 bytes) followed by a **Phar Lap PE header** (`PL\0\0`) at offset `0x80` (pointed to by the `u32` at `0x3C`). The bytecode CODE section begins at offset `0x200`.

## XMI Track Index Mapping

XMI track index `N` maps to file `AIRnnn.XMI`, where `nnn` is the zero-padded decimal value of `N`:

```
index  1 → VALK01.XMI
index  3 → AIR003.XMI
index  4 → AIR004.XMI
…
index 127 → AIR127.XMI
```

The index space is **sparse** — many slots have no corresponding file in FA_2.LIB (e.g. indices 2, 8, 10–12, 15, 17, 20, 27, 29, 30, 32–37, etc.). The numeric suffix in the filename IS the track index; `VALK01.XMI` is the sole exception to the `AIRnnn` naming pattern and occupies index 1.

All 78 XMI files are in FA_2.LIB only (FA_1.LIB contains none; FA_3.LIB is disc-2 content).

## Bytecode Script Format (Confirmed)

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

| File | Playlist ID | Track indices | Notes |
|------|-------------|--------------|-------|
| `M_AIR.MUS` | `"air"` | 4 6 107 108 109 18 110 116 117 118 119 24 **29** 21 121 122 123 125 126 127 \| 9 38 62 65 67 19 | 26 tracks in two groups split by `FE` conditional; index 29 has no file in FA_2.LIB |
| `M_NORMAL.MUS` | `"normal"` | 14 70 71 72 73 74 47 61 40 75 76 77 78 44 4 39 22 28 48 40 80 81 82 83 84 26 19 43 85 86 87 88 89 46 13 23 90 91 92 94 7 9 31 38 44 | 45 tracks; longest playlist |
| `M_DANGER.MUS` | `"danger"` | 100 101 13 48 47 61 28 39 46 43 **41** 26 102 5 18 4 45 104 19 23 21 6 | 22 tracks; index 41 has no file in FA_2.LIB |
| `M_VALK.MUS` | `"valk"` | 1 | 1 track → VALK01.XMI; valkyrie/dogfight state |
| `M_DECK.MUS` | `"deck"` | 14 13 43 | 3 tracks; carrier deck state |
| `M_HOME.MUS` | `"home"` | 25 26 40 | 3 tracks; return-to-base |
| `M_LAUNCH.MUS` | `"launch"` | 7 9 44 31 38 44 | 6 tracks (44 repeated); uses 3-byte `FB` form |
| `M_EJECT.MUS` | `"air"` | *(none)* | No `FB` opcodes; contains only `FD`/`FE` control flow — eject event redirects state rather than starting a new track |
| `M_SUCC.MUS` | `"succ"` | *(none)* | No `FB` opcodes; mission-success event is state control only |

### M_AIR.MUS decoded (detailed)

Playlist ID: `"air"` (in-flight music); CODE section at file offset `0x200`.

Setup opcodes:
- `FA 21 0x48` — likely fade-in time
- `FA 21 0x7E` — likely fade-out time
- `FA 32 0x30` — likely tempo/volume

Group 1 (low-intensity, 20 tracks): `4 6 107 108 109 18 110 116 117 118 119 24 29 21 121 122 123 125 126 127`

`FE 0x48` conditional (game-state branch)

Group 2 (high-intensity, 6 tracks): `9 38 62 65 67 19`

## Toolkit Roadmap

- New `cli/cmd_mus.cpp` — `ft mus dump <file.MUS>` prints decoded opcode stream and resolves `<idx>` values to XMI filenames using the index rule above
- No lib codec needed — MUS is pure bytecode; the dump walks the opcode stream

## TODO — Deep Dive

- Decode `FA` sub-opcode meanings (volume, fade, tempo) — confirmed sub-values: `0x19`, `0x21`, `0x32`, `0x50`; semantics need FA.SMS symbol cross-reference
- Decode `FE`/`FD` branch conditions — argument likely encodes game-state enum; cross-reference FA.SMS
- Clarify `FB` 3-byte vs 4-byte form — does mode byte determine terminator presence?

## Related

- [XMI.md](XMI.md) — Extended MIDI audio tracks played by the music system
- [SEQ.md](SEQ.md) — cutscene sequencer, which may also trigger music state changes
