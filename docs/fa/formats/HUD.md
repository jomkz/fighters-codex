# Heads-Up Display (.HUD)

FA_2.LIB contains 46 `.HUD` files — one per aircraft type (e.g. `A7.HUD`, `F22.HUD`). Each defines the cockpit HUD layout for that aircraft. Each is a **Win32 PE DLL** loaded at runtime.

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

All HUD files have identical CODE virtual size (`0x2BB` = 699 bytes), confirming a fixed-size struct regardless of aircraft.

### Loading mechanism

`FUN_00406040` (HUD init, called at aircraft load time):
1. Loads the HUD DLL by name via `FUN_004a6ae0`
2. Bulk-copies the entire CODE section (0xAC dwords + 2 bytes = **690 bytes**) to `DAT_00521360`
3. Scales every gauge parameter left by the display xscale/yscale factor (`bVar3`/`bVar4`)

The anchor point (`DAT_00521d94`/`DAT_00521d96`) is **not** read directly from the HUD file — it is computed dynamically at runtime via a smooth-follow interpolation that tracks the player aircraft's screen position.

### String layout (A7.HUD)

| VA | String | Role |
|----|--------|------|
| 00001001 | `~a7` | Aircraft model base name |
| 0000100E | `~a7h` | High-AoA/hook variant |
| 0000101B | `~a7s` | Speed-brake variant |
| 00001038 | `hud` | HUD identifier |
| 00001051 | `hudsym` | HUD symbol set |
| 0000113C–000011A4 | `~a7_l/c/r`, `~a7_lh/ch/rh`, `~a7_ls/cs/rs` | Left/centre/right sub-panel states |
| 00001245 | `GEAR`, `FLAP`, `BRAKE`, `HOOK` | Warning indicator labels |
| 00001275 | `~a7_p`, `~a7_w` | Engine/weapons panel sprites |
| 00001297 | `winfont` | Window font reference |

### Sub-panel sprite suffix semantics (confirmed)

| Suffix pattern | Meaning |
|----------------|---------|
| `_l` / `_c` / `_r` | Left / centre / right panel — normal state |
| `_lh` / `_ch` / `_rh` | Left / centre / right panel — high-AoA state |
| `_ls` / `_cs` / `_rs` | Left / centre / right panel — stowed/small state |

F22.HUD omits all `_l/c/r` sprites entirely — the F22 has no separate sub-panels. It also uses `BAY` (weapons bay indicator) instead of `HOOK`.

### Gauge parameter layout (confirmed)

All gauge positions are stored as signed s16 offsets from the HUD anchor point. Confirmed by tracing the HUD draw functions (`FUN_00406040`, `?HUDDrawHeading`, `?HUDDrawSpeed`, `?HUDDrawAlt`, `FUN_00408e20` (HVel indicator), `?HUDDrawWeaponInfo`, `?HUDDrawRangeInfo`) in FA.EXE via Ghidra.

After loading, the struct is resident at `DAT_00521360`. Field offsets within the copied struct:

