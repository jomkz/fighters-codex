# PIC -- Palettized Image (.PIC)

Custom image format used for aircraft skins, HUD overlays, instruments, and backgrounds.
Three sub-formats share the same 64-byte header, identified by the `format` field.

---

## Header (64 bytes, all fields LE)

```
Offset  Size  Description
------  ----  -----------
0        2    uint16: format  0=dense, 1=sparse, 0xD8FF=JPEG
2        4    uint32: width in pixels
6        4    uint32: height in pixels
10       4    uint32: pixels_offset  (absolute file offset of pixel data)
14       4    uint32: pixels_size
18       4    uint32: palette_offset (absolute file offset of inline palette, 0 if none)
22       4    uint32: palette_size   (0 if using system PALETTE.PAL)
26       4    uint32: spans_offset   (format=1 only)
30       4    uint32: spans_size     (format=1 only)
34       4    uint32: rowheads_offset (format=0 only)
38       4    uint32: rowheads_size   (format=0 only; must equal 4 * height)
42       4    uint32: font_data_offset (rarely used, usually 0)
46      18    Padding, zeroed
```

---

## Pixel Data

One byte per pixel -- each byte is a palette index (0-255). Index **0xFF = transparent**.

## Palette

The inline palette at `palette_offset` (if `palette_size > 0`) is raw VGA 6-bit data:
`palette_size / 3` RGB triplets, each channel in range 0-63.

Scale to 8-bit: `actual = (stored << 2) | (stored >> 6)` (rotate-left-2).

- If `palette_size == 0`: the PIC uses the system `PALETTE.PAL`.
- A partial palette (`palette_size < 768`) overrides only the first `palette_size/3` entries.

---

## Format 0 -- Dense / Texture

Pixel data is sequential, row-major (top to bottom, left to right): `width * height` bytes.

A row-head table at `rowheads_offset` contains `height` uint32 values, each the absolute
file offset of the start of that row. Must be reconstructed correctly when encoding:
```c
rowheads[y] = pixels_offset + y * width;
```

Used for aircraft skins, terrain tiles, full-screen images.

## Format 1 -- Sparse / Image

Used for HUD overlays and UI elements where most pixels are transparent.

`spans_offset` points to an array of 10-byte span records, terminated by `row = 0xFFFF`:

```
Offset  Size  Description
------  ----  -----------
0        2    uint16: row index
2        2    uint16: start column (inclusive)
4        2    uint16: end column (inclusive)
6        4    uint32: byte offset into pixels_data for this span's pixels
```

Pixels per span: `end - start + 1`.

## Format 0xD8FF -- JPEG

The entire `.PIC` file content is a standard JPEG -- pass it directly to a JPEG decoder.
All PIC files in `FA_3.LIB` (aircraft skins) are this format.

---

## Filename Conventions

The filename prefix identifies the role of the image within the engine:

| Prefix | Role | Example |
|--------|------|---------|
| `$` | 2D weapon / ordnance cockpit icon | `$AIM9M.PIC`, `$AGM65A.PIC` |
| `_` | Aircraft skin / texture (referenced by `.SH` `TextureFile` instruction) | `_A10.PIC`, `_KIN.PIC` |
| (none) | All other images: UI, medals, backgrounds, terrain tiles | `PALETTE.PIC`, `ATFSPLAS.PIC` |

The `$` and `_` prefixes are engine conventions embedded in the filenames stored in the
`.LIB` archives. They have no effect on the file format itself.

---

## Modding Notes

- `ft pic pack` always encodes as format=0 (dense) with a full inline palette. The game
  accepts format=0 in place of any sub-format, including JPEG originals.
- Keep image dimensions unchanged -- the engine does not resize at load time.
- Pixels are quantized to the nearest palette color on re-encode; alpha < 128 maps to 0xFF.
