---
format: RGN
name: Installer UI Region Map
extensions: [".RGN"]
category: installer
endianness: little
spec:
  status: complete
codec:
  direction: none
  issue: 108
  fixtures:
    synthetic: false
    real_manifest: false
related: [SSF]
---

# RGN — Installer UI Region Map (.RGN)

`.RGN` files define named rectangular regions within the EA installer's bitmap
UI assets. They are used for two purposes: hit-test regions (POSTER.RGN maps
screen clicks to button labels) and sprite-atlas lookups (BUTTONS.RGN maps
button labels to pixel regions within the button-state sprite sheet). Both
live in the Disc 1 root alongside the installer executable — not packed into
any LIB archive.

## File Layout

All multi-byte integers are little-endian.

| Offset | Size | Type | Description |
|--------|------|------|-------------|
| `0x00` | 4 | u32 | Record count |
| `0x04` | 40×N |  | Records (40 bytes each) |

Record layout:

| Offset | Size | Type | Description |
|--------|------|------|-------------|
| `+0x00` | 4  | char[4] | Name — ASCII label, null-padded (e.g. `B1\0\0`, `NE1\0`) |
| `+0x04` | 4  | u32 | Vertex count (always 4 in both files) |
| `+0x08` | 32 | u32[8] | 4 × (x, y) — clockwise from top-left: (x_min, y_min), (x_max, y_min), (x_max, y_max), (x_min, y_max) |

All 104 records across both files have vertex count = 4 (axis-aligned
rectangles). The format supports arbitrary polygons (count field is variable),
but all current records are rectangles.

The count field at offset 0 fully determines the file size (`4 + count × 40`):

| File | Count | Expected size | Actual size |
|------|-------|--------------|-------------|
| POSTER.RGN | 8 | 324 bytes | 324 bytes ✓ |
| BUTTONS.RGN | 96 | 3,844 bytes | 3,844 bytes ✓ |

## File Inventory

| File | Size | BMP companion | Role |
|------|------|--------------|------|
| `POSTER.RGN` | 324 bytes | `POSTER.BMP` (468×451, 8bpp) | 8 hit-test click regions on the splash screen |
| `BUTTONS.RGN` | 3,844 bytes | `BUTTONS.BMP` (1500×456, 8bpp) | 96 sprite-atlas entries for all button states |

### POSTER.RGN — Splash Screen Click Regions

8 records. Coordinates are in screen pixels within the 468×451 POSTER.BMP
image. Buttons cover the lower portion of the poster (y = 328–442).

| Name | x min | x max | y min | y max | Width | Height |
|------|-------|-------|-------|-------|-------|--------|
| B1 | 0 | 128 | 328 | 366 | 128 | 38 |
| B2 | 128 | 340 | 328 | 366 | 212 | 38 |
| B3 | 340 | 468 | 328 | 366 | 128 | 38 |
| B4 | 0 | 234 | 366 | 404 | 234 | 38 |
| B5 | 234 | 468 | 366 | 404 | 234 | 38 |
| B6 | 0 | 128 | 404 | 442 | 128 | 38 |
| B7 | 128 | 340 | 404 | 442 | 212 | 38 |
| B8 | 340 | 468 | 404 | 442 | 128 | 38 |

Layout (three rows, variable column widths):

```
y=328 ┌──B1──┬────B2────┬──B3──┐
y=366 ├────B4────┬────B5────┤
y=404 ├──B6──┬────B7────┬──B8──┤
y=442 └──────┴──────────┴──────┘
       x=0   128        340   468
```

### BUTTONS.RGN — Button Sprite Atlas

96 records. Coordinates reference pixel regions within the 1500×456
BUTTONS.BMP sprite sheet. The installer engine blits the appropriate region
onto the screen when rendering button states.

**Naming scheme** — format: `[state][row][col]`

| Position | Values | Meaning |
|----------|--------|---------|
| state | N | Normal |
| | F | Focus / hover |
| | P | Pressed / active |
| | D | Disabled |
| row | E | Top row group (y = 0–152) |
| | F | Middle row group (y = 152–304) |
| | G | Bottom row group (y = 304–456) |
| col | 1–8 | Column index within the sprite sheet |

**Sprite sheet layout** — each state row contains 4 sub-rows of 38px each (one
per state: N/F/P/D stacked vertically). Each of the 8 columns has a fixed
x-range:

| Col | x min | x max | Width |
|-----|-------|-------|-------|
| 1 | 0 | 128 | 128 |
| 2 | 128 | 340 | 212 |
| 3 | 340 | 468 | 128 |
| 4 | 468 | 702 | 234 |
| 5 | 702 | 936 | 234 |
| 6 | 936 | 1064 | 128 |
| 7 | 1064 | 1276 | 212 |
| 8 | 1276 | 1404 | 128 |

Row group y ranges: E = 0–152, F = 152–304, G = 304–456. Each group has 4
state sub-rows of 38px each. Total sprite sheet used area: 1404×456 px (96 px
at the right of the 1500px-wide BMP is blank/padding).

## Related

**Formats:** [SSF](SSF.md) — EA installer script that drives the UI;
references button labels by name.
