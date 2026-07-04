---
format: PIC
name: Palettized Image
extensions: [".PIC"]
variants: ["dense", "sparse", "jpeg"]
category: graphics
endianness: little
spec:
  status: partial
  gaps:
    - kind: re-static
      issue: 54
      note: "font_data_offset field semantics"
codec:
  direction: round-trip
  byte_identical: false
  issue: 175
  lib: [lib/src/pic.cpp]
  commands: [pic]
  tests: [tests/test_pic.cpp]
  fuzz: []
  gui: [gui/src/editors/pic_editor.cpp]
  fixtures:
    synthetic: true
    real_manifest: true
related: [PAL, LIB, SH]
---

# PIC — Palettized Image (.PIC)

Custom image format used for aircraft skins, HUD overlays, instruments, and
backgrounds. Three sub-formats share the same 64-byte header, identified by the
`format` field. Stored throughout the `.LIB` archives — FA_2.LIB carries the
in-game art, FA_3.LIB the encyclopedia photographs.

## Tools

### fx

```
fx pic info   <file.PIC>                # header and sub-format summary
fx pic unpack <file.PIC> [-o out.png]   # decode to PNG
fx pic pack   <file.png> [-o out.PIC]   # re-encode (see Round-Trip Notes)
```

### Other Tools

- **GIMP** — free, cross-platform; handles indexed-color and palette-aware editing well
- **Paint.NET** — free, Windows; simple and fast for texture touch-ups
- **Photoshop** `$` — industry standard; use 8-bit indexed mode to stay within palette
- **Affinity Photo** `$` — one-time purchase alternative to Photoshop

## File Layout

All multi-byte integers are little-endian.

### Header (64 bytes)

| Offset | Size | Type | Description |
|--------|------|------|-------------|
| `0x00` | 2  | u16 | format: 0=dense, 1=sparse, 0xD8FF=JPEG |
| `0x02` | 4  | u32 | width in pixels |
| `0x06` | 4  | u32 | height in pixels |
| `0x0A` | 4  | u32 | pixels_offset (absolute file offset of pixel data) |
| `0x0E` | 4  | u32 | pixels_size |
| `0x12` | 4  | u32 | palette_offset (absolute file offset of inline palette, 0 if none) |
| `0x16` | 4  | u32 | palette_size (0 if using system PALETTE.PAL) |
| `0x1A` | 4  | u32 | spans_offset (format=1 only) |
| `0x1E` | 4  | u32 | spans_size (format=1 only) |
| `0x22` | 4  | u32 | rowheads_offset (format=0 only) |
| `0x26` | 4  | u32 | rowheads_size (format=0 only; must equal 4 × height) |
| `0x2A` | 4  | u32 | font_data_offset (rarely used, usually 0; semantics unconfirmed — see Open Questions) |
| `0x2E` | 18 | u8[18] | Padding, zeroed |

### Pixel Data

One byte per pixel — each byte is a palette index (0–255). Index
**0xFF = transparent**.

### Palette

The inline palette at `palette_offset` (if `palette_size > 0`) is raw VGA 6-bit
data: `palette_size / 3` RGB triplets, each channel in range 0–63.

Scale to 8-bit: `actual = (stored << 2) | (stored >> 6)` (rotate-left-2).

- If `palette_size == 0`: the PIC uses the system `PALETTE.PAL`.
- A partial palette (`palette_size < 768`) overrides only the first
  `palette_size/3` entries.

### Format 0 — Dense / Texture

Pixel data is sequential, row-major (top to bottom, left to right):
`width × height` bytes.

A row-head table at `rowheads_offset` contains `height` u32 values, each the
absolute file offset of the start of that row. Must be reconstructed correctly
when encoding:
```c
rowheads[y] = pixels_offset + y * width;
```

Used for aircraft skins, terrain tiles, full-screen images.

### Format 1 — Sparse / Image

Used for HUD overlays and UI elements where most pixels are transparent.

`spans_offset` points to an array of 10-byte span records, terminated by
`row = 0xFFFF`:

