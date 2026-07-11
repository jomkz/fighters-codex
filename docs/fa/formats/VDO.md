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
      note: "Cobra per-frame compression (~45 leaf decoders) not reversed"
codec:
  direction: none
  issue: 55
  gui: [gui/src/editors/vdo_editor.cpp]
  fixtures:
    synthetic: false
    real_manifest: true
related: [FBC, 11K, CB8]
---

# VDO — RATVID Streaming Video (.VDO)

Video frames for mission briefing sequences. Found in FA_7.LIB (Disc 1) —
**355 files**, each a 4-character stem (e.g. `AACA`). Every `.VDO` has exactly
one same-stem `.FBC` frame-size index. Audio is shared per **3-character
briefing-group prefix**: `AAC.11K` narrates the whole `AACA`…`AACE` variant
group (the 4th character `A`–`J` is the angle/variant). 104 of the 105 groups
carry a `.11K`; one group (`IQC`) is silent — so a `.VDO` is not guaranteed a
same-stem audio track. All are 320×200, magic `RATV`. Pairing verified across
the full corpus (#137).

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
| `1` | Color-table refresh (RLE) — no pixel blit this frame |
| `2` | Image keyframe → `DecompressVideoImage`; a second `u16` gives the sub-stream length |
| other | Codebook sub-stream: **bit `0x8000` set ⇒ RLE-compressed**, low 15 bits = its byte length (`UnRLE` expands it); raw otherwise |

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
writes that group's 8 output pixels from the codebook. `DecompressVideoImage`
(`0x4C8CD8`) is the keyframe variant, adding a row-replication tail (each output
row copied to the next — the `320×200 → 640×480` vertical doubling). The
per-code handlers in the `0x50E5CE` table (built at init) are the remaining
detail (#139); the framing and dispatch above are complete.

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
| `0x4AF6B0` | `VDO_320x200_to_640x480` | 2× upscale for the SVGA present path |

All operate on the `VDO` struct, whose fields correspond to the header layout
above. These live in the `0x4AExxx`/`0x4C8xxx` range — distinct from the
`0x456300` Cobra/CB8 cluster.

## Open Questions

### 1. Per-code span handlers (`0x50E5CE` table)

The container, header, palette, frame indexing, RLE, and the frame/blit dispatch
are now fully mapped (#137, #138). The one remaining piece for a byte-exact
decoder is the semantics of the per-code span-emitter handlers that
`DecompressVideo` calls through the `0x50E5CE` jump table (indexed by the index
byte) — i.e. how each code writes its 8 output pixels from the codebook. This is
a small, bounded RE task (the whole `.VDO` codec is three functions, ~460
bytes), not the ~45-function cluster once feared. Tracked under epic #55
(the span-handler decode folds into #139/#140).

*Status: open — re-static (#55)*

## Related

**Formats:** [FBC](FBC.md) — the paired frame-size index; [11K](11K.md) — the
paired audio track; [CB8](CB8.md) — the other (fully decoded) FMV format.
