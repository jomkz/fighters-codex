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

## CODE Section Layout (Partially Confirmed)

FNT files use **Phar Lap PE format** (signature `PL\0\0`). No imports. The CODE section contains a **glyph pointer table** followed by variable-length glyph bitmap data.

### Pointer table

Starts at CODE section offset 0 (VA 0x1000). Layout:

```
u32  first_char_or_count  (value = 7 in 4X6.FNT and 4X12.FNT)
u32  glyph_va[N]          one VA per character, starting at ASCII 0
```

The first ~32 entries (ASCII control chars) all point to the same blank/space glyph VA. Variable-stride entries begin at the first printable character.

Sample from 4X6.FNT (first printable chars, ASCII 0x22..0x26):
- `'` (0x22): VA 0x1822, size ~25 bytes
- `#` (0x23): VA 0x183B, ...
- `$` (0x24): VA 0x1855, ...
- `%` (0x25): VA 0x1879, ...
- `&` (0x26): VA 0x189C, ...

The spacing is variable (19–36 bytes per glyph), confirming a **proportional font** with different advance widths.

### $$DOSX metadata

The $$DOSX section (512 bytes) contains a small header. u16 values at byte offsets +8 and +10 are `16` and `6` in 4X6.FNT — likely `(bitmap_stride_bytes, cell_height)`. For a 4-wide font at 1bpp, stride = ceil(4/8) × 32 glyphs per row = 16 bytes/row; height = 6. This is consistent.

### RE next steps

1. Hex-dump the glyph at VA 0x1804 (the "blank" shared by first 32 entries) — its byte count reveals glyph bitmap format.
2. For 4X6.FNT: glyph at VA 0x1804 should be 1 byte (blank glyph), followed immediately by glyph data at 0x1805 for the next char. Verify by inspecting bytes at those VAs.
3. Compare 4X6.FNT vs 4X12.FNT glyph data sizes to confirm the height scaling (expect 12/6 = 2× byte count per glyph).

## Toolkit Roadmap

Once pixel format, stride, and per-glyph record size are confirmed:

- New `lib/src/fnt.cpp` + `lib/include/ft/fnt.h` — parse pointer table + glyph bitmaps
- New `cli/cmd_fnt.cpp` — `ft fnt unpack <file.FNT> -o <dir>/` extracts each glyph as a 1-bpp PNG; writes `metrics.csv` with `{char, width, height}` per row
- GUI: `fnt_viewer.h/cpp` in `gui/src/editors/` for interactive glyph grid preview

## TODO — Deep Dive

- Confirm pixel format (1-bpp strongly suspected from stride=16, height=6, cell_w=4)
- Determine the per-glyph record size (is it just raw bits, or are there inline metrics?)
- Verify whether `00`/`01`/`11` suffix encodes locale, resolution, or style variant
- Identify all 15 FNT filenames and their roles (some may be symbol/icon fonts, not alpha fonts)

## Related

- [PIC.md](PIC.md) — 8-bit indexed bitmaps, also in FA_1.LIB
- [HUD.md](HUD.md) — HUD layouts that consume font data
