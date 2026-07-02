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

Video frames for mission briefing sequences. Each `.VDO` file is paired with a
`.FBC` index file and a `.11K` audio file of the same stem name. Found in
FA_7.LIB (Disc 1) — 355 files.

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
+48). Each frame is Cobra-compressed. Partial RE of `DecodeFrame` (VA
`0x442370` in FA.EXE) reveals the following structure:

### Cobra codec — known structure

`DecodeFrame(MovieContext*, arg1, arg2)` dispatches on two fields of its
`MovieContext` struct:

| `context[8]` | Meaning |
|---|---|
| `0` | Delta frame (P-frame) — only changed regions are encoded |
| `1` | Key frame (I-frame) — full frame is encoded |

For delta frames (`context[8] == 0`), a second field `context[9]` selects a
sub-mode via an 8-entry jump table (VA `0x44260C`). Sub-modes 5 and 6 are the
only values observed in practice. Each sub-mode calls a dedicated leaf decoder
in the range `0x456300–0x45D090` (~45 functions); those leaf functions have
not yet been reversed.

`InitMovieContext` (VA `0x442360`) initialises the context; it only sets
`context[0x14] = 0`. The MovieContext struct is large — a pointer at offset
`0xC14E` points to the output/canvas buffer.

`VDOSetMode` (VA `0x4AED50`) selects between 320×200 and 640×480 render paths
and stores the result in `VDO.field_0x38`; this feeds `context[8]` during
decode.

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

Confirmed loader functions (FA.SMS):

| Symbol | Demangled | Role |
|--------|-----------|------|
| `?ReadVDOHeader@@YADPAUVDO@@PAE@Z` | `char ReadVDOHeader(VDO *, unsigned char *)` | Parse the 816-byte file header into a `VDO` struct |
| `?ReadFrameSizesFile@@...` | `ReadFrameSizesFile(...)` | Read the paired `.FBC` frame-size index file |
| `?AllocVDO@@YADPAUVDO@@@Z` | `char AllocVDO(VDO *)` | Allocate frame decode buffers for a loaded VDO |
| `?OpenVDOFile@@...` | `OpenVDOFile(...)` | Open the `.VDO` file and begin streaming |

These four symbols are confirmed present in FA.SMS. All are member-like free
functions operating on a `VDO` struct; the struct fields correspond directly
to the header layout documented above.

## Open Questions

### 1. Cobra per-frame compression

The container, header, palette, and frame indexing are fully mapped, but the
actual pixel-decompression loops live in ~45 unreversed leaf functions
(`0x456300–0x45D090`) selected by the `context[9]` jump table. Implementing a
decoder requires completing that RE — this is the roadmap's long pole, tracked
as its own epic.

*Status: open — re-static (#55)*

## Related

**Formats:** [FBC](FBC.md) — the paired frame-size index; [11K](11K.md) — the
paired audio track; [CB8](CB8.md) — the other (fully decoded) FMV format.
