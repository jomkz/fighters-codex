# Font Bitmap (.FNT)

FA_1.LIB contains 13 `.FNT` files. These define the bitmapped fonts used for HUD text, menus, and briefing screens. Each is a **DOS MZ executable overlay** loaded by the FA engine at runtime.

## Format

DOS MZ executable (magic `4D 5A`). File sizes vary — `4X12.FNT` decompresses to **12800 bytes** (0x3200), consistent with the engine's 0x1200-multiple overlay sizing pattern.

```
Offset  Value   Description
------  -----   -----------
0x00    4D 5A   MZ magic
0x02    80 00   Last page bytes used (128)
0x04    01 00   Pages in file
...
0x3C    80 00   Overlay header offset
```

Named files include `4X12.FNT`, `4X6.FNT`, `HUD00.FNT`, `HUD01.FNT`, `HUDSYM00.FNT`, `MAPFONT.FNT`, `WIN00.FNT`, etc. — naming suggests width×height or target context (HUD, window, map).

## Location

| LIB | Count |
|-----|-------|
| FA_1.LIB | 13 |

## TODO — Deep Dive

- Disassemble a `.FNT` overlay to locate the glyph bitmap table and character metrics
- Determine pixel format (1-bpp, 4-bpp, or 8-bpp indexed) and grid layout
- Correlate font names (e.g. `4X6`) with rendered glyph dimensions

## Related

- [PIC.md](PIC.md) — 8-bit indexed bitmaps, also in FA_1.LIB
- [HUD.md](HUD.md) — HUD layouts that consume font data