| Offset | Size | Type | Description |
|--------|------|------|-------------|
| `+0x00` | 2 | u16 | row index |
| `+0x02` | 2 | u16 | start column (inclusive) |
| `+0x04` | 2 | u16 | end column (inclusive) |
| `+0x06` | 4 | u32 | byte offset into pixels_data for this span's pixels |

Pixels per span: `end - start + 1`.

### Format 0xD8FF — JPEG

The entire `.PIC` file content is a standard JPEG — pass it directly to a JPEG
decoder. All PIC files in `FA_3.LIB` are this format. These are encyclopedia
reference images (photographs, diagrams), not the 3D aircraft skin textures
(which use format 0 and carry the `_` prefix in FA_2.LIB).

## File Inventory

### Filename Conventions

The filename prefix identifies the role of the image within the engine:

| Prefix | Role | Example |
|--------|------|---------|
| `$` | 2D weapon / ordnance cockpit icon | `$AIM9M.PIC`, `$AGM65A.PIC` |
| `_` | Aircraft skin / texture (referenced by `.SH` `TextureFile` instruction) | `_A10.PIC`, `_KIN.PIC` |
| (none) | All other images: UI, medals, backgrounds, terrain tiles | `PALETTE.PIC`, `ATFSPLAS.PIC` |

The `$` and `_` prefixes are engine conventions embedded in the filenames stored
in the `.LIB` archives. They have no effect on the file format itself.

### FA_3.LIB Naming Convention (Encyclopedia Reference Images)

`FA_3.LIB` (Disc 2) contains 700+ JPEG-format PIC files used by the in-game
aircraft encyclopedia viewer. All are 512×384 pixels except the five bare-name
thumbnail files (640×480). They are never referenced by the 3D engine.

**Numeric suffix `<AC>_<N>.PIC` (N = 0–9)** — exterior photographs and action
shots of the aircraft, one image per slot. Most aircraft have 4–10 numeric
variants. The game cycles through them in the encyclopedia photo gallery.
Simple or uncommon aircraft may have only `_0`. **Count:** 678 files.

**Letter suffixes:**

| Suffix | Role | Count | Example |
|--------|------|-------|---------|
| `_C` | Cockpit interior photograph | 48 | `F14_C.PIC`, `F22_C.PIC` |
| `_E` | Engine photograph or cutaway | 38 | `F14_E.PIC`, `F22_E.PIC` |
| `_P` | Profile diagram with callout labels | 37 | `F14_P.PIC`, `F22_P.PIC` |
| `_F` | Internal structure / systems cutaway (CAD/exploded view) | 16 | `F22_F.PIC`, `F16C_F.PIC` |

`_F` is present only on higher-profile or more technically complex aircraft:
AF1, ASTOVL, AV8, B747, CMCHE, E2000, E3, F117, F16C, F22, F260, F29, F31,
GRIPEN, RAFALE, V22.

**Bare name `<AC>.PIC` (no suffix)** — five files (A6, F15, F15J, F18C, TU160)
at 640×480 pixels. These are aircraft selection screen / hangar thumbnails. The
aircraft image is composited against a white background. All other aircraft use
the `_0` exterior photo in contexts where a thumbnail is needed.

## Round-Trip Notes

- `fx pic pack` always encodes as format=0 (dense) with a full inline palette.
  The game accepts format=0 in place of any sub-format, including JPEG
  originals — but the repack is therefore **not byte-identical** for sparse or
  JPEG inputs, and dense repacks are not yet asserted byte-exact (#175 tracks
  the upgrade).
- Keep image dimensions unchanged — the engine does not resize at load time.
- Pixels are quantized to the nearest palette color on re-encode; alpha < 128
  maps to 0xFF.

## Open Questions

### 1. font_data_offset semantics

Header field `0x2A` is nonzero in only a handful of files and its consumer in
The game executable has not been traced; the FNT overlay DLLs carry the actual fonts, so the
field's role (legacy, or an alternate glyph path) is unconfirmed.

*Status: open — re-static (#54)*

## Related

**Formats:** [PAL](PAL.md) — the system palette and the 6-bit color encoding;
[LIB](LIB.md) — container for every PIC; [SH](SH.md) — 3D shapes reference `_`
textures via their `TextureFile` instruction.