| Struct offset | Global | Gauge | Field |
|---------------|--------|-------|-------|
| `0x1E1` | `DAT_00521541` | Heading tape | width (pixels) |
| `0x1E5` | `DAT_00521545` | Heading tape | dy from anchor |
| `0x1ED` | `DAT_0052154d` | Heading tape | tick spacing (dy) |
| `0x1F7` | `DAT_00521557` | Speed tape | dx from anchor |
| `0x1FB` | `DAT_0052155b` | Speed tape | dy from anchor |
| `0x1FF` | `DAT_0052155f` | Speed tape | height (pixels) |
| `0x209` | `DAT_00521569` | Speed tape | tick increment |
| `0x214` | `DAT_00521574` | Altitude tape | dx from anchor |
| `0x218` | `DAT_00521578` | Altitude tape | dy from anchor |
| `0x21C` | `DAT_0052157c` | Altitude tape | height (pixels) |
| `0x226` | `DAT_00521586` | Altitude tape | tick increment |
| `0x231` | `DAT_00521591` | Flight path marker | dx from anchor |
| `0x233` | `DAT_00521593` | Flight path marker | dy from anchor |
| `0x235` | `DAT_00521595` | Flight path marker | box half-width |
| `0x237` | `DAT_00521597` | Flight path marker | box half-height |
| `0x238` | `DAT_00521598` | (unknown) | No cross-references found |
| `0x239` | `DAT_00521599` | Lock indicator flag A | 3-state lock display; checked against `missile+0xa6 & 0x10` |
| `0x23A` | `DAT_0052159a` | Lock indicator flag B | Paired with A; selects state 5 (no lock) vs 6 (partial) |
| `0x23B` | `DAT_0052159b` | HUD center dot enable | Non-zero: draw center pip and radar velocity vector |
| `0x23C` | `DAT_0052159c` | ECM bar enable | Non-zero: enables ECM/threat bar draw (`FUN_00408c8b`) |
| `0x23D` | `DAT_0052159d` | Active-lock threat enable | Combined with 0x23C for active-lock threat bar |
| `0x23E` | `DAT_0052159e` | HVel altitude max | i16; HVel indicator (`FUN_00408e20`) hidden above this altitude; also radar lock altitude gate |
| `0x240` | `DAT_005215a0` | Lead indicator enable | Non-zero: draw lead angle / velocity prediction overlay |
| `0x241` | `DAT_005215a1` | Score indicator dx | i8; X offset for score/fuel threshold indicator (`FUN_004078b0`) |
| `0x243` | `DAT_005215a3` | Score indicator dy | i8; Y offset for score/fuel threshold indicator |
| `0x245` | `DAT_005215a5` | Advisory icon C | 8-byte icon data; drawn when `DAT_0050cfef & 0x040` |
| `0x24D` | `DAT_005215ad` | Advisory icon A | 8-byte icon data; drawn when `DAT_0050cfef & 0x100`; `!= ' '` → advisory active |
| `0x255` | `DAT_005215b5` | Advisory icon B | 8-byte icon data; drawn when `DAT_0050cfef & 0x080` |
| `0x25D` | `DAT_005215bd` | Advisory icon D | 8-byte icon data; drawn when `DAT_0050cfef & 0x200` or `(& 0x400) && (DAT_0050d322 & 2)` |
| `0x265` | `DAT_005215c5` | **Warning lights** | dx from anchor (A7=65, F22=70) — `FUN_00407930` |
| `0x267` | `DAT_005215c7` | Warning lights | dy from anchor (A7=−38, F22=−46) |
| `0x269` | `DAT_005215c9` | **Throttle/engine readout** | dx from anchor (A7=−65, F22=−70) — `FUN_00407a00` |
| `0x26B` | `DAT_005215cb` | Throttle/engine readout | dy from anchor (A7=−38, F22=−46) |
| `0x26D` | `DAT_005215cd` | Weapon info | dx from anchor |
| `0x26F` | `DAT_005215cf` | Weapon info | dy from anchor |
| `0x271` | `DAT_005215d1` | Range info | dx from anchor |
| `0x273` | `DAT_005215d3` | Range info | dy from anchor |

## `DAT_0050cfef` Advisory/State Flags (Confirmed)

`DAT_0050cfef` is the HUD state flags word. Bits are set by the game's subsystems at each simulation tick; `FUN_00407930` and the tape render functions read them to gate icon and display variants.

