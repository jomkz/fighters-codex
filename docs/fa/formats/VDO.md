---
format: VDO
name: RATVID Streaming Video
extensions: [".VDO"]
category: video
endianness: little
spec:
  status: partial
  gaps:
    - kind: re-static
      issue: 55
      note: "tag-2 image-keyframe (DecompressVideoImage) path absent from the stock corpus, so decoded as a passthrough stub — unvalidated"
codec:
  direction: read
  rationale: "the engine ships no .VDO encoder (RATVID is authored offline); fx_lib decodes the container + per-frame copy-mask codec to palette-indexed / RGBA frames, validated by rendering the stock corpus"
  lib: [lib/src/vdo.cpp]
  commands: [vdo]
  tests: [tests/test_vdo.cpp]
  fuzz: [fuzz/fuzz_vdo.cpp]
  gui: [gui/src/editors/vdo_editor.cpp]
  fixtures:
    synthetic: true
    real_manifest: true
    real_install: true
related: [FBC, 11K, CB8]
---

# VDO — RATVID Streaming Video (.VDO)

Video frames for mission briefing sequences. Found in FA_7.LIB (Disc 1) —
**355 files**, each a 4-character stem (e.g. `AACA`). Every `.VDO` has exactly
one same-stem `.FBC` frame-size index. Audio is shared per **3-character
briefing-group prefix**: `AAC.11K` narrates the whole `AACA`…`AACE` variant
group (the 4th character `A`–`J` is the angle/variant). 104 of the 105 groups
carry a `.11K`; the odd one out (`IQC`) is **not silent** — its narration is a
`.5K` ([11K.md](11K.md)), the only one in the corpus paired with video, and the
file that proves the `.5K` sample rate is 5512 Hz and not 5000
([#491](https://github.com/jomkz/fighters-codex/issues/491)). A `.VDO` is still not
guaranteed a same-stem audio track. All are 320×200, magic `RATV`. Pairing verified
across the full corpus (#137).

Audio and video durations agree exactly (`tests/test_audio.cpp` reconciles all 106
tracks): `audio_bytes / rate == frames / fps`, the one exception being `ZAC`, whose
8.0 s narration ends before its 14.7 s clip does.

## Tools

### fx

```
fx vdo info   F16C.VDO  [F16C.FBC]      # header (+ frame count with the FBC)
fx vdo export AACA.VDO   AACA.FBC -o out/  # decode every frame to PNG
```

`fx_lib` decodes read-only (the engine has no encoder). `vdo_open` needs the
paired `.FBC` — it supplies the frame boundaries the `.VDO` itself omits.
Decoding is sequential (frames are inter-coded); the decoder replays from frame
0 when asked for an earlier frame.

## File Layout

All multi-byte integers are little-endian.

| Offset | Size | Description |
|--------|------|-------------|
| `0x00` | 816 | Header (see below) |
| `0x330`| var | Frame data blocks, one per frame (sizes given by paired .FBC) |

### Header (816 bytes)

| Offset | Size | Type | Description |
|--------|------|------|-------------|
| `0x00` | 6 | char[6] | Magic `RATVID` (ASCII, no null terminator) |
| `0x06` | 1 | u8  | Major version = 1 |
| `0x07` | 1 | u8  | Minor version = 2 |
| `0x08` | 4 | u32 | Frame rate (observed: 15 fps) |
| `0x0C` | 4 | u32 | **Unknown** (observed: 0) |
| `0x10` | 2 | u16 | Frame count (N) |
| `0x12` | 2 | u16 | Width in pixels (observed: 320) |
| `0x14` | 2 | u16 | Height in pixels (observed: 200) |
| `0x16` | 2 | u16 | Palette entry count (always 256) |
| `0x18` | 2 | u16 | Audio channel count (always 1 = mono) |
| `0x1A` | 2 | u16 | Audio sample rate in Hz (observed: 8000) |
| `0x1C` | 4 | u32 | **Unknown** |
| `0x20` | 16 | u8[16] | Zeroed |
| `0x30` | 768 | u8[256×3] | VGA palette: 256 × 3 bytes (R, G, B each 6-bit, range 0–63) |

The 768-byte palette at offset 48 is a standard VGA DAC palette — 256 entries
of 3 bytes each, all values in 0–63 (6-bit per channel, matching VGA register
format). Entry 0 is always black (00 00 00). The palette is per-video; each
`.VDO` file carries its own.

Field at offset 22 is confirmed to be the palette entry count (always 256,
matching the 768-byte palette block at +48). Field at offset 24 is confirmed
to be the audio channel count (always 1 = mono).

### Frame Data

Frame data begins at offset 816. Frames are packed back-to-back with no
delimiters. Frame N starts at offset `816 + sum(FBC[0..N-1])`.

Frame data is palettized (8-bit palette indices into the header palette at
+48). Each frame is decoded by `GetVDOFrame` (VA `0x4AF510`) into the VDO
struct's decode buffers.

> **The `.VDO` codec is *not* the Cobra `DecodeFrame` cluster.** The shipped
> 320×200 8bpp movies decode through a small, self-contained path —
> `GetVDOFrame` → `UnRLE` → `DecompressVideo` (three functions at
> `0x4C8AA4–0x4C8DBB`, ~460 bytes) — and never touch the `DecodeFrame`
> dispatcher (`0x442370`) or the ~45 Cobra/CB8 leaves at `0x456300–0x45D090`.
> Those are the [CB8](CB8.md) player. "Cobra" is the shared FMV umbrella; the
> two codecs are distinct. (Reconciles the earlier note in
> [video-decode.md](../video-decode.md) that submode-6 served `.VDO`.)

### Per-frame stream structure

`GetVDOFrame` reads the whole frame (`FBC[n]` bytes) and parses it as a leading
`u16` **tag** followed by one or two RLE-or-raw sub-streams:

| Tag (`u16`) | Meaning |
|-------------|---------|
| `0` | Reuse the current codebook; decode the index stream only (delta frame) |
| `1` | **Whole-canvas RLE keyframe**: `UnRLE` straight into the canvas, no pixel blit. A `u16` at `+2` gives the frame's remaining bytes (`FBC[n] - 4`) and is *stepped over*; the RLE stream — whose own `u16` output count is 64,000, the full 320×200 canvas — starts at `+4` |
| `2` | Image keyframe → `DecompressVideoImage`; a second `u16` gives the sub-stream length |
| other | Codebook sub-stream: **bit `0x8000` set ⇒ RLE-compressed**, low 15 bits = its byte length (`UnRLE` expands it); raw otherwise |

> **The `+4`, not `+2`, matters** ([#491](https://github.com/jomkz/fighters-codex/issues/491)).
> `GetVDOFrame` (`0x4AF510`) calls `UnRLE((short *)((int)p + 2), canvas)` with `p = frame+2`,
> so the RLE header sits at `frame+4`. This spec said "color-table refresh" and the codec
> read the count from `frame+2` — which is the frame's *length*, not a pixel count — so it
> decoded roughly `frame_size` pixels instead of 64,000 and left a black band across the
> bottom of every keyframe. 89 such frames in 28 of the 355 videos; `LACA.VDO` uses one for
> 62 of its 135 frames. `tests/test_vdo.cpp` now decodes every real frame and compares the
> keyframes against an independent RLE expansion.

After the codebook sub-stream a `u16` **marker** separates the index sub-stream:
`0` = end, `0xFFFF` = an RLE-compressed index stream follows (`UnRLE`). The
frame is then rendered by `DecompressVideo(colortable, codebook, index, width,
height)`. Verified against the corpus: frame 0 of `AACA.VDO` begins `BE 8B`
(`tag = 0x8BBE`, bit `0x8000` set ⇒ a `0x0BBE`-byte RLE codebook block).

### UnRLE (`0x4C8AFC`)

Byte-oriented RLE, prefixed by a `u16` output-pixel count. Each control byte:
bit `0x80` set ⇒ **run** — length `(b & 0x7f) + 1` (or, if `0x7f`, a following
`u16 + 1`) of the next byte repeated; clear ⇒ **literal** — copy `b` bytes
verbatim. Decoding stops when the output count is exhausted.

### DecompressVideo (`0x4C8AA4`) — the blit dispatch

Walks the index stream in **8-pixel groups** (`width × height / 8`): a `0` byte
skips the group (leaves the prior frame's pixels — the inter-frame delta), and a
nonzero byte indexes a **span-emitter jump table at `0x50E5CE`** whose handler
writes that group's 8 pixels. `DecompressVideoImage` (`0x4C8CD8`) is the keyframe
variant, adding a row-replication tail (each output row copied to the next — the
`320×200 → 640×480` vertical doubling).

### The index byte is an 8-bit copy mask (#139)

The `0x50E5CE` handlers are **generated x86**, built once at startup by
`BuildSelfModifyCode` (`0x4C8BEC`, via `VDOInit` `0x405490`) into a code buffer
at `0x50E9CE`; the table statically holds only zeros. Each handler decodes one
index byte, and that byte is a **per-pixel copy mask** over the group's 8
pixels: **a set bit copies the next source pixel; a clear bit keeps the previous
frame's pixel** (advances the output pointer without reading source). So the
source stream carries only the pixels that changed, and the mask says where.

`BuildSelfModifyCode` emits, per index value:

- `0x00` → `add edi, 8; ret` — skip the whole group (the `0` fast-path in
  `DecompressVideo`).
- `0xFF` → `movsd; movsd; ret` — copy all 8 pixels (`A5 A5`).
- everything else → two `DoNibble` (`0x4C8C60`) calls (high nibble → pixels
  0–3, low nibble → pixels 4–7) + `ret`.

`DoNibble` emits the x86 for one 4-bit nibble = 4 pixels, MSB first:

| Nibble / 2-bit pair | Emitted | Effect |
|---|---|---|
| nibble `0xF` | `A5` (`movsd`) | copy 4 |
| pair `11` | `66 A5` (`movsw`) | copy 2 |
| pair `10` | `A4 47` (`movsb; inc edi`) | copy, keep |
| pair `01` | `47 A4` (`inc edi; movsb`) | keep, copy |
| pair `00` | `47 47` (`inc edi; inc edi`) | keep 2 |

So the codec is a per-pixel delta: **mask bit set ⇒ take a new pixel from the
source stream; clear ⇒ retain the prior frame's pixel** — plus the RLE that
compresses the mask and source streams.

Worked example — `AACA.VDO` frame 0 (`FBC[0]` = 35,546 B, `tag = 0x8BBE`): the
mask sub-stream is a `0x0BBE`-byte RLE block that expands to exactly **8,000**
mask bytes (`= 320×200/8`, one per group); the `0xFFFF` marker then introduces
an RLE source stream declaring **32,534** pixels. Reconciling the mask's set-bit
total against that source count byte-for-byte is the one detail the fx_lib
reference decoder pins down (#140).

### Audio

Audio is stored separately in the paired `.11K` file (raw PCM, 8000 Hz mono
8-bit). It is not embedded in the `.VDO`.

## File Inventory

| File | Frames | Width | Height | FPS | Audio Hz |
|------|--------|-------|--------|-----|----------|
| AACA.VDO | 123 | 320 | 200 | 15 | 8000 |
| AACB.VDO | 260 | 320 | 200 | 15 | 8000 |
| IPCA.VDO | 1685 | 320 | 200 | 15 | 8000 |

## Engine Notes

Confirmed functions (FA.SMS names), in load → decode order:

| VA | Symbol | Role |
|----|--------|------|
| `0x4AF1E0` | `OpenVDOFile` | Open the `.VDO` file, begin streaming |
| `0x4AF200` | `ReadVDOHeader` | Parse the 816-byte file header into a `VDOHEADER` |
| `0x4AF230` | `ReadFrameSizesFile` | Read the paired `.FBC` frame-size index |
| `0x4AF2D0` | `ReadVDOPalette` | Load the 768-byte VGA palette into `T_RGB[]` |
| `0x4AF320` | `VDOfromVDOHEADER` | Build the runtime `VDO` struct from the header |
| `0x4AF3A0` | `AllocVDO` | Allocate the frame decode buffers (codebook / index / color) |
| `0x4AF070` | `StartVDOAudio` | Start the paired `.11K` narration track |
| `0x4AF510` | `GetVDOFrame` | Read + parse one frame, dispatch to the decoder |
| `0x4C8AFC` | `UnRLE` | RLE decompress a sub-stream (see § UnRLE) |
| `0x4C8AA4` | `DecompressVideo` | Blit the index stream via the `0x50E5CE` table |
| `0x4C8CD8` | `DecompressVideoImage` | Keyframe/image blit + row replication |
| `0x405490` | `VDOInit` | One-time init; calls `BuildSelfModifyCode` |
| `0x4C8BEC` | `BuildSelfModifyCode` | Generate the 256 copy-mask handlers into `0x50E9CE` |
| `0x4C8C60` | `DoNibble` | Emit the x86 for one nibble (4 pixels) of a handler |
| `0x4AF6B0` | `VDO_320x200_to_640x480` | 2× upscale for the SVGA present path |

All operate on the `VDO` struct, whose fields correspond to the header layout
above. These live in the `0x4AExxx`/`0x4C8xxx` range — distinct from the
`0x456300` Cobra/CB8 cluster.

## Open Questions

### 1. tag-2 image-keyframe path

The `fx_lib` decoder (#140) decodes every frame of all 355 stock `.VDO` files —
the mask/RLE/copy-mask path is validated end to end (frame 0 of `AACA.VDO`
renders the briefing image pixel-for-pixel; the mask set-bit count matches the
source-pixel count exactly). The one unexercised branch is **tag 2**
(`DecompressVideoImage`, the row-replicated image keyframe): no shipped file
uses it, so the decoder treats it as a prior-frame passthrough. Confirming that
path needs a `.VDO` that carries it (none in the corpus) or the running engine.

*Status: open — re-static (#55; only the corpus-absent tag-2 path remains)*

## Related

**Formats:** [FBC](FBC.md) — the paired frame-size index; [11K](11K.md) — the
paired audio track; [CB8](CB8.md) — the other (fully decoded) FMV format.
