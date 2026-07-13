---
format: FBC
name: Video Frame Index
extensions: [".FBC"]
category: video
endianness: little
spec:
  status: complete
codec:
  direction: round-trip
  byte_identical: true
  lib: [lib/src/fbc.cpp]
  commands: [fbc]
  tests: [tests/test_fbc.cpp]
  fuzz: [fuzz/fuzz_fbc.cpp]
  gui: [gui/src/editors/vdo_editor.cpp]
  fixtures:
    synthetic: true
    real_manifest: true
    real_install: false
related: [VDO, 11K]
---

# FBC — Video Frame Index (.FBC)

Companion index file for a `.VDO` video. Provides the byte size of each frame
so that a decoder can seek directly to any frame without scanning the video
data. Found in `FA_7.LIB`: **exactly one `.FBC` per `.VDO`**, sharing the same
4-character stem (355 pairs). Audio pairs differently — by 3-character prefix,
not by full stem (see § Relationship to .VDO).

## Tools

### fx

```
fx fbc info <file.FBC>     # frame count + expected paired-VDO size
fx fbc ls   <file.FBC>     # per-frame size and VDO offset table
```

`fx_lib` round-trips the format byte-identically (`fbc_read`/`fbc_write`);
the fxs VDO editor renders the same index through the library.

## File Layout

All multi-byte integers are little-endian.

No magic or header. The file is a flat array of N u32 values, where N equals
the frame count stored in the paired `.VDO` header (offset 16). File size is
always `4 × N` bytes.

| Offset | Size  | Type     | Description |
|--------|-------|----------|-------------|
| `0x00` | 4 × N | u32[N]   | Byte size of each frame in the paired .VDO |

### Relationship to .VDO

```
frame_data_offset(n) = 816 + sum(FBC[0 .. n-1])
invariant: sum(FBC[0 .. N-1]) == VDO_file_size - 816
```

`N` also equals the frame-count `u16` at VDO header offset `0x10` (confirmed).

**Verified against the full corpus (#137).** All **355** `.FBC`/`.VDO` pairs in
`FA_7.LIB` satisfy the invariant with zero mismatches; frame counts range 12 →
1685 (41,163 frames total). Every `.VDO` has exactly one same-stem `.FBC`.

**Audio pairing is by 3-character prefix, not full stem.** The `.11K` narration
files use a 3-character stem (e.g. `AAC.11K`) shared across all 4-character
`.VDO` variants of that briefing group (`AACA`, `AACB`, … — the 4th character
`A`–`J` is the variant/angle). Of the 105 briefing-group prefixes, **104 carry
audio**; one group (`IQC`) is silent. So a `.VDO` is not guaranteed a
same-stem `.11K` — it shares its group's track (if any).

### Observed Values

Representative pairs (the invariant is verified across all 355 — see above):

| File | Entries (N) | FBC size | VDO size | VDO size − 816 |
|------|-------------|----------|----------|----------------|
| ABCA.FBC | 68 | 272 | 113,353 | 112,537 |
| AACA.FBC | 123 | 492 | 156,301 | 155,485 |
| AACB.FBC | 260 | 1040 | 362,853 | 362,045 |
| IPCA.FBC | 1685 | 6740 | 13,141,769 | 13,140,953 |

## Related

**Formats:** [VDO](VDO.md) — the video stream this file indexes; [11K](11K.md)
— the audio track, shared per 3-character briefing-group prefix.

**Engine:** the fxs VDO editor renders this index (frame table); the
fx_lib codec is tracked in #107.