| Bit | Hex | Source function | Meaning |
|-----|-----|-----------------|---------|
| 0 | `0x00001` | `_DAMAGEDoHit@12` damage state `0x50d3ff` | Aircraft damage indicator level 1 |
| 1 | `0x00002` | `_DAMAGEDoHit@12` damage state `0x50d400` | Aircraft damage indicator level 2 |
| 2 | `0x00004` | `_DAMAGEDoHit@12` damage state `0x50d401` | Aircraft damage indicator level 3 |
| 3 | `0x00008` | `_DAMAGEDoHit@12` damage state `0x50d3f7` (also clears bit 5) | Aircraft damage / engine-out state — cleared bit 5 indicates afterburner disabled by damage |
| 4 | `0x00010` | `_DAMAGEDoHit@12` damage state `0x50d40c` | Aircraft damage indicator level 4 |
| 5 | `0x00020` | `FUN_00407a00`; cleared by `_DAMAGEDoHit@12` state `0x50d3f7` | Aircraft has afterburner AND throttle is at max (`DAT_0050d06e == 0x6400`) — shows `"THR: AFT"` instead of numeric throttle; cleared when damage state removes afterburner |
| 6 | `0x00040` | `FUN_00407ee0`, `FUN_00408420`, `FUN_00407930`; set/cleared by `FUN_00451c90` (speedbrake actuator) via input 0x67 | Advisory icon C active — speedbrake/airbrake deployed; speed tape swaps live reference marker to approach-speed source (`DAT_0050d3aa`); altitude tape draws approach-altitude bracket markers (`DAT_0050d0aa`, `DAT_0050d3ae`) |
| 7 | `0x00080` | `FUN_00407930`; set/cleared by `FUN_00451d70` (flap actuator) via input 0x62 | Advisory icon B active — flap deployed |
| 8 | `0x00100` | `FUN_00407930`; set/cleared by `FUN_00451b60` (gear actuator) via input 0x66 | Advisory icon A active — landing gear deployed (down) |
| 9 | `0x00200` | `FUN_00407930`; set/cleared by `FUN_00452630` (tailhook actuator) via input 0x6f | Advisory icon D active (single-player path) — tailhook deployed |
| 10 | `0x00400` | `FUN_00407930`; set/cleared by `FUN_00451c30` (bay-door actuator) via input 0x68 | Advisory icon D active (multiplayer path, also requires `DAT_0050d322 & 2`) — weapon bay door / hook variant open |
| 11 | `0x00800` | `FUN_004530a0` (weapon-state scan, each tick) | Active weapon lock — at least one weapon has ammo and an acquired lock |
| 12 | `0x01000` | `FUN_00407a00`; toggled by `FUN_00416380` via input 0x61 | Flight-lock / autopilot active — replaces throttle/G readout with lock sprite (`DAT_004ebf94`) |
| 13 | `0x02000` | `FUN_00414690` case 0x61 (autopilot key handler) | Autopilot ILS/ACLS sub-mode — set alongside bit 12 when flight mode is 6 and aircraft has ACLS capability (PT+0xe9 ≠ 0); gates carrier-approach glide-slope computation |
| 14 | `0x04000` | `?MPReceive@@YGDXZ` (0x46C980 = FUN_0046c98f, decompile failed — writes at 0x46db2b) in multiplayer; unanalyzed code at 0x4bc177/0x4bc190 may also write it in single-player. No function in FA.EXE was found to set this bit via a direct `OR [mem], 0x4000` constant — the SP writer path remains unknown. Read in `FUN_004164b0` during ejection states 0x11/0x12 in conjunction with `DAT_0050d0b1` (nearest entity pointer) and entity+0xFB range comparison; also gates aerodynamic integrator reset (`DAT_005451e8`/`ec`, `DAT_00545200`). | Unknown — likely a network-synced or proximity-alert advisory state |
| 15 | `0x08000` | `FUN_0049fb70` via `FUN_00452140` — OR'd into `DAT_0050cfef` every 5 ticks; returned when glidepath ratio is 240–(dist+299) | Carrier glideslope: on glidepath |
| 16 | `0x10000` | `FUN_0049fb70` via `FUN_00452140` — returned when glidepath ratio is in the (dist+300)–(dist+599) band | Carrier glideslope: above glidepath |
| 17 | `0x20000` | `FUN_0049fb70` via `FUN_00452140` — returned when glidepath ratio is 1–239 | Carrier glideslope: below glidepath |
| 18 | `0x40000` | `FUN_0049fb70` via `FUN_00452140` — returned when glidepath ratio < 1 or no carrier in range | Carrier glideslope: critically below glidepath (wave-off) |
| 28 | `0x10000000` | `FUN_00407ee0`, `FUN_00408420`; set by `_DAMAGEDoHit@12` state `0x50d40e` | Classified / redacted display — speed tape substitutes string at `0x004ebfbc`; altitude tape substitutes `s_XXXXX_004ebfd8` ("XXXXX"); tape tick-mark rendering skipped entirely; also triggered by aircraft-out-of-control damage state |
| 29 | `0x20000000` | `_DAMAGEDoHit@12` damage state `0x50d410` | Emergency state — aircraft critical / imminent crash indicator |
| 30 | `0x40000000` | `_DAMAGEDoHit@12` damage state `0x50d40f` (conditional on `FUN_004562f0(3)` result) | Emergency state variant A — aircraft spinning / uncontrolled flight |
| 31 | `0x80000000` | `_DAMAGEDoHit@12` damage state `0x50d40f` (conditional on `FUN_004562f0(3)` result) | Emergency state variant B — aircraft spinning / uncontrolled flight (alternate roll) |

