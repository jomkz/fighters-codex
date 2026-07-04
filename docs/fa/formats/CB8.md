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
      note: "playback palette is engine-internal, not yet recovered"
    - kind: re-static
      issue: 54
      note: "unknown fields in DRBC/MRFA/MRFI headers"
codec:
  direction: read
  issue: 95
  lib: [lib/src/cb8.cpp]
  commands: [cb8]
  tests: []
  fuzz: []
  gui: [gui/src/editors/cb8_editor.cpp]
  fixtures:
    synthetic: false
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
fx cb8 info   <file.CB8>               # header and chunk summary
fx cb8 frames <file.CB8> [output_dir]  # decode every frame to PGM images
```

There is no repack command (#95) — frame-level edits only.

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
| `0x00` | 4  | char[4] | Magic `DRBC` |
| `0x04` | 4  | u32 | flags (observed: 0x00000000 or 0x00000001) |
| `0x08` | 8  | ?   | **Unknown** constant (observed identical across all files) |
| `0x10` | 2  | ?   | **Unknown** (observed: 0x0000 or 0x0080) |
| `0x12` | 46 | u8[46] | 0xFF padding |

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

### Chunk Type: MRFI — Inter Video Frame

A single delta-coded (P-frame) video frame. The chunk carries a skip map
identifying which 4×4 pixel blocks changed, followed by the new block data.

| Offset | Size | Type | Description |
|--------|------|------|-------------|
| `+0x00` | 4   | char[4] | Magic `MRFI` |
| `+0x04` | 4   | u32 | Chunk size (variable, minimum 8240) |
| `+0x08` | 16  | u32[4] | Payload header — **unknown** purpose, constant across frames |
| `+0x18` | 600 | u8[600] | Skip map: 4800 bits, one bit per 4×4 block (LSB-first per byte). Bit = 1: block changed; bit = 0: block unchanged |
| `+0x270`| var |  | Block data (chunk_size − 624 bytes; see below) |

**Block grid.** Video is divided into non-overlapping 4×4 pixel blocks. For
320×240 video: 80 columns × 60 rows = 4800 blocks. Block index `b` maps to
pixel coordinates `((b % 80) * 4, (b / 80) * 4)`.

**Block data — two sections.** Let `bdSize = chunk_size − 624` and
`n_changed` = number of set bits in the skip map.

**Section 1** (bytes `0 .. n_changed×16 − 1`): delta blocks for each changed
position, stored in skip-map order (ascending block index). Each entry is 16
raw 8-bit palette index bytes covering the 4×4 pixel block in row-major order.

**Section 2** (bytes `n_changed×16 .. bdSize − 1`, present when
`bdSize > n_changed×16`): full-state overwrite starting from block 0.
`⌊(bdSize − n_changed×16) / 16⌋` complete blocks replace the canvas from
block 0 upward. Any trailing bytes (< 16) are ignored. Trailing zero bytes may
be omitted from the end of section 2.

**Decode algorithm:**

```
canvas  = uint8_t[4800 × 16]   // persistent across frames; init to 0x00
changed = [b for b in 0..4799 if skip_map bit b is set]

// Section 1: apply delta blocks
for i in 0..len(changed)-1:
    canvas[changed[i] * 16 .. +15] = block_data[i*16 .. i*16+15]

// Section 2: overwrite from block 0 (when extra data present)
extra = (bdSize - len(changed)*16) / 16   // integer division
for i in 0..extra-1:
    canvas[i * 16 .. +15] = block_data[len(changed)*16 + i*16 .. +15]
```

To render frame: for each block `b`, copy `canvas[b*16..+15]` to the 4×4 pixel
area at `((b%80)*4, (b/80)*4)` in row-major order.

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
MRFA  — silent/blank audio lead-in
MRFA  — first audio block
VooM  — A/V index (N entries) + video key frame marker
MRFI  — delta frame 0
MRFI  — delta frame 1
...
MRFI  — delta frame N-1
MRFA  — trailing audio
```

### Palette and Colour

CB8 frame data consists entirely of 8-bit palette indices. The palette used to
render those indices is **not stored in any LIB file**. It is an
engine-internal table loaded at startup from a resource embedded in the game executable (or
a companion file loaded before the cutscene begins).

**PALETTE.PAL is the wrong palette for CB8.** Indices 1–46 in PALETTE.PAL are
all set to `(63, 0, 63)` (magenta in 6-bit VGA). CB8 frame data is heavily
saturated with index `0x01` (the sky / background colour for cutscene videos),
so applying PALETTE.PAL produces a garbled pink/magenta image.

**Greyscale fallback:** mapping each index byte directly to a grey value
(`R = G = B = index`) is always spatially correct and produces a legible
monochrome image. It is the recommended display mode until the true engine
palette is recovered.

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

## Open Questions

### 1. Playback palette recovery

The CB8 palette is engine-internal (an game-executable-embedded resource or a
pre-cutscene companion load); until it is located, decoded frames render
correctly only in greyscale.

*Status: open — re-static (#54)*

### 2. Unknown header fields

The DRBC header's 8-byte constant and 2-byte field, the four MRFA dwords at
`+0x08..+0x17`, and the MRFI 16-byte payload header are stable across the
corpus but semantically unmapped.

*Status: open — re-static (#54)*

## Related

**Formats:** [11K](11K.md) — the paired external audio (and the MRFA sample
format); [PAL](PAL.md) — why PALETTE.PAL does *not* apply here; [LIB](LIB.md)
— the six carrier archives.
