---
format: MUS
name: Music Playlist Sequencer
extensions: [".MUS"]
category: audio
endianness: little
spec:
  status: partial
  gaps:
    - kind: re-static
      issue: 54
      note: "FA/FB sub-opcode semantics are Miles-internal; WAIL32.DLL untraced"
codec:
  direction: read
  rationale: "the .MUS is a compiled PE/LE DLL consumed by Miles at runtime; the sequencer bytecode has no authoring format and the FA/FB sub-opcodes are Miles-internal (#54). Music is modded by replacing the referenced XMI tracks (§ Replacing in-game music), not by re-emitting the DLL, so the disassembler is read-only (round-trip decision, #101)"
  lib: [lib/src/mus.cpp]
  commands: [mus]
  tests: [tests/test_mus.cpp, tests/test_overlays.cpp]
  fuzz: [fuzz/fuzz_mus.cpp]
  gui: [gui/src/editors/mus_editor.cpp]
  fixtures:
    synthetic: true
    real_manifest: true
    real_install: true
related: [XMI, SEQ, 11K]
---

# MUS — Music Playlist Sequencer (.MUS)

FA_2.LIB contains 9 `.MUS` files (e.g. `M_AIR.MUS`). These control background
music playback. Each is a **Win32 PE DLL** loaded at runtime whose CODE section
holds a **bytecode script** (not compiled x86) that sequences `.XMI` track
playback; the `.XMI` files in FA_2.LIB are the actual audio.

## Tools

### fx

```
fx mus dump <file.MUS>    # raw opcode stream; FB <idx> resolved to XMI filenames
```