Advisory icon names (label strings embedded in the HUD file, order confirmed from A7.HUD string block at VA `0x00001245`):

| Icon | Bit | Struct offset | Label in A7.HUD | Label in F22.HUD | Subsystem |
|------|-----|---------------|-----------------|------------------|-----------|
| A | `0x100` | `+0x245` | `GEAR` | `GEAR` | `FUN_00451b60` — gear actuator (input 0x66) |
| B | `0x080` | `+0x24D` | `FLAP` | `FLAP` | `FUN_00451d70` — flap actuator (input 0x62) |
| C | `0x040` | `+0x255` | `BRAKE` | `BRAKE` | `FUN_00451c90` — speedbrake actuator (input 0x67) |
| D | `0x200`/`0x400` | `+0x25D` | `HOOK` | `BAY` | `FUN_00452630` (tailhook, input 0x6f) / `FUN_00451c30` (bay door, input 0x68) |

The command dispatcher is `FUN_00414690`. Each input case passes `(current_bit == 0)` to the actuator, which deploys the surface when TRUE (bit clear = currently retracted) and retracts it when FALSE (bit set = currently deployed). The actuator function updates the 3D model state and writes the advisory bit.

## Toolkit Roadmap

- New `lib/src/hud.cpp` + `lib/include/ft/hud.h` — parse sprite name table and gauge parameter block
- New `cli/cmd_hud.cpp` — `ft hud dump <file.HUD>` prints gauge table as JSON `[{gauge, dx, dy}]`
- GUI: overlay viewer that renders gauge positions on a 640×480 canvas

## TODO

- Confirm per-aircraft anchor point source (the `0x10 0x10` init values in `FUN_00406040` suggest a default; actual per-aircraft position may come from the PT file or a separate config)
- `FUN_00407930` confirmed as advisory icon renderer (bits 6–11 only): calls `FUN_004986b0(x, y, mode, sprite_ptr)` for GEAR/FLAP/BRAKE/HOOK icons; does NOT read bits 0–4 or 28–31
- Bits 0–4 and 28–31 confirmed written by `_DAMAGEDoHit@12` — display element that reads these bits (damage overlay or cockpit warning lights) not yet identified; `FUN_00407930` is not the reader
- Bit 14 (`0x04000`) SP writer still unknown. Exhaustive search via `DumpHUDBit14Search.java` confirmed: 215 write refs to `DAT_0050cfef` across 55 functions — none set bit 14 with a direct `0x4000` constant. `?MPReceive@@YGDXZ` (FUN_0046c98f) has a confirmed WRITE ref at 0x46db2b but its decompile fails ("Cannot properly adjust input varnodes"). Unanalyzed [no-function] writes at 0x4bc177/0x4bc190. To resolve: raw disasm of 0x46db2b and 0x4bc177 to find the actual store instruction. Structural note: `DAT_0050cfef` = player entity base (`0x50ce80`) + `0x16F` (confirmed from FUN_00454140: `DAT_0050cfef = *(undefined4 *)(pcVar3 + 0x16f)`).
- Bits 15–18 writer confirmed as `FUN_0049fb70`; exact glidepath ratio thresholds (using `FUN_004bed70` carrier finder, `FUN_004c66cc` distance, `FUN_0049f7b0` scale) partially decoded from decompile; full semantic confirmation (what `iVar5` represents in ratio units) requires live tracing

## Related

- [BRF.md](BRF.md) — `.PT` aircraft type records that likely reference the corresponding `.HUD`
- [FNT.md](FNT.md) — font files used to render HUD text elements
- [PIC.md](PIC.md) — bitmap assets used for HUD graphical elements
