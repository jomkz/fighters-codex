# Hangar Screen (.HGR)

FA_2.LIB contains 2 `.HGR` files. Each defines a hangar screen — the aircraft selection and loadout interface shown at an airbase. Each is a **Win32 PE DLL** loaded at runtime.

## File Inventory

| File | Purpose |
|------|---------|
| H_AIRB.HGR | Air base hangar screen |
| H_AIRB2.HGR | Carrier hangar screen (confirmed via FA_2.LIB string scan) |

## Content

String analysis of `H_AIRB.HGR` reveals asset references:

- **`h_airb.PIC`** — hangar background image (appears twice, likely for foreground and background layers)
- **`SELICONS.PIC`** — aircraft selection icons displayed in the hangar UI

## Format

Win32 PE DLL. `H_AIRB.HGR` decompresses to **4608 bytes**.

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 2 |

## Loading Mechanism (Confirmed)

`FUN_00480150` (HGR load trigger, called from `_MISSIONInit2@0`):
1. Calls `FUN_004809d0` — initialises `?hangarName@@3PADA` (`0x004fb1e8`) from `DAT_004fbbf0`, and copies `"ord_air3.PIC"` to `DAT_004fb1f8`
2. `DAT_004fb198` selects carrier (non-zero) vs. land-base hangar
3. Calls `FUN_004543c0(0, &DAT_004fb1e8, '\0', '\x01')` — actual HGR file loader

`FUN_004543c0` (HGR file loader) — confirmed structure:
- Loads HGR DLL via `FUN_004a6ae0(name, 0x8000)`
- When `param_4 == '\0'` (standard load): skips first 13 bytes (`pcVar5 = pcVar6 + 0xd`)
- Loads embedded sub-resource via `FUN_004a6cc0(pcVar5, 0x8104)`
- Aircraft slot entries start at **offset +0x27** from the sub-resource base: 30 × 8-byte entries
- Iterates up to `DAT_00529128` aircraft types (capped at 100)
- Builds X coordinate array (`local_640`: 400 shorts) and Y coordinate array (`local_320`: 400 shorts) for aircraft screen positions

## Slot Entry Layout — Confirmed (2026-05-19)

`FUN_004558f0` (VA 0x4558f0) is the hangar slot reader. It iterates the 30 × 8-byte slot entries starting at `param_5 + 0x27` (where `param_5` is the sub-resource handle returned by `FUN_004543c0`). Each 8-byte entry has this confirmed layout:

| Byte offset | Type | Field | Notes |
|-------------|------|-------|-------|
| +0 | s16 | `x_offset` | Aircraft hangar X position; `-1` = slot unused (empty sentinel) |
| +2 | s16 | `y_offset` | Aircraft hangar Y position |
| +4 | s16 | `angle_index` | Index into angle-correction table `asStack_b` (0-based) |
| +6 | s16 | `occupied` | Slot occupancy flag: `0` = free, `1` = assigned |

Screen position is computed as:
```c
screen_x = asStack_b[angle_index * 2]     + x_offset;
screen_y = asStack_b[angle_index * 2 + 1] + y_offset;
```
where `asStack_b[5]` is a 5-short correction table loaded from the sub-resource.

Slot search (param_1 == '\0', standard assign): finds first slot where `x_offset != -1` and `occupied == 0`, copies its 8 bytes to output buffer, sets `occupied = 1`.

Carrier assign path (param_1 != '\0'): walks a separate slot-index table at `param_5 + 0x117` (4 ints per entry, up to 4 entries); finds first entry pointing to a free slot; marks all referenced slots as occupied.

## Related

- [PIC.md](PIC.md) — `h_airb.PIC` and `SELICONS.PIC` are PIC atlas files
- [MNU.md](MNU.md) — menus that transition to the hangar screen
