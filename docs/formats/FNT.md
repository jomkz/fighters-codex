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
u32  header_constant      (value = 7 in all observed FNT files — likely format version)
u32  glyph_va[256]        one VA per character, ASCII 0–255
```

Control characters (ASCII 0–31) each have a VA pointing to one of the 32 consecutive `0xC3` bytes at VA 0x1804–0x1823. All of those bytes are `0xC3` (the blank glyph marker), so all control chars render as blank regardless of which specific VA they hold.

ASCII 32 (' ', space) also maps to a `0xC3` byte at VA 0x1824 — space is a blank glyph.

Printable character data begins at VA 0x1825.

### Glyph encoding

**`0xC3` = blank/empty glyph** (single-byte; the engine reads it as "advance with no pixels").

All other glyphs use a variable-length encoding built from four recurring byte values:

| Byte | Binary |
|------|--------|
| `0x03` | 00000011 |
| `0xF9` | 11111001 |
| `0x88` | 10001000 |
| `0x07` | 00000111 |

The encoding is **not** raw 1-bpp row data — these values do not correspond to simple scanlines for a 4-pixel-wide cell. The same four values appear in both 4X6.FNT and 4X12.FNT in different orders, confirming a compact encoding (possibly nibble-packed rows + advance-width byte, or a custom RLE). Exact scheme requires tracing the glyph-drawing routine in FA.EXE.

4X6.FNT glyph for ASCII 33 ('!') — starts at VA 0x1825, 22 bytes before the next `0xC3`:
```
03 F9 88 07 03 F9 88 07 03 F9 88 07 03 F9 03 F9 88 07 03 F9 03 F9
```

4X12.FNT '!' starts at VA 0x1825 with a different byte order (same values, more bytes), confirming the 4X12 glyphs are taller/larger encodings of the same characters.

### $$DOSX metadata

The $$DOSX section (512 bytes) contains a small header. Both 4X6.FNT and 4X12.FNT show identical $$DOSX values (`u16[4]=16, u16[5]=6`). Since the two files have different glyph heights but the same $$DOSX, these values are **not** per-file cell dimensions — they are system constants or encoding parameters shared by all FNT files.

## Toolkit Roadmap

Pointer table and glyph data layout are confirmed. Blocked on glyph encoding scheme (needs Ghidra trace of the drawing routine) before PNG export can produce correct output:

- New `lib/src/fnt.cpp` + `lib/include/ft/fnt.h` — parse pointer table + glyph bitmaps
- New `cli/cmd_fnt.cpp` — `ft fnt unpack <file.FNT> -o <dir>/` extracts each glyph as a 1-bpp PNG; writes `metrics.csv` with `{char, width, height}` per row
- GUI: `fnt_viewer.h/cpp` in `gui/src/editors/` for interactive glyph grid preview

## TODO — Deep Dive

- Decode exact glyph encoding scheme — bytes `{03, F9, 88, 07}` are confirmed code values (not raw pixels); nibble-packed rows, RLE, or advance-width encoding still unresolved; requires tracing the glyph-drawing routine in FA.EXE via Ghidra
- Confirm per-glyph record size (are there inline width/height metrics, or is cell size fixed per FNT file?)
- Verify whether `00`/`01`/`11` suffix encodes locale, resolution, or style variant
- Resolve count discrepancy: file inventory lists 15 files but LIB count shows 13

## Related

- [PIC.md](PIC.md) — 8-bit indexed bitmaps, also in FA_1.LIB
- [HUD.md](HUD.md) — HUD layouts that consume font data
