---
format: XMI
name: Extended MIDI Audio
extensions: [".XMI"]
category: audio
endianness: big
spec:
  status: partial
  gaps:
    - kind: re-static
      issue: 54
      note: "AIL tempo-multiplier scaling and TIMB entry-field semantics unverified"
codec:
  direction: read
  rationale: "XMI→MID is a one-way format translation, not a round-trip back to XMI: the exporter reads the IFF/EVNT stream and emits a Standard MIDI File (AIL delays → SMF deltas, note durations → note-offs). Re-encoding MID→XMI would be a distinct converter, not an identity, so byte-round-trip does not apply."
  lib: [lib/src/xmi.cpp]
  commands: [xmi]
  tests: [tests/test_xmi.cpp]
  fuzz: [fuzz/fuzz_xmi.cpp]
  gui: [gui/src/editors/xmi_editor.cpp]
  fixtures:
    synthetic: true
    real_manifest: true
    real_install: true
related: [MUS, 11K]
---

# XMI — Extended MIDI Audio (.XMI)

FA_2.LIB contains 78 `.XMI` files. XMI is Miles Sound System's Extended MIDI
format — a compact IFF-based variant of Standard MIDI that supports multiple
independent sequences in a single file. Confirmed by hex analysis: magic
`FORM...XDIR`. All 78 are in FA_2.LIB only (FA_1.LIB contains none; FA_3.LIB
is disc-2 content).

## Tools

### fx

```
fx xmi info   <file.XMI>                     # sequences, timbres, chunk inventory
fx xmi export <file.XMI> [-s N] -o out.mid   # sequence N (default 0) -> Standard MIDI
```

`fx xmi export` writes a Standard MIDI File (format 0) from the selected
sequence: the AIL delay encoding is rewritten to SMF variable-length deltas
and each note-on's XMI duration becomes a scheduled note-off. XMI→MID is a
one-way translation, not a byte-identity round-trip. All 78 stock `.XMI`
files export to structurally valid SMF (balanced note-on/note-off pairs,
monotonic deltas, proper end-of-track); the output parses in independent
MIDI libraries.

## File Layout

IFF-style chunk structure — **chunk sizes are big-endian**, unlike every other
FA format. Well-documented external format.

| Offset | Chunk | Description |
|--------|-------|-------------|
| `0x00` | FORM | IFF outer envelope |
| `0x04` | (size) | u32 BE: total content size |
| `0x08` | XDIR | Extended MIDI directory marker |
| `0x0C` | INFO | Sequence count block |
| — | CAT | Sequence catalog |
| — | FORM XMID | One entry per MIDI sequence |
| — | TIMB | Instrument/timbre table |
| — | EVNT | MIDI event stream (AIL delta encoding) |

Key differences from Standard MIDI:
- Fixed 120 BPM base; tempo encoded as AIL-specific multipliers
- Delta times use AIL's variable-length encoding (not SMF)
- The format allows multiple sequences per file via the `CAT`/`XMID` structure. **FA never
  uses it**: all 78 shipped files declare `INFO` = 1 and carry exactly one `FORM XMID`, whose
  only chunks are `TIMB` and `EVNT`. The multi-sequence path is exercised by the synthetic
  fixtures alone.

### TIMB chunk (instrument table)

`u16` entry count (little-endian), then `count × 2` bytes — one `(patch,
bank)` pair per timbre the sequence uses. AIR003.XMI carries 18 entries in a
38-byte chunk (`2 + 18×2`). The per-entry field roles beyond patch/bank are
not independently verified here (see Open Questions).

### EVNT chunk (event stream) — decoded and validated

The event stream is Standard-MIDI status bytes with two AIL differences,
recovered from the 78-file corpus:

- **Delay (interval) encoding.** Between events, every byte `< 0x80`
  accumulates into the delay for the next event (a sum-of-bytes VLQ, *not*
  the SMF 7-bit VLQ). A delay of 256 ticks encodes as `7F 7F 02`. The first
  byte `≥ 0x80` ends the delay and is the event's status byte.
- **Note-on carries an explicit duration.** A `0x9n` note-on is followed by
  note, velocity, and a **duration** as a standard 7-bit VLQ; there is no
  matching note-off in the stream. The exporter emits the note-on at the
  current tick and schedules a note-off `duration` ticks later.
- **All other events are Standard MIDI**, with the usual data-byte counts:
  `0x8n`/`0xAn`/`0xBn`/`0xEn` (two data bytes), `0xCn`/`0xDn` (one),
  `0xF0`/`0xF7` sysex (SMF-VLQ length + data), and `0xFF` meta (type +
  SMF-VLQ length + data). `FF 2F` ends the sequence. Status bytes are always
  explicit — running status cannot occur because data bytes (`< 0x80`) would
  be consumed as delay.

## Round-Trip Notes

**There is no round-trip here to lean on.** XMI→MID is a one-way translation, so the usual
proof-by-byte-identity does not apply — and that makes the failure mode worse, not better. The
event loop stops on anything it cannot read (a truncated event, an unknown status byte) rather
than desync. That is the right call, but a stream it gives up on halfway still produces a
**perfectly well-formed SMF — just a shorter one, silently**. Nothing distinguishes *finished*
from *stopped*.

So the decoder now says which (`XmiDecode`), and `tests/test_xmi.cpp` asserts it over **all 78
shipped files** (under `FX_FA_ROOT`): each one's `INFO` count matches the sequences actually
walked, and every `EVNT` stream is decoded **to its explicit `FF 2F` end-of-track**, consuming
the whole chunk but for the IFF pad byte. **218,254 events** decode across the corpus. The
emitted SMF is checked structurally too — header, one track, and a track-length field that
agrees with the bytes after it.

All 78 pass. Unlike the other formats audited under
[#491](https://github.com/jomkz/fighters-codex/issues/491), **no decode bug was found here** —
the codec was already right. The census is what makes that a fact rather than an assumption.

## Open Questions

### 1. AIL tempo scaling and TIMB entry semantics

Two residual details are externally documented (Miles Sound System) but not
independently verified against FA's files here: the exact mapping from AIL's
tempo multipliers to microseconds-per-quarter-note (the exporter emits a
default 120 BPM and passes any in-stream `FF 51` tempo meta through
unchanged), and the meaning of the TIMB entry fields beyond the `(patch,
bank)` pair. Neither affects the note content of the exported MIDI.

*Status: open — re-static (#54)*

## Related

**Formats:** [MUS](MUS.md) — playlist DLLs that select XMI tracks by index;
[11K](11K.md) — PCM audio formats (`.5K`, `.11K`) used for sound effects and
voice.
