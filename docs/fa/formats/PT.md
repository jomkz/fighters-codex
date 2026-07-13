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
    real_install: false
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
| `0xCA` | 130  | aero block          | 65×word  | control-authority, stall/spin and landing-gate parameters — **every word now code-traced** via the `_cgt` type-record mirror (`0x50D268 + off`); see [§ The 65-word aerodynamic block](#the-65-word-aerodynamic-block-0xca0x14b) for the full per-word map |
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

### The 65-word aerodynamic block (0xCA–0x14B)

The 65 words at `0xCA–0x14B` are the flight-model tuning table. They are named
here by **code trace**, not by value-guessing: at startup `GetCurObj` copies the
active aircraft's PLANE_TYPE record into the `_cgt` type-record mirror at base
`0x50D268` byte-for-byte, so PT offset `X` is read by the engine as
`DAT_[0x50D268 + X]` (anchored by exact hits — `fuel_consumption_mil` at PT
`0x161` = `DAT_0050d3c9`, `military_thrust` at `0x14F` = `DAT_0050d3b7`,
`g_drag` at `0x16B` = `DAT_0050d3d3`). Enumerating every reference into
`0x50D332–0x50D3B3` (aero block) gives the consumer of each word. **Every word
has a consumer — none are engine-unused.** Words `0xCA–0xF8` are consumed as two
block-copied limit vectors (see below), which is why the middle words of those
blocks carry no direct xref of their own.

The 12 stall/spin words (`0x128–0x13E`) confirm and now **position** the fields
BRF.md already named but could not place; the remaining words are newly named.
F16C values shown; "per-a/c" = varies by aircraft, "const" = identical across
the eight sampled `.PT` files (F16C/F14/A10/B52/AH64/AV8/C130/F117).

**Control-authority limit vectors (`0xCA–0xF8`, words 0–23).** `_COBv@0`
(`0x477EA0`) block-copies words 0–11 into the live control buffer `0x547338`;
`_COBrv@0` (`0x477ED0`) block-copies words 12–23 into `0x547350` and scales them
down by battle-damage (`DAT_0050ce8e`) and control-surface loss (`DAT_00522547`)
— i.e. these are the max control-response limits, degraded as the airframe takes
hits.

| word | off | field | F16C | reader / evidence |
|------|-----|-------|------|-------------------|
| 0 | `0xCA` | accel_runway (BRF) | −73 | `_COBv` block[0] |
| 1 | `0xCC` | decel_runway (BRF) | 0 | `_COBv` block |
| 2 | `0xCE` | roll_speed_min (BRF) | 73 | `_COBv` block |
| 3 | `0xD0` | roll_speed_max (BRF) | 73 | `_COBv` block |
| 4 | `0xD2` | pull_rate (BRF) | −146 | `_COBv` block |
| 5 | `0xD4` | neg_g_limit (BRF) | 146 | `_COBv` block |
| 6–11 | `0xD6`–`0xE0` | ground/low-speed handling limits (cont.) | 7,7,−146,146,73,73 · const | `_COBv` block words 6–11; ±-paired rate limits, roles inferential |
| 12–15 | `0xE2`–`0xE8` | rotational-authority axis A {neg,pos,rate,accel} | −270,270,362,724 · per-a/c | `_COBrv` block; fighters ±270, bombers ±30 |
| 16–19 | `0xEA`–`0xF0` | rotational-authority axis B {neg,pos,rate,accel} | 0,0,9,9 · per-a/c | `_COBrv` block |
| 20–23 | `0xF2`–`0xF8` | rotational-authority axis C {neg,pos,rate,accel} | −45,45,90,90 · per-a/c | `_COBrv` block |

**Stick-to-motion gains and turn coordination (`0xFA–0x116`, words 24–46).**
The three primary control axes are proven by their `_StickInput_28` calls in
`_FMFlight@0` (`0x47B020`): each is driven by a specific pilot input, so the
axis identity is definitive.

| word | off | field | F16C | reader / evidence |
|------|-----|-------|------|-------------------|
| 24 | `0xFA` | pitch-input → pitch-rate gain (`×/9`) | 20 · const | `_FMFlight` `DAT_0050d362 * pitchIn / 9` |
| 25 | `0xFC` | forward-motion scale (`×256`) | 70 · const | `_MovePlane@0` (`0x476AE0`) |
| 26 | `0xFE` | vertical/reverse-motion scale (`×−256`) | 15 · const | `_MovePlane@0` |
| 27 | `0x100` | AI visual-detection / contrail interval factor | 115 · per-a/c | `FUN_0047759b` (time-of-day-scaled spotting timer) |
| 28–31 | `0x102`–`0x108` | rudder turn-coordination axis {neg,pos,rate,accel} | −4,4,4,9 | `_FMFlight` `_StickInput(&DAT_0050d007, …, _rudder)`, speed/G-scaled |
| 32 | `0x10A` | adverse-yaw (roll→yaw) coupling | 10 · const | `_FMFlight` `DAT_0050d02b = DAT_0050d372 * rollRate` |
| 33 | `0x10C` | sideslip / yaw drag coefficient | 128 · const | `FUN_0047a970` (× sideslip); also `PROJGuideLoft` |
| 34 | `0x10E` | yaw/heading response rate | 5 · per-a/c | `_FMFlight` `DAT_0050d00f += d376·resp`; `PROJGuideLoft` |
| 35–38 | `0x110`–`0x116` | **roll** axis {neg,pos,rate,accel} | −90,90,362,724 | `_FMFlight` `_StickInput(&DAT_0050d033, …, _stickX)` |
| 39–42 | `0x118`–`0x11E` | **pitch** axis {neg,pos,rate,accel} | −90,90,362,724 | `_FMFlight` `_StickInput(&DAT_0050d037, …, _stickY)` |
| 43–46 | `0x120`–`0x126` | **yaw** axis {neg,pos,rate,accel} | −90,90,362,724 | `_FMFlight` `_StickInput(&DAT_0050d03b, …, _rudder)` |

**Stall / spin model (`0x128–0x13E`, words 47–58).** These are the BRF.md
stall/spin fields, now pinned to their binary offsets. Names are BRF.md's;
offsets and readers are the new evidence.

| word | off | field (BRF name) | F16C | reader / evidence |
|------|-----|------------------|------|-------------------|
| 47 | `0x128` | stall_warn_delay | 512 · per-a/c | `?CheckForEvents2` fires the stall-warning event; `_FMFlight` |
| 48 | `0x12A` | stall_duration | 512 · per-a/c | `?CheckForEvents2`; `_FMFlight` `d08d < DAT_0050d392` |
| 49 | `0x12C` | stall_severity | 256 | `_FMFlight` (`DAT_0050d394`) |
| 50 | `0x12E` | stall_pitch_down (deg/sec) | 30 · const | `_FMFlight` `_MatchF24(&pitch, −0x5A00, …)` drives nose to −90°; `?PROJEventProc` |
| 51 | `0x130` | spin_entry_ease (0 = harder) | 0 · per-class | `FUN_0047ccb0` sets spin state `d08c=3`; fighters 0, bombers 1, helo 2 |
| 52 | `0x132` | spin_exit_ease (neg = harder) | −2 · per-class | `FUN_0047cdb0` recovery selector (−2 / −1); `_FMFlight` |
| 53–54 | `0x134`–`0x136` | spin_yaw_low / _high (deg/sec) | 120,180 · const | `_FMFlight` `FUN_0047ce70(lo, hi, spinPhase)` |
| 55–56 | `0x138`–`0x13A` | spin_aoa_low / _high (deg) | 30,70 · const | `_FMFlight` `FUN_0047ce70` |
| 57–58 | `0x13C`–`0x13E` | spin_bank_low / _high (deg) | 15,5 · const | `_FMFlight` `FUN_0047ce70` |

**Gear pitch and landing gate (`0x140–0x14A`, words 59–64).** Word 59 is the
gear-down nose-up trim; words 60–64 are the universal landing-quality gate
(identical in every `.PT`) that `_CheckLandingParms@0` (`0x477140`) scores —
each threshold overshoot returns 5 (warning) or 6 (bad landing).

| word | off | field | F16C | reader / evidence |
|------|-----|-------|------|-------------------|
| 59 | `0x140` | gear-down pitch-trim authority (`× 0xB6`) | 0 · per-a/c | `_FMUpdateGearPitch@0` (`0x4514C0`); AV8 = 3 |
| 60 | `0x142` | landing gate: reference speed (fail if exceeded by `>0x32`) | 330 · const | `_CheckLandingParms`; `?HUDDrawSpeed` reads it as the HUD approach ref |
| 61 | `0x144` | landing gate: max sink rate (fail if `>10` over) | 51 · const | `_CheckLandingParms` |
| 62 | `0x146` | landing gate: pitch/AoA limit | 95 · const | `_CheckLandingParms`; `?HUDDrawAlt` |
| 63 | `0x148` | landing gate: attitude limit 1 (fail if `>0x14` over) | 25 · const | `_CheckLandingParms` |
| 64 | `0x14A` | landing gate: attitude limit 2 (fail if `>0x14` over) | 10 · const | `_CheckLandingParms` |

Confidence: the three primary axes (roll/pitch/yaw, words 35–46) and the
stall/spin/landing blocks (words 47–64) are reader-proven. The two block-copied
limit vectors (words 0–23) have a proven consumer (`_COBv`/`_COBrv`) and proven
damage-scaling role, but the per-word breakdown inside words 6–23 is inferred
from the ±-paired numeric structure and is annotated as such. Method is
reproducible with `scripts/ghidra/run_ghidra.sh` over the `0x50D332–0x50D3B3`
range.

## Open Questions

### 1. Debris-position candidates

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
