# Hangar Screen (.HGR)

FA_2.LIB contains 2 `.HGR` files. Each defines a hangar screen — the aircraft selection and loadout interface shown at an airbase. Each is a **Win32 PE DLL** loaded at runtime via `LoadLibrary`.

## File Inventory

| File | Purpose |
|------|---------|
| H_AIRB.HGR | Air base hangar screen |
| (second file TBD) | — |

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
- Aircraft slot entries start at **offset +0x2D** from the DLL base: 30 × 8-byte entries
  - Bytes [0..1] of each slot: cleared on load (selection state flags, zeroed)
  - Bytes [2..7]: position/icon data (not yet decoded)
- Iterates up to `DAT_00529128` aircraft types (capped at 100)
- Builds X coordinate array (`local_640`: 400 shorts) and Y coordinate array (`local_320`: 400 shorts) for aircraft screen positions

## TODO — Deep Dive

- Identify the second `.HGR` filename (likely `H_AIRB2.HGR` — carrier hangar)
- Decode the 8-byte slot entry structure (icon position, angle, camera setting)
- Confirm meaning of `FUN_004a6cc0(pcVar5, 0x8104)` sub-resource key

## Related

- [PIC.md](PIC.md) — `h_airb.PIC` and `SELICONS.PIC` are PIC atlas files
- [MNU.md](MNU.md) — menus that transition to the hangar screen
