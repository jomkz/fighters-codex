---
format: PAL
name: VGA Palette
extensions: [".PAL"]
category: graphics
endianness: none
spec:
  status: complete
codec:
  direction: round-trip
  byte_identical: true
  lib: [lib/src/pal.cpp]
  commands: [pal]
  gui: [gui/src/editors/pal_editor.cpp]
  tests: [tests/test_pal.cpp]
  fuzz: []
  fixtures:
    synthetic: true
    real_manifest: true
related: [PIC, LIB]
---

# PAL — VGA Palette (.PAL)

A `.PAL` file is exactly **768 bytes**: 256 RGB triplets in VGA 6-bit format.
No header. `PALETTE.PAL`, the game's primary palette, is stored compressed
(flags=4) inside `FA_2.LIB`.

## Tools

### fx

```
fx pal info <file.PAL>     # summary
fx pal dump <file.PAL>     # entry-by-entry listing
```

### fxs

Opening a `.PAL` record (in a LIB or standalone) shows a 16×16 swatch grid
with per-index RGB tooltips, and can set the palette applied to PIC/CB8
previews — see [gui.md](../../gui.md#palette-viewer-and-switcher).

### Other Tools

No standard image editor reads the raw 6-bit VGA palette format directly. Use a
hex editor to view or patch individual entries (3 bytes per color: R, G, B at
6-bit scale).

- **HxD** — free, Windows; lightweight and fast for small binary files like `.PAL`
- **VS Code** + [Hex Editor](https://marketplace.visualstudio.com/items?itemName=ms-vscode.hexeditor) — free, cross-platform; convenient if already using VS Code for text editing
- **010 Editor** `$` — paid; binary templates allow a labelled struct view over the palette entries

## File Layout

Single-byte values only; no multi-byte integers.

| Offset | Size | Type      | Description |
|--------|------|-----------|-------------|
| `0x00` | 768  | u8[256×3] | R, G, B for each palette index 0..255 |

Each channel is stored as a **6-bit value** (range 0–63). Scale to 8-bit for
display or PNG output:

```c
uint8_t to_8bit(uint8_t v6) { return (v6 << 2) | (v6 >> 6); }
```

### The System Palette

- `PALETTE.PAL` is the game's primary palette. It is stored compressed (flags=4)
  inside `FA_2.LIB` and must be extracted before decoding paletted PIC files.
- Each game version (USNF, ATF, FA) ships its own palette — do not mix them.
- Palette index **0xFF (255)** is reserved as transparent across all PIC
  sub-formats.

## Round-Trip Notes

The format has no derived or redundant fields, so decode → encode reproduces
the input byte-for-byte; `tests/test_pal.cpp` asserts it, alongside the 6-bit
→ 8-bit scaling values.

## Related

**Formats:** [PIC](PIC.md) — consumes this palette (inline palettes use the
same 6-bit encoding); [LIB](LIB.md) — carries `PALETTE.PAL` DCL-compressed.
