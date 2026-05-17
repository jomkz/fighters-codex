# Heads-Up Display (.HUD)

FA_2.LIB contains 46 `.HUD` files — one per aircraft type (e.g. `A7.HUD`, `F22.HUD`). Each defines the cockpit HUD layout for that aircraft. Each is a **Win32 PE DLL** loaded at runtime via `LoadLibrary`.

## Format

Win32 PE DLL. All observed `.HUD` files decompressed to **4608 bytes**.

## Content

String analysis of `F22.HUD` and `B2.HUD` reveals the asset reference pattern:

| String | Role |
|--------|------|
| `~f22` / `~b2` | Aircraft 3D model reference |
| `~f22h` / `~b2h` | HUD overlay image (heads-up display graphic) |
| `~f22s` / `~b2s` | HUD symbol set |
| `hudsym` | HUD symbol font (`HUDSYM*.FNT`) |
| `GEAR`, `FLAP`, `BRAKE` | Indicator label strings |
| `~f22_p` / `~b2_p` | Aircraft propulsion/engine panel reference |
| `~f22_w` / `~b2_w` | Weapons panel reference |
| `winfont` | Window font (`WIN*.FNT`) reference |

The `~` prefix indicates LIB-resident asset references. The HUD DLL binds its aircraft-specific assets at load time using these names.

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 46 |

## CODE Section Layout (Confirmed)

HUD files use **Phar Lap PE format** (signature `PL\0\0`). The CODE section is a **pure data structure** — no imports, no dispatch table, no x86 code. The engine loads HUD assets by name at runtime.

### String layout (A7.HUD)

| VA | String | Role |
|----|--------|------|
| 00001001 | `~a7` | Aircraft model base name |
| 0000100E | `~a7h` | High-AoA/hook variant |
| 0000101B | `~a7s` | Speed-brake variant |
| 00001038 | `hud` | HUD identifier |
| 00001051 | `hudsym` | HUD symbol set |
| 0000113C–000011A4 | `~a7_l/c/r`, `~a7_lh/ch/rh`, `~a7_ls/cs/rs` | Left/centre/right gauge states (normal, high, small) |
| 00001245 | `GEAR`, `FLAP`, `BRAKE`, `HOOK` | Warning indicator labels |
| 00001275 | `~a7_p`, `~a7_w` | Engine/weapons panel sprites |
| 00001297 | `winfont` | Window font reference |

### Coordinate data

Starting at VA 0x1062, a block of signed s16 pairs encodes gauge positions as **offsets from the HUD anchor point**.

### Per-aircraft anchor points (confirmed)

| Aircraft | Anchor (x, y) | Notes |
|----------|--------------|-------|
| A7 | (320, 100) | Centred horizontally |
| F22 | (249, 100) | Offset left — narrower HUD layout |
| F14 | (349, 114) | Offset right + lower — wide-cockpit geometry |

All HUD files have identical CODE virtual size (`0x2BB`), confirming a fixed-size data structure regardless of aircraft.

### F22-specific differences

F22.HUD omits the `~f22_l/c/r` sub-panel sprites entirely — F22 has no separate left/centre/right gauge panels. It also uses `BAY` (weapons bay indicator) instead of `HOOK` (arresting hook present on carrier aircraft such as the A7 and F14).

Sample offsets (A7.HUD):

| VA | (dx, dy) | Probable gauge |
|----|----------|----------------|
| 00001062 | (-52, -40) | — |
| 00001066 | (105, 111) | — |
| 0000106A | (-25, -28) | — |
| 0000106E | (48, 86) | — |

## Toolkit Roadmap

- New `lib/src/hud.cpp` + `lib/include/ft/hud.h` — parse sprite name table and coordinate block
- New `cli/cmd_hud.cpp` — `ft hud dump <file.HUD>` prints gauge table as JSON `[{sprite, dx, dy}]`
- GUI: overlay viewer that renders gauge positions on a 640×480 canvas

## TODO — Deep Dive

- Map each (dx, dy) offset to a specific gauge type (airspeed, altitude, heading, weapons)
- Confirm anchor point encoding (two u16s at a fixed VA offset, or derived from another field?)
- Identify all gauge state variants (`_l`/`_lh`/`_ls` suffixes — likely normal/high-AoA/stowed states)
- Diff A7 vs F22 offset tables byte-by-byte to isolate aircraft-specific gauge positions

## Related

- [BRF.md](BRF.md) — `.PT` aircraft type records that likely reference the corresponding `.HUD`
- [FNT.md](FNT.md) — font files used to render HUD text elements
- [PIC.md](PIC.md) — bitmap assets used for HUD graphical elements
