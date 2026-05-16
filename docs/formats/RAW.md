# Screenshot -- Raw Screen Capture (.RAW)

`.RAW` files are **proprietary binary screenshots** written by the FA engine. They are
not compatible with standard image tools and must be converted before viewing or editing.

The original FATK (DuoSoft 1998) can convert `.RAW` ↔ `.BMP`. The fighters-toolkit
will add `.RAW` → `.PNG` conversion once the binary structure is fully reverse-engineered.

---

## Known Facts

- Written to the FA install directory when the player takes an in-game screenshot
- Width and height match the game's render resolution (640×480 in standard VGA mode)
- Likely palettized (8 bpp), consistent with the rest of the engine's graphics pipeline
- FATK treats them as directly convertible to/from 256-color `.BMP`

## Binary Structure

**Not yet fully documented.** Structure is under investigation.

| Offset | Size | Description |
|--------|------|-------------|
| ?      | ?    | Header / magic — unknown |
| ?      | ?    | Width, height — unknown |
| ?      | ?    | Palette — likely 768 bytes (256 × RGB6) as in `.PAL` |
| ?      | ?    | Pixel data — likely raw 8-bit palette indices, row-major |

## Related

- [PAL.md](PAL.md) — the 6-bit VGA palette format used across FA graphics
- [PIC.md](PIC.md) — the standard palettized image format for textures and UI
