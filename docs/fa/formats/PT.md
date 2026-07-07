---
format: PT
name: Aircraft Flight Model
extensions: [".PT"]
family: BRF
category: typedef
endianness: none
spec:
  status: partial
  gaps:
    - kind: re-static
      issue: 54
      note: "59 of 65 aero-parameter words (0xCA-0x14B) unnamed"
    - kind: re-static
      issue: 54
      note: "debris-pos candidates at 0x1F/0x2D unconfirmed"
codec:
  direction: round-trip
  byte_identical: true
  lib: [lib/src/brf.cpp, lib/src/ot.cpp]
  commands: [pt]
  tests: [tests/test_brf.cpp, tests/test_ot.cpp]
  fuzz: [fuzz/fuzz_brf.cpp]
  gui: [gui/src/editors/brf_editor.cpp]
  fixtures:
    synthetic: true
    real_manifest: true
related: [BRF, OT, JT, SH]
---

# PT — Aircraft Flight Model (.PT)

`.PT` files are the per-aircraft aerodynamics and avionics records. There are
145+ `.PT` files in FA_2.LIB, one per aircraft variant. They use
[BRF](BRF.md) (Brent's Relocatable Format) — plain ASCII text that is parsed
at game startup by `SetupPT`.

## Tools

### fx

```
fx pt info   F16C.PT          # field dump: thrust, speed, fuel, stall, ceiling
fx pt unpack F16C.PT -o F16C.pt.txt
fx pt pack   F16C.pt.txt -o F16C_mod.PT
```

See [BRF.md](BRF.md) for the full field reference (OT base fields + PT
extension fields, G-envelope section, hardpoints, `systemDamage` array).

## File Layout

Plain text; BRF syntax. This page documents the **in-memory PLANE_TYPE binary
struct** the text compiles into — the text-format field reference lives in
[BRF.md](BRF.md).

### In-Memory Struct: PLANE_TYPE

At game startup `_SetupPT` (`0x4A7220`) chains through `_SetupNT` →
`_SetupOT` → `FUN_004a6b10` to load each `.PT` file and lock its binary data
into memory. The binary layout is needed for runtime patching, network-sync
analysis, and AI parameter extraction.

Layout derived by direct byte-counting against `F16C.PT` (type_size = 660 =
0x294). Offsets marked **confirmed** were read or written directly by
decompiled the game executable code. All others are inferred from packing (BRF fields
written sequentially, no alignment padding).

`file_name` is **not stored** in the binary struct — it is the LIB lookup key
only. `ot_names` is a **single** ptr; it points to a name-record holding all
name strings.

Total layout: OBJ_TYPE 166 B (0x00–0xA5) + NPC_TYPE 20 B (0xA6–0xB9) +
PLANE_TYPE main 258 B (0xBA–0x1BB) + 9 hardpoints × 24 B (0x1BC–0x293) =
**660 B** ✓

#### OBJ_TYPE section (0x00–0xA5, 166 B)

| Offset | Size | Field              | BRF type | Notes |
|--------|------|--------------------|----------|-------|
| `0x00` | 1    | struct_type        | byte     | = 5 for PT |
| `0x01` | 2    | type_size          | word     | F16C = 660 |
| `0x03` | 2    | instance_size      | word     | **confirmed** — `_T_AddObj@12` reads `*(short*)(ptr+3)` |
| `0x05` | 4    | ot_names           | ptr      | single ptr to name record; F16C = ptr |
| `0x09` | 4    | ot_flags           | dword    | F16C = `$806bf3` (flyable, visible in library) |
| `0x0D` | 2    | obj_class          | word     | **confirmed** — `_SetupOT` tests bits `0xc000`/`0x2000`/`0x1000`/`0x800`/`0x400`/`0x200` |
| `0x0F` | 4    | shape              | ptr      | **confirmed** — `_SetupOT` passes `iVar4+0x0f` to `FUN_004a71e0`; string compared to `"eject_SH"` |
| `0x13` | 4    | shadow_shape       | ptr      | **confirmed** — `_SetupOT` reads `*(char**)(iVar4+0x13)` for shape-name derivation |
| `0x17` | 4    | *(damage_shape_a)* | ptr      | filled by `_SetupOT` (`"_a"` variant); dword 0 in source BRF |
| `0x1B` | 4    | *(damage_shape_b)* | ptr      | filled by `_SetupOT` (`"_b"` variant); dword 0 in source BRF |
| `0x1F` | 2    | **Unknown**        | word     | F16C = 0; possibly dst_debris_pos[0] |
| `0x21` | 2    | **Unknown**        | word     | F16C = 0; possibly dst_debris_pos[1] |
| `0x23` | 2    | **Unknown**        | word     | F16C = 30; possibly dst_debris_pos[2] |
| `0x25` | 4    | *(damage_shape_c)* | ptr      | filled by `_SetupOT` (`"_c"` variant); dword 0 in source BRF |
| `0x29` | 4    | *(damage_shape_d)* | ptr      | filled by `_SetupOT` (`"_d"` variant); dword 0 in source BRF |
| `0x2D` | 2    | **Unknown**        | word     | F16C = 30; possibly dmg_debris_pos[0] |
| `0x2F` | 2    | **Unknown**        | word     | F16C = 0 |
| `0x31` | 2    | **Unknown**        | word     | F16C = 0 |
| `0x33` | 4    | dmg_type / damage_set | dword | F16C = 0; written `_Rand(2)+1` at destruction, selects the `{_C,_D}` wreck pair when `2` — see [shape-selection.md](../shape-selection.md) |
| `0x37` | 4    | year_available     | dword    | F16C = 1984 (F-16C service entry year) |
| `0x3B` | 2    | max_vis_dist       | word     | F16C = 148 |
| `0x3D` | 2    | camera_dist        | word     | F16C = 0 |
| `0x3F` | 2    | laser_targeting_sig | word    | F16C = 88 |
| `0x41` | 2    | ir_signature       | word     | F16C = 88 |
| `0x43` | 2    | rcs_signature      | word     | F16C = 100 |
| `0x45` | 2    | hit_points         | word     | F16C = 100 |
| `0x47` | 2    | dmg_planes         | word     | F16C = 0 |
| `0x49` | 2    | dmg_ships          | word     | **confirmed** — `_T_AddObj@12` reads as `DAT_0050ce8e` (crash damage threshold); F16C = 92 |
| `0x4B` | 2    | dmg_structs        | word     | F16C = 255 |
| `0x4D` | 2    | dmg_armor          | word     | F16C = 255 |
| `0x4F` | 2    | dmg_other          | word     | F16C = 255 |
| `0x51` | 2    | **Unknown**        | word     | F16C = 255 |
| `0x53` | 2    | **Unknown**        | word     | F16C = 255 |
| `0x55` | 1    | explosion_type     | byte     | F16C = 30 |
| `0x56` | 1    | crater_size        | byte     | F16C = 0 |
| `0x57` | 4    | empty_weight       | dword    | F16C = 14,567 lbs (matches real F-16C) |
| `0x5B` | 2    | **Unknown**        | word     | F16C = 199 |
| `0x5D` | 16   | movement info      | 8×word   | speed/accel params; F16C = 0,16380,14560,−14560,16380,0,0,0 |
| `0x6D` | 16   | movement dwords    | 4×dword  | F16C = ^0,^0,^300,^60000 |
| `0x7D` | 4    | utilProc           | symbol   | F16C = `_PLANEProc` |
| `0x81` | 4    | loopSound          | ptr      | |
| `0x85` | 4    | secondSound        | ptr      | |
| `0x89` | 4    | engineOnSound      | ptr      | |
| `0x8D` | 4    | engineOffSound     | ptr      | |
| `0x91` | 1    | (byte)             | byte     | F16C = 1 |
| `0x92` | 16   | sound params       | 8×word   | F16C = 15000,320,160,20,1600,0,20,60 |
| `0xA2` | 4    | hudName            | ptr      | |

#### NPC_TYPE section (0xA6–0xB9, 20 B)

| Offset | Size | Field   | BRF type | Notes |
|--------|------|---------|----------|-------|
| `0xA6` | 4    | ctType  | dword    | F16C = 0 |
| `0xAA` | 4    | ctName  | ptr      | |
| `0xAE` | 1    | (byte)  | byte     | F16C = 12 |
| `0xAF` | 1    | (byte)  | byte     | F16C = 32 |
| `0xB0` | 1    | (byte)  | byte     | F16C = 20 |
| `0xB1` | 2    | (word)  | word     | F16C = 32767 |
| `0xB3` | 2    | (word)  | word     | F16C = 0 |
| `0xB5` | 1    | (byte)  | byte     | F16C = 9 (hardpoint count) |
| `0xB6` | 4    | hards   | ptr      | → hardpoints array |

#### PLANE_TYPE section (0xBA–0x1BB, 258 B)

| Offset | Size | Field               | BRF type | Notes |
|--------|------|---------------------|----------|-------|
| `0xBA` | 4    | carrier_flags       | dword    | F16C = `$11` (land-based); see BRF.md carrier_flags table |
| `0xBE` | 4    | env                 | ptr      | → G-envelope section in same file |
| `0xC2` | 2    | neg_g_count         | word     | F16C = −4 (number of negative-G envelope entries) |
| `0xC4` | 2    | pos_g_count         | word     | F16C = 9 |
| `0xC6` | 2    | max_speed_sl        | word     | F16C = 1342 mph (sea level) |
| `0xC8` | 2    | max_speed_36k       | word     | F16C = 1934 mph (36,000 ft) |
| `0xCA` | 130  | (aero params)       | 65×word  | aerodynamic control-surface and flight-model parameters; first named fields per BRF.md: accel_runway (0xCA), decel_runway (0xCC), roll_speed_min (0xCE), roll_speed_max (0xD0), pull_rate (0xD2), neg_g_limit (0xD4); remaining 59 names unknown — see Open Questions |
| `0x14C` | 1   | num_engines         | byte     | F16C = 1 |
| `0x14D` | 2   | **Unknown**         | word     | F16C = 0; unlisted in BRF.md |
| `0x14F` | 4   | military_thrust     | dword    | F16C = 17,687 lbf (≈ F100-PW-229 dry) |
| `0x153` | 4   | afterburner_thrust  | dword    | F16C = 32,000 lbf |
| `0x157` | 2   | throttle_accel      | word     | F16C = 40 %/sec |
| `0x159` | 2   | throttle_decel      | word     | F16C = 60 %/sec |
| `0x15B` | 2   | tv_min_angle        | word     | F16C = 0 (no thrust vectoring) |
| `0x15D` | 2   | tv_max_angle        | word     | F16C = 0 |
| `0x15F` | 2   | tv_speed            | word     | F16C = 0 |
| `0x161` | 2   | fuel_consumption_mil | word    | F16C = 2 |
| `0x163` | 2   | fuel_consumption_ab  | word    | F16C = 16 |
| `0x165` | 4   | fuel_capacity       | dword    | F16C = 6,972 lbs (matches real F-16C internal fuel) |
| `0x169` | 2   | aero_drag           | word     | F16C = 256 (baseline) |
| `0x16B` | 2   | g_drag              | word     | F16C = 33 |
| `0x16D` | 2   | airbrake_drag       | word     | F16C = 256 |
| `0x16F` | 2   | wheel_brake_drag    | word     | F16C = 102 |
| `0x171` | 2   | flap_drag           | word     | F16C = 76 |
| `0x173` | 2   | gear_drag           | word     | F16C = 23 |
| `0x175` | 2   | weapons_bay_drag    | word     | F16C = 0 |
| `0x177` | 2   | flaps_lift          | word     | F16C = 51 |
| `0x179` | 2   | drag_loaded         | word     | F16C = 30 |
| `0x17B` | 2   | g_drag_loaded       | word     | F16C = 13 |
| `0x17D` | 2   | gear_pitch          | word     | F16C = 40 |
| `0x17F` | 2   | max_landing_speed   | word     | F16C = 40 ft/sec |
| `0x181` | 2   | max_side_speed      | word     | F16C = 40 ft/sec |
| `0x183` | 2   | max_sink_rate       | word     | F16C = 2560 ft/sec |
| `0x185` | 2   | max_landing_pitch   | word     | F16C = 5120 |
| `0x187` | 45  | systemDamage[]      | byte×45  | subsystem hit thresholds; F16C values range 6–150 |
| `0x1B4` | 2   | misc_per_flight     | word     | F16C = 10 (maintenance man-hours) |
| `0x1B6` | 2   | repair_multiplier   | word     | F16C = 10 |
| `0x1B8` | 4   | mtow                | dword    | F16C = 33,000 lbs |

#### Hardpoints section (0x1BC–0x293, 9 × 24 B)

F16C.PT has **9** hardpoints. Each hardpoint is 24 bytes: 8×word + ptr (4B) +
byte + word + byte.

| Offset | Size | Field         | Notes |
|--------|------|---------------|-------|
| `+0x00` | 2   | hld           | loading-data flags |
| `+0x02` | 2   | offset_x      | ft right/left |
| `+0x04` | 2   | offset_y      | ft up/down |
| `+0x06` | 2   | offset_z      | ft fore/aft |
| `+0x08` | 2   | slew_heading  | 1° = 182 |
| `+0x0A` | 2   | slew_pitch    | |
| `+0x0C` | 2   | slew_limit_heading | |
| `+0x0E` | 2   | slew_limit_pitch   | |
| `+0x10` | 4   | default_type  | ptr to weapon filename |
| `+0x14` | 1   | weight        | hundreds of lbs |
| `+0x15` | 2   | quantity      | |
| `+0x17` | 1   | location      | see BRF.md location codes |

## Engine Notes

### Setup call chain

```
_SetupPT  (0x4A7220)  -- thin wrapper
  └─ _SetupNT  (0x4A7200)  -- NT-layer init
       ├─ FUN_004a6b10(param_1)  -- MM handle → raw data ptr
       └─ _SetupOT  (0x4A6EB0)  -- OT-layer init: resolves shape ptrs via RMAccess_8
            └─ FUN_004a71e0(ptr)  -- loads one SH file: *ptr = _RMAccess_8(*ptr, 0x8000)

FUN_004a71c0  (0x4A71C0)  -- single-object variant; calls FUN_004a71e0 on shape fields
```

`_SetupOT` reads six shape ptr fields in the binary struct and resolves each
via `_RMAccess_8` (load from LIB). For flying objects (`obj_class & 0xc000 ≠
0`) it also derives four damage-state shape names (_a / _b / _c / _d suffix)
from `shadow_shape`, writes them into the struct, and resolves them.

### Entry points

| VA         | Symbol         | Role |
|------------|----------------|------|
| `0x4A7200` | `_SetupNT`     | NT-layer wrapper; calls `FUN_004a6b10` + `_SetupOT` |
| `0x4A7220` | `_SetupPT`     | PT entry point; thin wrapper calling `_SetupNT` |
| `0x4A71C0` | `FUN_004a71c0` | Single-object shape loader; calls `FUN_004a71e0` on shape fields |
| `0x4A6EB0` | `_SetupOT`     | OT-layer shape resolver; patches +0x0f/+0x13/+0x17/+0x1b/+0x25/+0x29 |
| `0x4A71E0` | `FUN_004a71e0` | Resolves one shape ptr: `*ptr = _RMAccess_8(*ptr, 0x8000)` |
| `0x4A6B10` | `FUN_004a6b10` | MM handle → raw data ptr (`*(int*)(param+0xf)`, with MM lock if bit 1 of `+0xe`) |
| `0x4A6B30` | `FUN_004a6b30` | BRF record lookup + string→binary conversion via `_RMType_4` / `_RMFind_4` |

## Open Questions

### 1. The 65-word aerodynamic block (0xCA–0x14B)

BRF.md names the first six fields (accel_runway through neg_g_limit) but the
remaining 59 cover roll/pitch/yaw surface limits, G-load model parameters,
stall/spin tuning, and landing-gear dynamics. Direct `.PT` byte-counting
against `F16C.PT` is the effective approach — no Ghidra script needed. To name
the remaining 59 words, compare F16C.PT values side-by-side with a
known-different aircraft (e.g. F14A.PT or A10.PT) and correlate numeric deltas
with the BRF.md stall/spin field list.

*Status: open — re-static (#54)*

### 2. Debris-position candidates

The three 2-byte unknowns at 0x1F/0x21/0x23 and three at 0x2D/0x2F/0x31 are
plausible candidates for `dst_debris_pos[3]` and `dmg_debris_pos[3]` (i16[3]
each per BRF.md), but the 0x23/0x2D values of 30 don't obviously map to
debris-offset coordinates — needs a second PT file comparison to confirm.

*Status: open — re-static (#54)*

## Related

**Formats:** [BRF](BRF.md) — full BRF field reference for all PT fields,
hardpoints, G-envelope; [OT](OT.md) — OT base fields that PT inherits;
[JT](JT.md) — weapon/projectile types loaded via `SetupJT` (`0x4A7230`);
[SH](SH.md) — 3D model format referenced by `shape` / `shadow_shape` ptr
fields.
