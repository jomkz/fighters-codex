# Screenshot -- Raw Screen Capture (.RAW)

`.RAW` files are **proprietary binary screenshots** written by the FA engine. They are
not compatible with standard image tools and must be converted before viewing or editing.

Triggered in-game with **Ctrl-Alt-Shift-V**. Files are written to the FA install directory
as `screen0.raw`, `screen1.raw`, etc. (incrementing counter).

## File Layout

```
Offset   Size      Description
------   ----      -----------
0        6         Magic: "mhwanh"
6        2         Width (u16 LE) â€” observed: 0x0400 = 1024
8        2         Unknown (observed: 0x0004)
10       2         Unknown (observed: 0x0003)
12       2         Unknown (observed: 0x0001) â€” possibly frame count
14       18        Null padding
32       768       Embedded palette: 256 Ã— RGB8 triplets (3 bytes each, 8-bit 0â€“255)
800      w Ã— h     Pixel data: 8-bit palette indices, row-major, top-to-bottom
```

Total size for a 1024Ã—768 screenshot: **787,232 bytes** (32 + 768 + 786,432).

## Notes

**Resolution:** Observed files are 1024Ã—768, not 640Ã—480. Width is encoded at offset 6
as a u16 LE (0x0400 = 1024). Height is not directly visible as a standard u16 in the
observed header; it can be derived from file size: `(filesize âˆ’ 800) / width`. The
values 4 and 3 at offsets 8 and 10 are likely `width / 256` and `height / 256`
(1024/256 = 4, 768/256 = 3) but this has not been confirmed against other resolutions.

**Embedded palette:** The `.RAW` palette is 8-bit RGB (0â€“255 per channel), **not** the
6-bit VGA format used by `.PAL` and `.PIC` files. No scaling is needed â€” values are
already full 8-bit.

**`PALETTE.PAL`:** The `PALETTE.PAL` extracted from the LIBs is the in-game cockpit/UI
palette and does **not** match the palette embedded in `.RAW` files. Use the embedded
palette when converting screenshots.

## Conversion

To convert a `.RAW` to a standard image:
1. Read the 32-byte header; extract width from offset 6 (u16 LE).
2. Read 768 bytes of embedded palette (256 Ã— R, G, B).
3. Derive height: `(filesize âˆ’ 800) / width`.
4. Read `width Ã— height` bytes of pixel indices.
5. Map each index through the palette to produce RGB output.

## Applications

Use `fx raw unpack` to convert to PNG. There is no pack command yet â€” converted images
are for viewing and reference only.

- **GIMP** â€” free, cross-platform
- **Paint.NET** â€” free, Windows
- **Photoshop** `$` â€” industry standard
- **Affinity Photo** `$` â€” one-time purchase alternative to Photoshop
