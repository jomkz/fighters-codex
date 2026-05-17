# Font Bitmap (.FNT)

FA_1.LIB contains 15 `.FNT` files. These define the bitmapped fonts used for HUD text, menus, and briefing screens. Each is a **Win32 PE DLL** loaded at runtime via `LoadLibrary`.

## Format

Win32 PE DLL. File sizes vary — `4X12.FNT` decompresses to **12800 bytes** (0x3200). The large size relative to other 4608-byte overlays reflects embedded glyph bitmap data covering the full printable ASCII range.

## File Inventory

| File | Likely dimensions / context |
|------|-----------------------------|
| `4X6.FNT` | 4×6 pixel glyphs (tiny text) |
| `4X12.FNT` | 4×12 pixel glyphs |
| `HUD00.FNT` | HUD numeric / status text |
| `HUD01.FNT` | HUD alternate style |
| `HUD11.FNT` | HUD variant |
| `HUDSYM00.FNT` | HUD symbol glyphs (non-alphanumeric) |
| `HUDSYM01.FNT` | HUD symbol variant |
| `HUDSYM11.FNT` | HUD symbol variant |
| `HUI11.FNT` | HUD interface text |
| `HUISYM11.FNT` | HUD interface symbols |
| `MAPFONT.FNT` | Theater map labels |
| `WII11.FNT` | Window interface text |
| `WIN00.FNT` | Window text (referenced as `winfont` from `.HUD` files) |
| `WIN01.FNT` | Window text variant |
| `WIN11.FNT` | Window text variant |

The name prefix encodes context (`HUD`, `WIN`, `MAP`) and suffix may encode locale or variant (`00`=base, `01`=alt, `11`=third).

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
