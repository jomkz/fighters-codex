---
format: CB8
name: FMV Container
extensions: [".CB8"]
category: video
endianness: little
spec:
  status: partial
  gaps:
    - kind: re-static
      issue: 54
      note: "MRFA payload dwords, DRBC +0x10 field, EDB palette-half global"
codec:
  direction: round-trip
  byte_identical: false
  rationale: "VQ re-encode is pixel-exact (decode→repack→decode identical; audio and container fields carry verbatim) but the encoder chooses its own codebook packing — byte identity is a non-goal for FMV"
  lib: [lib/src/cb8.cpp]
  commands: [cb8]
  tests: [tests/test_cb8.cpp]
  fuzz: []
  gui: [gui/src/editors/cb8_editor.cpp]
  fixtures:
    synthetic: true
    real_manifest: true
related: [11K, PAL, LIB]
---

# CB8 — FMV Container (.CB8)

Multiplexed audio/video container for full-motion video. Used for intros,
cutscenes, and per-aircraft presentation clips. Each `.CB8` is paired with a
`.11K` audio file of the same stem (for playback outside the container).
Found in `FA_4C.LIB`, `FA_4D.LIB`, `FA_10.LIB`, `FA_10B.LIB`, `FA_11.LIB`,
and `FA_11B.LIB`.

## Tools

### fx

```
fx cb8 info   <file.CB8>                       # header and chunk summary
fx cb8 frames <file.CB8> [-o output_dir]       # decode every frame to PGM (indices)
fx cb8 unpack <file.CB8> [-o output_dir]       # decode every frame to colour PNG
fx cb8 repack <orig.CB8> <png_dir> [-o out]    # rebuild the movie around edited frames
```