The disassembler is `fx::mus_disassemble` in `lib/src/mus.cpp`
([api.md](../../api.md) § mus.h); the `fx mus dump` CLI and the fxs music
editor are thin consumers of it. Direction is **read** only: the MUS CODE
section is a compiled DLL consumed by Miles as-is, with no authoring format
to write back — music is modded by swapping the referenced XMI tracks
(§ Replacing in-game music), not by re-emitting the DLL (round-trip
decision resolved in #101; see the front-matter rationale).

## File Layout

All multi-byte integers are little-endian.

Each file contains a standard **DOS MZ stub** (128 bytes) followed by a
**Phar Lap PE header** (`PL\0\0`) at offset `0x80` (pointed to by the `u32` at
`0x3C`). The bytecode CODE section begins at offset `0x200`. All observed
`.MUS` files decompress to **4608 bytes**. String analysis of `M_AIR.MUS`
yields only the standard PE header strings — no embedded `.XMI` track names
are visible as plain text, confirming XMI references are encoded as integer
indices resolved at runtime.

### Bytecode Script Format — confirmed

| Opcode | Length | Meaning |
|--------|--------|---------|
| `FF <name\0>` | variable | Playlist identifier string (e.g. `"air"`) |
| `FA <sub> <u32>` | 6 bytes | Setup/config; confirmed `sub` values: `0x21`, `0x32`, `0x50`, `0x19` |
| `FB <mode> <idx> F9` | 4 bytes | Play XMI track `<idx>`; confirmed `mode` values: `0x50`, `0x5A`, `0x32`, `0x19` |
| `FB <mode> <idx>` | 3 bytes | Play XMI track — short form (no `F9` terminator); appears in M_LAUNCH context |
| `FC` | 1 byte | Shuffle/loop marker; followed by state dispatch block `01 02 03 02 01 02 03 02 01` |
| `FE <u32>` | 5 bytes | Conditional branch (game-state test) |
| `FD <n> <n bytes>` | 2 + n | Track list: a **count-prefixed** run of `n` XMI track indices |
| `<idx> [F9]` | 1–2 bytes | A bare track index, played under the mode the last `FB` set — MIDI-style **running status** |

The `01 02 03 02 01 02 03 02 01` byte pattern immediately following `FC` is a
**state machine dispatch table** — the same pattern appears in DLG CODE
sections just before JMP thunks, identifying it as a shared engine construct.

### XMI Track Index Mapping

XMI track index `N` maps to file `AIRnnn.XMI`, where `nnn` is the zero-padded
decimal value of `N`:

```
index  1 → VALK01.XMI
index  3 → AIR003.XMI
index  4 → AIR004.XMI
…
index 127 → AIR127.XMI
```

The index space is **sparse** — many slots have no corresponding file in
FA_2.LIB (e.g. indices 2, 8, 10–12, 15, 17, 20, 27, 29, 30, 32–37, etc.). The
numeric suffix in the filename IS the track index; `VALK01.XMI` is the sole
exception to the `AIRnnn` naming pattern and occupies index 1.

## File Inventory

The game has nine fixed music slots. Playlist decodes below are from
`fx mus dump` over all 9 files:

| File | Trigger | Playlist ID | Track indices | Notes |
|------|---------|-------------|--------------|-------|
| `M_AIR.MUS` | Dogfight | `"air"` | 4 6 107 108 109 18 110 116 117 118 119 24 **29** 21 121 122 123 125 126 127 \| 9 38 62 65 67 19 | 26 tracks in two groups split by `FE` conditional; index 29 has no file in FA_2.LIB |
| `M_NORMAL.MUS` | Normal flight | `"air"` | 14 70 71 72 73 74 47 61 40 75 76 77 78 44 4 39 22 28 48 40 80 81 82 83 84 26 19 43 85 86 87 88 89 46 13 23 90 91 92 94 7 9 31 38 44 | 45 tracks; longest playlist |
| `M_DANGER.MUS` | Enemy detected | `"air"` | 100 101 13 48 47 61 28 39 46 43 **41** 26 102 5 18 4 45 104 19 23 21 6 | 22 tracks; index 41 has no file in FA_2.LIB |
| `M_VALK.MUS` | Ctrl+V hidden track | `"valk"` | 1 | 1 track → VALK01.XMI; valkyrie/dogfight state |
| `M_DECK.MUS` | On the deck | `"air"` | 14 13 43 | 3 tracks; carrier deck state |
| `M_HOME.MUS` | Almost home | `"air"` | 25 26 40 | 3 tracks; return-to-base |
| `M_LAUNCH.MUS` | Takeoff | `"air"` | 7 9 44 31 38 44 | 6 tracks (44 repeated); uses 3-byte `FB` form |
| `M_EJECT.MUS` | Ejected | `"air"` | *(none)* | No `FB` opcodes; contains only `FD`/`FE` control flow — eject event redirects state rather than starting a new track |
| `M_SUCC.MUS` | Success | `"air"` | *(none)* | No `FB` opcodes; mission-success event is state control only |

(The trigger column comes from the community TOOLKIT documentation of the nine
slots; an earlier account that these slots point at `.11K` files was wrong —
the confirmed decode below shows XMI playback via Miles.)

### M_AIR.MUS decoded (detailed)

Playlist ID: `"air"` (in-flight music); CODE section at file offset `0x200`.

Setup opcodes:
- `FA 21 0x48` — likely fade-in time
- `FA 21 0x7E` — likely fade-out time
- `FA 32 0x30` — likely tempo/volume

Group 1 (low-intensity, 20 tracks):
`4 6 107 108 109 18 110 116 117 118 119 24 29 21 121 122 123 125 126 127`

`FE 0x48` conditional (game-state branch)

Group 2 (high-intensity, 6 tracks): `9 38 62 65 67 19`

### Replacing in-game music (community workflow)

1. Author or convert a replacement `.XMI` (see [XMI.md](XMI.md)) and name it
   for the target track index (`AIRnnn.XMI`).
2. Patch it into FA_2.LIB with `fx lib patch`.
3. To change *which* tracks a slot plays, edit the slot's `FB` opcodes and
   patch the `.MUS` back in; `fx mus dump` verifies the result.

## Engine Notes

### Playback Architecture — confirmed via Ghidra

Traced from `_SEQmusic` (`0x00446B70`), `?MusicOn` (`0x004329E0`),
`?MusicVolume` (`0x00432B40`) in the game executable.

```
_SEQmusic(name, seq_idx)
  → appends name to base path (DAT_004f4f6c) to form "M_AIR.MUS" etc.
  → calls MusicOn(filename, seq_idx)
      → RMAccess(filename, 0x10c)   — loads MUS DLL from LIB archive
      → _AIL_allocate_sequence_handle   — allocate Miles Sound System handle
      → _AIL_init_sequence(handle, mus_data, seq_idx)  — pass MUS CODE section to AIL
      → _AIL_start_sequence(handle)     — begin playback
```

**The MUS CODE section is passed directly to the Miles Sound System (AIL).**
FA does not interpret the FA/FB/FC/FD/FE bytes itself — Miles processes them
natively as XMIDI or MSS sequence data. The sub-opcode semantics (`FA 0x19`,
`FA 0x21`, etc.) are Miles-internal and cannot be decoded from the game executable alone.

**Volume:** `?MusicVolume(vol)` maps the 0–100 game volume scale to AIL's
0–127 range: `AIL_set_XMIDI_master_volume(handle, vol * 127 / 100)`.

**`_SEQfadein` / `_SEQfadeout`** (`0x00446890` / `0x00446910`) are **palette
(screen) fades**, not music fades. They operate on a 768-byte RGB palette
table (256 × 3 bytes at `curPalette`). They are unrelated to MUS audio.

**`seq_idx` parameter:** the `short param_2` passed through `_SEQmusic` →
`MusicOn` → `_AIL_init_sequence` is the **AIL sequence index** — which section
of the XMIDI data to start playback from. Normally 0 (first sequence).

## Open Questions

### 1. FA/FB sub-opcode semantics

The sub-opcode values (`FA 0x19`, `FA 0x21`, `FB` mode bytes, the `FC` state
dispatch) are consumed by Miles, not the game executable — decoding them requires tracing
the AIL wrapper (WAIL32.DLL), whose import surface is untraced, or consulting
Miles Sound System XMIDI documentation.

*Status: open — re-static (#54)*

## Related

**Formats:** [XMI](XMI.md) — the Extended MIDI tracks these playlists select;
[SEQ](SEQ.md) — the cutscene sequencer whose `_SEQmusic` path triggers these
slots; [11K](11K.md) — PCM sound effects (a music slot does *not* reference
these — see the correction note there).

### `FD` is a list, not a jump target (#491)

This table used to read `FD <u24> — 4 bytes — loop / jump`, and `lib/src/mus.cpp` was
written from it. The retail bytes say otherwise — the operand is a **count** followed by
that many track indices:

```
M_AIR    fd 02 07 1f              n=2
M_EJECT  fd 03 0f 16 27           n=3
M_SUCC   fd 05 03 0b 14 31 10     n=5
```

Reading a fixed 3-byte operand consumed the count plus two entries and then landed
mid-list, so the disassembler aborted on a track index it mistook for an opcode: **3 of
the 9 shipped playlists** (`M_AIR`, `M_EJECT`, `M_SUCC`) stopped early and lost their
tail. `M_AIR` also ends with bare indices under the last `FB` mode
(`fb 50 13 f9 | 17 f9 | 15 f9 | 06`) — the running-status form above.

The oracle is that a playlist ends at its dispatch table and zero padding; all nine now
disassemble to that boundary (`tests/test_mus.cpp`, real-asset mode).
