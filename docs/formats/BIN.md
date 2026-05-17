# Binary Lookup Tables (.BIN)

FA_2.LIB contains 6 `.BIN` files. All are flat lookup tables used by the engine's color blending and insignia systems.

## File Inventory

| File | Size | Purpose |
|------|------|---------|
| INSIGMAP.BIN | 256 B | Insignia slot → palette/asset mapping |
| MIX2.BIN | 512 B | Non-linear half-intensity reduction table (gamma-corrected) |
| MIX2L.BIN | 512 B | Linear half-intensity table: `output = floor(input / 2)` |
| MIX4.BIN | 1024 B | Non-linear quarter-intensity reduction table (gamma-corrected) |
| MIX4L.BIN | 1024 B | Linear quarter-intensity table: `output = floor(input / 4)` |
| VFONTPAL.BIN | 48 B | 16-entry VGA 6-bit palette for video briefing font rendering |

---

## MIX Tables (MIX2, MIX2L, MIX4, MIX4L)

Used for palette-mode transparency and color blending. When the engine blends two
8-bit palette indices, it sums their intensity values and uses a MIX table to look
up the resulting index.

- **MIX2L** — 512-byte table; `output[i] = floor(i / 2)`. Exact 50% linear reduction.
- **MIX4L** — 1024-byte table; `output[i] = floor(i / 4)`. Exact 25% linear reduction.
- **MIX2** — 512-byte non-linear variant; applies gamma correction. Maps 0→0, 255→128, 511→255 but with a non-linear curve (square-ish at low end, linear at high end).
- **MIX4** — 1024-byte non-linear variant; same gamma-correction approach for quarter-intensity blending.

The `L` suffix = **L**inear (no gamma), without `L` = perceptually-corrected.

---

## VFONTPAL.BIN (48 bytes = 16 × 3-byte VGA RGB entries)

Palette for rendering text in video briefing sequences. VGA 6-bit format (values 0–63 per channel).

| Index | 6-bit RGB | 8-bit RGB | Role |
|-------|-----------|-----------|------|
| 0 | `3F 00 3F` | (255, 0, 255) | Transparent / color key |
| 1 | `0E 17 29` | (56, 93, 166) | Blue text — bright |
| 2 | `0C 15 26` | (48, 85, 154) | Blue text |
| 3 | `0B 14 22` | (44, 81, 138) | Blue text |
| 4 | `09 10 1E` | (36, 65, 121) | Blue text |
| 5 | `07 0C 14` | (28, 48, 81)  | Blue text |
| 6 | `04 09 0F` | (16, 36, 60)  | Blue text |
| 7 | `02 03 07` | (8, 12, 28)   | Blue text — dark |
| 8 | `04 34 3F` | (16, 211, 255) | Cyan text — bright |
| 9 | `03 2C 36` | (12, 178, 219) | Cyan text |
| 10 | `02 24 2C` | (8, 146, 178) | Cyan text |
| 11 | `01 1D 23` | (4, 117, 142) | Cyan text |
| 12 | `01 15 1A` | (4, 85, 105)  | Cyan text |
| 13 | `01 0E 11` | (4, 56, 69)   | Cyan text |
| 14 | `00 06 08` | (0, 24, 32)   | Cyan text — dark |
| 15 | `3F 3F 3F` | (255, 255, 255) | White |

Index 0 (magenta) is the standard VGA transparency key. Indices 1–7 render a blue gradient (likely shadow or secondary text). Indices 8–14 render a cyan gradient (primary text). Index 15 is white (highlight).

---

## INSIGMAP.BIN (256 bytes)

Flat 256-entry byte array. Entry 0 = `0x00`; all remaining 255 entries = `0x3B` (59 decimal). The `0x3B` fill suggests a "no insignia" sentinel — most insignia slots are unused, with the actual insignia assets referenced by the pilot save file fields at offsets `0x6E`–`0x94` ([PLT.md](PLT.md)).

---

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 6 |

## Related

- [PLT.md](PLT.md) — pilot save files reference insignia asset IDs cross-referenced by INSIGMAP.BIN
- [PAL.md](PAL.md) — main VGA palette; VFONTPAL.BIN is a separate 16-color subset for video text
