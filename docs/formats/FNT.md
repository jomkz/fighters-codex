# Font Bitmap (.FNT)

FA_1.LIB contains 15 `.FNT` files. These define the bitmapped fonts used for HUD text, menus, and briefing screens. Each is a **Win32 PE DLL** loaded at runtime via `LoadLibrary`.

## Format

Win32 PE DLL. File sizes vary — `4X12.FNT` decompresses to **12800 bytes** (0x3200). The large size relative to other 4608-byte overlays reflects the glyph function code covering the full printable ASCII range.

## File Inventory

`font_height` confirmed by reading dword at CODE section offset 0 (file offset `0x200`) for all 15 files.

| File | font_height | Decompressed size | Context |
|------|-------------|-------------------|---------|
| `4X6.FNT` | **7** | 8704 | Tiny fixed-pitch text (4 wide × 6 glyph rows + 1 spacing = 7) |
| `4X12.FNT` | **12** | 12800 | Fixed-pitch 4×12 text |
| `HUD00.FNT` | **5** | 8704 | HUD text — 320×200 mode |
| `HUD01.FNT` | **10** | 12800 | HUD text — hi-res mode A |
| `HUD11.FNT` | **10** | 12800 | HUD text — hi-res mode B |
| `HUDSYM00.FNT` | **15** | 12800 | HUD symbols — 320×200 mode |
| `HUDSYM01.FNT` | **29** | 16896 | HUD symbols — hi-res mode A |
| `HUDSYM11.FNT` | **31** | 20992 | HUD symbols — hi-res mode B |
| `HUI11.FNT` | **10** | 12800 | HUD interface text |
| `HUISYM11.FNT` | **31** | 16896 | HUD interface symbols |
| `MAPFONT.FNT` | **10** | 12800 | Theater map labels |
| `WII11.FNT` | **10** | 12800 | Window interface text |
| `WIN00.FNT` | **6** | 12800 | Window/dialog text — 320×200 mode |
| `WIN01.FNT` | **12** | 16896 | Window/dialog text — hi-res mode A |
| `WIN11.FNT` | **10** | 12800 | Window/dialog text — hi-res mode B |

**Suffix semantics confirmed**: `00` = 320×200 (small font, short `font_height`), `01`/`11` = higher-resolution display modes (larger `font_height`). The `01` vs `11` distinction likely targets different colour depths or renderer paths — both are hi-res but `01` fonts are taller than their `11` counterparts for the `WIN`/`HUDSYM` families.

## Location

| LIB | Count |
|-----|-------|
| FA_1.LIB | 15 |

## CODE Section Layout (Confirmed)

FNT files use **Phar Lap PE format** (signature `PL\0\0`). No imports. The CODE section contains a **FONT struct** — a pointer table of compiled x86 glyph functions followed by a width table, then the glyph function bodies.

### FONT struct

Starts at CODE section offset 0 (VA 0x1000). Confirmed from tracing `@G_Print@16` in FA.EXE (`0x004986B0`), which accesses the loaded FNT DLL via the global `?cFont@@3PAUFONT@@A`:

```
u32  font_height          height of all glyphs in this font, in pixels
u32  glyph_fn[256]        VAs of compiled glyph functions, one per ASCII 0–255
u32  glyph_width[256]     advance width of each glyph in pixels
```

- `cFont[0]` — font height (used in `@G_Print@16` clip bounds check)
- `cFont[char + 1]` — called as a function pointer: `(*(code *)cFont[char + 1])(dst_ptr)`
- `cFont[char + 0x101]` — advance width; passed to `_G_Blit@36` as the glyph width

Total struct: 1 + 256 + 256 = 513 `u32` values = **2052 bytes** minimum before glyph bodies.

### Glyph functions

**Each glyph is a compiled x86 function**, not encoded bitmap data. `@G_Print@16` calls each glyph function directly, passing the destination framebuffer position in registers.

**Confirmed calling convention** (traced from `@G_Print@16` at `0x004986B0` + glyph body at raw file offset `0xA25`):

| Register | Role |
|----------|------|
| `EDI` | Current row pointer in destination framebuffer |
| `ECX` | Scanline stride (bytes per row) |
| `AL` | Pixel color value |

**Instruction pattern:**

| Sequence | Meaning |
|----------|---------|
| `03 F9` = `ADD EDI, ECX` | Advance to next row |
| `88 07` = `MOV [EDI], AL` | Write pixel at current position |
| `ADD EDI, ECX` alone | Skip a blank row (no pixel written) |
| `C3` = `RET` | End of glyph |

**`0xC3` = `RET`** — the blank/space glyph is a single-byte function that returns immediately, writing nothing. Control characters (ASCII 0–31) and space (ASCII 32) all point to `0xC3` bytes (VA 0x1804–0x1824).

Printable character functions begin at VA 0x1825 (raw file offset `0xA25`). Confirmed disassembly of ASCII 33 (`!`):

```asm
ADD  EDI, ECX      ; row 0 — lit (bar)
MOV  [EDI], AL
ADD  EDI, ECX      ; row 1 — lit
MOV  [EDI], AL
ADD  EDI, ECX      ; row 2 — lit
MOV  [EDI], AL
ADD  EDI, ECX      ; row 3 — blank
ADD  EDI, ECX      ; row 4 — lit (dot)
MOV  [EDI], AL
ADD  EDI, ECX      ; row 5 — blank
ADD  EDI, ECX      ; row 6 — trailing advance
RET
```

7 row advances for a font named `4X6` suggests `cFont[0]` (font height) = 7 — the cell is 6 glyph rows + 1 inter-line spacing row. The raw bytes `{03 F9 88 07}` are two x86 instructions, not an encoded bitmap format.

**Ghidra navigation note:** When importing a FNT file as a raw binary with base `0x1000`, the CODE section (file offset `0x200`) appears at Ghidra address `0x1200`. Loaded VAs (e.g. `0x1825`) correspond to Ghidra address `0x1000 + file_offset = 0x1000 + (VA - 0x1000 + 0x200)` = VA + `0x200`.

### $$DOSX metadata

The $$DOSX section (512 bytes) contains a small header. Both 4X6.FNT and 4X12.FNT show identical $$DOSX values (`u16[4]=16, u16[5]=6`). These are system constants shared by all FNT files, not per-file cell dimensions.

## Toolkit Roadmap

FONT struct layout, glyph encoding, and all per-file metrics are confirmed. No further RE required before codec implementation:

- New `lib/src/fnt.cpp` + `lib/include/ft/fnt.h` — parse FONT struct, enumerate glyph functions, extract width/height metrics
- New `cli/cmd_fnt.cpp` — `ft fnt unpack <file.FNT> -o <dir>/` renders each glyph by executing the x86 function against a pixel buffer; writes `metrics.csv` with `{char, width, height}` per row
- GUI: `fnt_viewer.h/cpp` in `gui/src/editors/` for interactive glyph grid preview

## Related

- [PIC.md](PIC.md) — 8-bit indexed bitmaps, also in FA_1.LIB
- [HUD.md](HUD.md) — HUD layouts that consume font data