`unpack` renders through each frame's embedded palette. `repack` re-encodes
the video (each PNG must use ≤ 256 distinct colours; the per-frame palette is
rebuilt) while the DRBC header, every audio chunk, the stream order, and the
VooM timing carry over from the original verbatim — the decode→edit→repack
loop is pixel-exact (#95).

### Other Tools

- **GIMP** — free, cross-platform; batch script (`File → Script-Fu`) useful for processing many frames
- **Paint.NET** — free, Windows
- **Photoshop** `$` — industry standard; *Image Processor* script for batch frame edits
- **Affinity Photo** `$` — one-time purchase alternative to Photoshop

## File Layout

All multi-byte integers are little-endian.

The file begins with a 64-byte DRBC header, followed by a sequence of
variable-length typed chunks packed back-to-back. Each chunk starts with a
4-byte ASCII type tag and a 4-byte total size (including the tag and size
field).

| Offset | Size | Description |
|--------|------|-------------|
| `0x00` | 64  | DRBC file header |
| `0x40` | var | Typed chunks: MRFA, MRFI, VooM (see below) |

Chunk structure:

| Offset | Size | Type | Description |
|--------|------|------|-------------|
| `+0x00` | 4 | char[4] | ASCII type tag (`MRFA`, `MRFI`, or `VooM`) |
| `+0x04` | 4 | u32 | Total size of this chunk in bytes (including these 8 bytes) |
| `+0x08` | var |  | Chunk payload |

### DRBC File Header (64 bytes)

| Offset | Size | Type | Description |
|--------|------|------|-------------|
| `0x00` | 4  | char[4] | Magic `DRBC` — the fourth **generation** of the format: `InitCobra` explicitly rejects `ARBC`, `BRBC`, and `CRBC` as too old (confirmed) |
| `0x04` | 4  | u32 | Flags: bit 0 = audio interleaved (MRFA chunks present), bit 1 = pixel-doubled playback (confirmed from `InitCobra`) |
| `0x08` | 2  | u16 | Audio timing divisor `P` (observed: 150) |
| `0x0A` | 2  | u16 | Audio rate term `Q` (observed: 22050); samples per MRFA chunk = `Q × 50 / P` = 7350 (confirmed: `InitCobra` primes two chunks of exactly this size) |
| `0x0C` | 4  | u32 | Format version — the engine requires `< 0x67` (observed: 0x65) |
| `0x10` | 2  | ?   | **Unknown** (observed: 0x0000 or 0x0080) |
| `0x12` | 46 | u8[46] | 0xFF padding |

The engine reads the file **sequentially** — header, two priming MRFA chunks
(when flag bit 0 is set; the first sample of each is forced to 0x80 silence),
the VooM index, then frames — and never checks the MRFI/MRFA/VooM tags: the
index is trusted for every seek (confirmed; the tags exist for the file
format, not the player).

### Chunk Type: MRFA — Audio Block

Raw PCM audio data. The chunk payload contains uncompressed 8-bit unsigned PCM
samples at 11025 Hz (matching the `.11K` convention). Silence is 0x80.

| Offset | Size | Type | Description |
|--------|------|------|-------------|
| `+0x00` | 4 | char[4] | Magic `MRFA` |
| `+0x04` | 4 | u32 | Chunk size (observed: 7374) |
| `+0x08` | 4 | u32 | **Unknown** (observed: 128 — not a pixel dimension) |
| `+0x0C` | 4 | u32 | **Unknown** (observed: 0) |
| `+0x10` | 4 | u32 | **Unknown** (observed: 8) |
| `+0x14` | 4 | u32 | **Unknown** (observed: 1) |
| `+0x18` | 7350 | u8[] | Raw 8-bit unsigned PCM samples at 11025 Hz |

7350 samples ÷ 11025 Hz = 666.7 ms = exactly 10 video frames at 15 fps.

### Chunk Type: MRFI — Video Key Frame

One **self-contained, vector-quantized key frame** (confirmed — engine trace
#95, `DecodeSVGA8Frame` at `0x456EC0`; the `DecodeFrame` dispatcher at
`0x442370` has **no inter decoder for the 8-bit submode**, so nothing carries
over between frames). Every frame brings its own palette and codebooks.

> An earlier revision of this section documented a delta/skip-map model with
> block data at `+0x18`; that model was wrong — the region at `+0x18` is the
> **palette** — and it never decoded real frames. The layout below is read
> from the engine and validated by a pixel-exact decoder.

| Offset | Size | Type | Description |
|--------|------|------|-------------|
| `+0x00` | 4 | char[4] | Magic `MRFI` (unchecked by the player) |
| `+0x04` | 4 | u32 | Chunk size, padded to a 4-byte multiple |
| `+0x08` | 1 | u8  | Frame kind: 0 = key (the only kind used by CB8) |
| `+0x09` | 1 | u8  | Submode: 5 = 8-bit paletted (6 = the 15/16/24-bit VDO path) |
| `+0x0A` | 2 | u16 | `A` — detail-book entries for pixel rows `< X` |
| `+0x0C` | 2 | u16 | `B` — detail-book entries for pixel rows `>= X` |
| `+0x0E` | 2 | u16 | `C` — single-book entries |
| `+0x10` | 2 | u16 | `S` — mode-bitmap bytes (`((cells + 31) / 32) × 4`; 600 for 320×240) |
| `+0x12` | 2 | u16 | `D` — palette bytes (768) |
| `+0x14` | 2 | u16 | `X` — detail-book switch row in pixels (0xFFFF = never) |
| `+0x16` | 2 | u16 | Padding (0) |
| `+0x18` | `D` | u8[] | **Palette**: 256 × 3 bytes of 6-bit VGA RGB — every frame carries its own |
| — | `(A+B)×4` | u8[] | **Detail book**: 2×2-pixel entries (4 palette indices, row-major); entries `0..A-1` serve rows `< X`, `A..A+B-1` rows `>= X` |
| — | `C×4` | u8[] | **Single book**: 2×2-pixel entries expanded to 4×4 at decode |
| — | `S` | u8[] | **Mode bitmap**: one bit per 4×4 cell, row-major, continuous across rows; consumed as u32-LE words **MSB-first** |
| — | var | u8[] | **Index stream** (to end of chunk, zero-padded to the 4-byte boundary) |

**Cell grid.** The frame divides into 4×4-pixel cells, row-major (80 × 60 =
4,800 for 320×240). For each cell, its mode bit selects the coding:

- **Bit 0 — single** (`CopySB8`): one index byte into the single book. The
  4-byte entry `(a, b, c, d)` is a 2×2 block expanded to 4×4 — plainly, each
  value pixel-doubled into its quadrant (`ExpandDB`), or with a dither when
  the display path enables it (`EDB`, below).
- **Bit 1 — detail** (`CopyDB8`): four index bytes selecting detail-book
  entries placed as the TL, TR, BL, BR **2×2 quadrants**; each 4-byte entry
  is one 2×2 block (row 0: bytes 0–1, row 1: bytes 2–3). Rows `>= X` index
  the second book half (the `B` entries).

**Decode algorithm:**

```
parse A,B,C,S,D,X; locate palette/detail/single/bitmap/index regions
cell = 0
for cy in 0..cell_rows-1:
    book = detail[0..A)        if cy*4 <  X or X == 0xFFFF
         = detail[A..A+B)      otherwise
    for cx in 0..cell_cols-1:
        word = u32le(bitmap[(cell/32)*4 ..])       // MSB-first bit order
        bit  = (word >> (31 - cell%32)) & 1
        if bit == 0:  expand single[next_index()] into the 4x4 cell
        else:         place book[next_index()] at TL, TR, BL, BR (4 indices)
        cell += 1
```

**EDB dither (display-time option).** When the SVGA path enables dithering,
each single-book value `v` expands not by plain doubling but as a 2×2
checkerboard of `v` and a partner `p`: the neighbour index (`v−1` or `v+1`,
within the same 128-entry palette half, guarded at 0/0x7F/0x80/0xFF) whose
RGB — looked up in **this frame's palette** — is nearest to `v`'s with
squared distance ≤ 8; `p = v` when neither qualifies. The expanded rows are
`(a, pa, b, pb) / (pa, a, pb, b)` and likewise for `(c, d)`. `fx` decodes
with the plain expansion (deterministic and exactly invertible); the dither
is a player-side rendering variant, not stream data.

### Chunk Type: VooM — Video Index / Key Frame

Serves as both a video key frame marker and an A/V index table. The payload
begins with a 12-byte header describing the video stream, followed by a flat
array of 16-byte index entries (one per MRFI frame).

| Offset | Size | Type | Description |
|--------|------|------|-------------|
| `+0x00` | 4 | char[4] | Magic `VooM` |
| `+0x04` | 4 | u32 | Chunk size |
| `+0x08` | 4 | u32 | Video width in pixels (observed: 320) |
| `+0x0C` | 4 | u32 | Video height in pixels (observed: 240) |
| `+0x10` | 4 | u32 | Audio sync rate = samples_per_frame × fps (observed: 6000 = 400 × 15) |
| `+0x14` | 16×N |  | Index entries (N = (chunk_size − 20) / 16) |

Index entry (16 bytes each):

| Offset | Size | Type | Description |
|--------|------|------|-------------|
| `+0x00` | 4 | u32 | Absolute file offset of this MRFI chunk |
| `+0x04` | 4 | u32 | Byte size of this MRFI chunk |
| `+0x08` | 4 | u32 | Cumulative audio sample count at this frame (frame_index × samples_per_frame) |
| `+0x0C` | 4 | u32 | Audio samples per frame (constant: 400) |

### Typical Chunk Sequence

```
DRBC header
MRFA  — priming audio block (first sample forced to 0x80 silence)
MRFA  — second priming block
VooM  — A/V index (N entries)
MRFI  — key frame 0
MRFI  — key frame 1
...   — MRFA blocks interleaved roughly every 10 frames
MRFI  — key frame N-1
```

### Palette and Colour

**Every MRFI frame embeds its own 768-byte palette** (256 × 3 bytes of 6-bit
VGA RGB at payload offset `+0x18`) — resolved by the #95 engine trace. No
external palette exists or is needed: decoding is fully self-contained, and
palettes can change per frame (the movies fade by animating them).

PALETTE.PAL never applies to CB8 (its low indices are magenta placeholders);
the earlier greyscale-fallback advice is obsolete now that the in-band
palette decodes true colour.

## File Inventory

| File | Video | Audio | Source LIB |
|------|-------|-------|------------|
| ATF.CB8 | VooM 320×240 | .11K (external) | FA_4C.LIB |
| C_INTRO.CB8 | VooM 320×240 | .11K (external) | FA_4C.LIB |
| JANELOGO.CB8 | 466 MRFI frames | MRFA blocks (11025 Hz) | FA_4C.LIB |
| B2_D.CB8 | MRFI delta frames | MRFA blocks (11025 Hz) | FA_10.LIB |

JANELOGO.CB8 (6,496,064 bytes): VooM at offset 14812 with 466 index entries
(chunk_size 7476 = 20 + 466×16). Frame 0 offset: 22288, duration: ~31.1 s
@ 15 fps.

## Engine Notes

### Provenance: the Cobra framework

CB8's player is **"Cobra"**, EA's in-house movie framework — not licensed
middleware (confirmed, #95 engine trace). The evidence: the decoder is
first-party C++ compiled into the game executable (`InitCobra`, `SetupCobra`,
`PlayCobra`, `DecodeFrame` — MSVC-mangled names from EA's own FA.SMS
symbols), it shares engine-internal structs (`GlobalData`, `MovieContext`,
`FrameHeader`), reads movies through the engine's own [LIB](LIB.md) resource
layer rather than a middleware I/O callback, and renders through the engine's
VGA banking (`SetVESABank`, `DrawAcrossBank`). The magic's generation lineage
— `InitCobra` rejects `ARBC`, `BRBC`, and `CRBC` as too old before accepting
`DRBC`, plus an internal version gate (`< 0x67`) — is a format evolving
privately across the USNF-line titles this anthology collects; licensed FMV
of the era (RAD's Smacker, and Bink from 1999) shipped as self-contained
libraries with their own containers. "CB8" reads as *CodeBook, 8-bit*; the
same Cobra dispatcher also serves the 15/16/24-bit submode used by the
[VDO](VDO.md) hi-color movies, so Cobra is the umbrella for both. The name's
origin is unrecorded in the binary — an internal codename (inferred).

## Open Questions

### 1. Playback palette recovery — resolved

**Resolved by the #95 engine trace:** the palette is embedded per frame
(768 bytes at MRFI `+0x18`); nothing engine-internal or external exists.
`DecodeSVGA8Frame` reads it from the frame, and `EDB`'s dither computes
neighbour distances against it.

*Status: resolved — re-static (#95)*

### 2. Unknown header fields

The four MRFA payload dwords at `+0x08..+0x17` (observed 128, 0, 8, 1), the
DRBC field at `+0x10` (0x0000 or 0x0080), and the global palette-half selector
`EDB` consults (`GlobalData+0xc1b0` bit 0) remain semantically unmapped. The
DRBC audio-timing pair and version gate are now confirmed (see the header
table); the MRFI payload header is fully mapped.

*Status: open — re-static (#54)*

## Related

**Formats:** [11K](11K.md) — the paired external audio (and the MRFA sample
format); [PAL](PAL.md) — why PALETTE.PAL does *not* apply here; [LIB](LIB.md)
— the six carrier archives.
