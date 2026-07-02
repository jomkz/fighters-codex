---
format: RAW
name: Raw Screen Capture
extensions: [".RAW"]
category: graphics
endianness: little
spec:
  status: partial
  gaps:
    - kind: re-gameplay
      issue: 56
      note: "header fields +8/+10/+12 unconfirmed against other resolutions"
codec:
  direction: read
  issue: 96
  lib: [lib/src/raw.cpp]
  commands: [raw]
  tests: []
  fuzz: []
  gui: [gui/src/editors/raw_viewer.cpp]
  fixtures:
    synthetic: false
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
```

There is no pack command yet (#96) — converted images are for viewing and
reference only.

### Other Tools

- **GIMP** — free, cross-platform
- **Paint.NET** — free, Windows
- **Photoshop** `$` — industry standard
- **Affinity Photo** `$` — one-time purchase alternative to Photoshop

## File Layout

All multi-byte integers are little-endian.

| Offset | Size  | Type    | Description |
|--------|-------|---------|-------------|
| `0x00` | 6     | char[6] | Magic `mhwanh` |
| `0x06` | 2     | u16     | Width — observed: 0x0400 = 1024 |
| `0x08` | 2     | u16     | **Unknown** (observed: 0x0004) — likely `width / 256` |
| `0x0A` | 2     | u16     | **Unknown** (observed: 0x0003) — likely `height / 256` |
| `0x0C` | 2     | u16     | **Unknown** (observed: 0x0001) — possibly frame count |
| `0x0E` | 18    | u8[18]  | Null padding |
| `0x20` | 768   | u8[256×3] | Embedded palette: 256 × RGB8 triplets (8-bit 0–255) |
| `0x320`| w × h | u8[]    | Pixel data: 8-bit palette indices, row-major, top-to-bottom |

Total size for a 1024×768 screenshot: **787,232 bytes** (32 + 768 + 786,432).

**Resolution:** observed files are 1024×768, not 640×480. Height is not
directly visible as a standard u16 in the observed header; it can be derived
from file size: `(filesize − 800) / width`.

**Embedded palette:** the `.RAW` palette is 8-bit RGB (0–255 per channel),
**not** the 6-bit VGA format used by `.PAL` and `.PIC` files. No scaling is
needed — values are already full 8-bit.

**`PALETTE.PAL`:** the `PALETTE.PAL` extracted from the LIBs is the in-game
cockpit/UI palette and does **not** match the palette embedded in `.RAW` files.
Use the embedded palette when converting screenshots.

### Conversion

To convert a `.RAW` to a standard image:
1. Read the 32-byte header; extract width from offset 6 (u16).
2. Read 768 bytes of embedded palette (256 × R, G, B).
3. Derive height: `(filesize − 800) / width`.
4. Read `width × height` bytes of pixel indices.
5. Map each index through the palette to produce RGB output.

## Open Questions

### 1. Header fields at +0x08, +0x0A, +0x0C

The values 4 and 3 at offsets 8 and 10 are likely `width / 256` and
`height / 256` (1024/256 = 4, 768/256 = 3), and offset 12 possibly a frame
count — but every capture examined so far is 1024×768, so the hypotheses have
not been confirmed against other resolutions. Closing this needs captures taken
at a different display resolution on the running game.

*Status: open — re-gameplay (#56)*

## Related

**Formats:** [PAL](PAL.md) — contrast: RAW's embedded palette is 8-bit, not
VGA 6-bit; [PIC](PIC.md) — the engine's other paletted-image format.
