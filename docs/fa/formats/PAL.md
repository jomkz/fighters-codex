# PAL -- VGA Palette (.PAL)

A `.PAL` file is exactly **768 bytes**: 256 RGB triplets in VGA 6-bit format. No header.

```
Offset  Size  Description
------  ----  -----------
0       768   256 x 3 bytes: R, G, B for each palette index 0..255
```

Each channel is stored as a **6-bit value** (range 0-63).

Scale to 8-bit for display or PNG output:
```c
uint8_t to_8bit(uint8_t v6) { return (v6 << 2) | (v6 >> 6); }
```

## The System Palette

- `PALETTE.PAL` is the game's primary palette. It is stored compressed (flags=4) inside
  `FA_2.LIB` and must be extracted before decoding paletted PIC files.
- Each game version (USNF, ATF, FA) ships its own palette -- do not mix them.
- Palette index **0xFF (255)** is reserved as transparent across all PIC sub-formats.

## Applications

No standard image editor reads the raw 6-bit VGA palette format directly. Use a hex
editor to view or patch individual entries (9 bytes per color: R, G, B at 6-bit scale).

- **HxD** — free, Windows; lightweight and fast for small binary files like `.PAL`
- **VS Code** + [Hex Editor](https://marketplace.visualstudio.com/items?itemName=ms-vscode.hexeditor) — free, cross-platform; convenient if already using VS Code for text editing
- **010 Editor** `$` — paid; binary templates allow a labelled struct view over the palette entries
