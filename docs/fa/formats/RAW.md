---
format: RAW
name: Raw Screen Capture
extensions: [".RAW"]
category: graphics
endianness: little
spec:
  status: complete
codec:
  direction: round-trip
  byte_identical: true
  lib: [lib/src/raw.cpp]
  commands: [raw]
  tests: [tests/test_raw.cpp]
  fuzz: [fuzz/fuzz_raw.cpp]
  gui: [gui/src/editors/raw_viewer.cpp]
  fixtures:
    synthetic: true
    real_manifest: false
related: [PAL, PIC]
---

# RAW — Raw Screen Capture (.RAW)

`.RAW` files are **proprietary binary screenshots** written by the FA engine.
They are not compatible with standard image tools and must be converted before
viewing or editing. Triggered in-game with **Ctrl-Alt-Shift-V**; files are
written to the FA install directory as `screen0.raw`, `screen1.raw`, etc.
(incrementing counter) — they never appear inside the `.LIB` archives.

## Tools

### fx

```
fx raw info   <file.RAW>               # header summary
fx raw unpack <file.RAW> [-o out.png]  # convert to PNG
fx raw pack   <file.png> [-o out.RAW]  # PNG -> RAW (max 256 distinct colours)
```

### Other Tools

- **GIMP** — free, cross-platform
- **Paint.NET** — free, Windows
- **Photoshop** `$` — industry standard
- **Affinity Photo** `$` — one-time purchase alternative to Photoshop

## File Layout

Width and height are stored **big-endian** — unusual for this little-endian
engine, and confirmed against captures at four resolutions (1024×768,
800×600, 640×480, 320×200; see below).

| Offset | Size  | Type    | Description |
|--------|-------|---------|-------------|
| `0x00` | 6     | char[6] | Magic `mhwanh` |
| `0x06` | 2     | u8[2]   | Constant `00 04` (identical at every resolution) |
| `0x08` | 2     | u16 BE  | Width in pixels |
| `0x0A` | 2     | u16 BE  | Height in pixels |
| `0x0C` | 2     | u8[2]   | Constant `01 00` |
| `0x0E` | 18    | u8[18]  | Null padding |
| `0x20` | 768   | u8[256×3] | Embedded palette: 256 × RGB8 triplets (8-bit 0–255) |
| `0x320`| w × h | u8[]    | Pixel data: 8-bit palette indices, row-major, top-to-bottom |

Total size for a 1024×768 screenshot: **787,232 bytes** (32 + 768 + 786,432).

**Resolution evidence (confirmed):** captures taken at the four supported
display modes decode as `04 00`/`03 00` (1024×768), `03 20`/`02 58`
(800×600), `02 80`/`01 e0` (640×480), and `01 40`/`00 c8` (320×200) at
offsets 8/10 — big-endian width and height, with the surrounding fields
constant. An earlier revision read a little-endian width at `0x06` and
guessed `w/256`, `h/256` at 8/10; the multi-resolution corpus falsifies
that.

**Embedded palette:** the `.RAW` palette is 8-bit RGB (0–255 per channel),
**not** the 6-bit VGA format used by `.PAL` and `.PIC` files. No scaling is
needed — values are already full 8-bit.

**`PALETTE.PAL`:** the `PALETTE.PAL` extracted from the LIBs is the in-game
cockpit/UI palette and does **not** match the palette embedded in `.RAW` files.
Use the embedded palette when converting screenshots.

### Conversion

To convert a `.RAW` to a standard image:
1. Read the 32-byte header; width = u16 **big-endian** at offset 8, height =
   u16 big-endian at offset 10.
2. Read 768 bytes of embedded palette (256 × R, G, B — already 8-bit).
3. Read `width × height` bytes of pixel indices.
4. Map each index through the palette to produce RGB output.

## Round-Trip Notes

- `raw_repack` re-emits any capture byte-identically from its parsed fields
  (header re-serialized, palette and pixels copied through; trailing bytes
  fail rather than being silently carried). All in-install captures verify
  (tests/test_raw.cpp).
- `fx raw pack` (PNG import) rebuilds the palette from the image's distinct
  colours in first-seen order — at most 256; the PNG→RAW→PNG loop is
  pixel-exact. A repacked file need not be byte-identical to an engine
  capture of the same scene (palette order is the encoder's choice).

## Related

**Formats:** [PAL](PAL.md) — contrast: RAW's embedded palette is 8-bit, not
VGA 6-bit; [PIC](PIC.md) — the engine's other paletted-image format.
