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

## TODO — Deep Dive

- Identify the second `.HGR` filename (likely a carrier or alternate airbase variant)
- Disassemble to identify the hangar layout table (aircraft slot positions, icon placement, camera angle)

## Related

- [PIC.md](PIC.md) — `h_airb.PIC` and `SELICONS.PIC` are PIC atlas files
- [MNU.md](MNU.md) — menus that transition to the hangar screen
