# BRF -- Brent's Relocatable Format (.OT / .NT / .PT / .JT / .SEE / .ECM / .GAS)

> Format fully documented by the [OpenFA project](https://gitlab.com/openfa/openfa).
> Primary reference: `crates/asset/xt_parse/src/lib.rs`, `crates/asset/ot/src/lib.rs`,
> `crates/asset/pt/src/lib.rs`. Our implementation is in `lib/src/brf.cpp` and
> `lib/src/ot.cpp`.

---

## Overview

BRF is a **plain ASCII text** container for all game type definitions. Seven file
extensions share the same tokenizer; the `struct_type` field distinguishes them.

| Extension | struct_type | Contents |
|-----------|-------------|----------|
| `.OT` | 1 | Object type (generic game object) |
| `.NT` | 3 | NPC type (AI unit / crew) |
| `.PT` | 5 | Plane type (aircraft aerodynamics + avionics) |
| `.JT` | 7 | Jettison type (projectile / weapon physics) |
| `.SEE` | 10 | Seeker type (missile guidance) |
| `.ECM` | 9 | ECM pod definition |
| `.GAS` | 8 | Gas / fuel tank definition |

---

## File Structure

```
[brent's_relocatable_format]

:<ptr_name1>
:<ptr_name2>
\tend

<kind> <value>
<kind> <value>
...
\tend

[<section_name>]
<kind> <value>
...
\tend
```

### Tokens

| Kind | Value syntax | C++ type |
|------|-------------|---------|
| `byte` | decimal integer | `uint8_t` |
| `word` | decimal integer | `uint16_t` / `int16_t` |
| `dword` | decimal integer | `uint32_t` / `int32_t` |
| `ptr` | `"filename"` or `NULL` | `std::string` (may be empty) |
| `symbol` | `NAME` | `std::string` |
| `string` | `"text"` | `std::string` |

`\tend` terminates each block. The pointer table (`:name` lines) comes first, then the
main field block, then optional named subsections.

---

## OT Fields (Object Type)

OT versioning is determined by field count: V0=49, V1=51, V2=63, V3=64.

Key fields (abridged — full list in `crates/asset/ot/src/lib.rs`):

```
struct_type         byte    1=OT, 3=NT, 5=PT, 7=JT
type_size           word
instance_size       word
short_name          ptr     e.g. "F-16C"
long_name           ptr     e.g. "General Dynamics F-16C Fighting Falcon"
file_name           ptr     e.g. "F16C"
ot_flags            dword   see ot_flags table below
obj_class           word    see obj_class table below
shape               ptr     3D model filename (no extension)
shadow_shape        ptr     shadow/crash shape; convention: NAME_S.SH
max_vis_dist        word    feet (max 204 typical; over ~30000 makes object silent)
camera_dist         word
laser_targeting_sig word
ir_signature        word
rcs_signature       word
hit_points          word
dmg_planes          word    damage dealt to each target type
dmg_ships           word
dmg_structs         word
dmg_armor           word
dmg_other           word
explosion_type      byte
crater_size         byte    feet
empty_weight        dword   pounds
dmg_debris_pos      i16[3]  debris spawn offset on damage (x y z, feet)
dst_debris_pos      i16[3]  debris spawn offset on destruction
dmg_type            dword
year_available      dword   earliest campaign year this object appears
```

### `ot_flags` values

| Value | Meaning |
|-------|---------|
| `$6bf3` | Flyable aircraft (player-selectable) |
| `$2bf3` | Non-flyable (AI-only) |
| `$8xxxxxx` prefix | Hidden from in-game reference library |

### `obj_class` values

| Value | Meaning |
|-------|---------|
| `$8000` | Fighter |
| `$4000` | Bomber |
| `$2000` | Ship |
| `$1000` | Structure |
| `$0800` | Vehicle / armor |

---

## PT Fields (Plane Type)

PT extends OT with ~80 additional aerodynamic and avionics fields, beginning immediately
after the NPC_TYPE section in the BRF file.

### Carrier / datalink / thrust-vectoring dword

The first dword of the PT section is a flag word controlling several systems:

| Value | Meaning |
|-------|---------|
| `$53` | Carrier-capable, single-seat |
| `$57` | Carrier-capable, two-seat |
| `$55` | Land-based only (no carrier) |
| `$20` prefix | ATA (air-to-air) datalink |
| `$40` prefix | ATG (air-to-ground) datalink |
| `$60` prefix | Both ATA + ATG datalink |
| `$91` suffix | Horizontal-axis thrust vectoring |
| `$591` suffix | Horizontal + vertical thrust vectoring (3D) |

Example: `$4591` = ATG datalink + full 3D thrust vectoring.

### Core aerodynamic fields

```
carrier_flags       dword   see table above
env                 ptr     → G-envelope section
neg_g_count         word    number of negative-G envelope entries (negative number)
pos_g_count         word    number of positive-G envelope entries
max_speed_sl        word    mph at sea level
max_speed_36k       word    mph at 36,000 ft
accel_runway        word    acceleration on runway
decel_runway        word    deceleration on runway
roll_speed_min      word    deg/sec (negative)
roll_speed_max      word    deg/sec
pull_rate           word    pitch pull rate
neg_g_limit         word
num_engines         byte
military_thrust     dword   lbf
afterburner_thrust  dword   lbf
throttle_accel      word    percent/sec
throttle_decel      word    percent/sec
tv_min_angle        word    thrust-vectoring min angle (−60 = 60°)
tv_max_angle        word    thrust-vectoring max down-angle
tv_speed            word    deg/sec
fuel_consumption_mil word   at military power
fuel_consumption_ab  word   at afterburner
fuel_capacity       dword   pounds
aero_drag           word    256 = baseline
g_drag              word    drag increase per G
airbrake_drag       word
wheel_brake_drag    word
flap_drag           word
gear_drag           word
weapons_bay_drag    word
flaps_lift          word
drag_loaded         word    extra drag when fully loaded
g_drag_loaded       word
gear_pitch          word    nose-up angle on ground (e.g. 5 = taildragger)
max_landing_speed   word    ft/sec
max_side_speed      word    ft/sec
max_sink_rate       word    ft/sec
max_landing_pitch   word    degrees
max_landing_roll    word    ft/sec roll-out distance
structural_warn     word    speed limit warning (ft/sec)
structural_limit    word    hard speed limit (ft/sec)
mtow                dword   max take-off weight, pounds
misc_per_flight     word    maintenance man-hours per flight
repair_multiplier   word    repair cost multiplier
```

### Stall / spin fields

```
stall_warn_delay    word    clocks (1 clock = 1/256 sec)
stall_duration      word
stall_severity      word
stall_pitch_down    word    deg/sec pitch-down during stall
spin_entry_ease     word    0 = harder
spin_exit_ease      word    negative = harder
spin_yaw_low        word    deg/sec
spin_yaw_high       word
spin_aoa_low        word    degrees
spin_aoa_high       word
spin_bank_low       word    degrees
spin_bank_high      word
```

### G-envelope section

Each envelope entry covers one G-load level and lists up to 16 speed/altitude pairs
defining the aircraft's performance boundary at that G.

```
[env_entry]
gload               word    e.g. -4, -3, … 9
count               word    number of valid speed/altitude pairs
stall_lift          word    index of stall boundary in data[]
max_speed           word    index of max-speed boundary in data[]
data[0..15]:
  speed             word    ft/sec
  altitude          dword   feet
```

Unused slots are zeroed. A typical FA aircraft has 4 negative-G and 9 positive-G entries.

### Hardpoints

Each PT has exactly 10 hardpoints. Per-hardpoint fields:

```
hld                 word    Hardpoint Loading Data flags (see table below)
offset_x            word    right/left offset, feet (positive = right)
offset_y            word    up/down offset, feet
offset_z            word    fore/aft offset, feet
slew_heading        word    1 deg = 182 (e.g. 364 = 2°)
slew_pitch          word    1 deg = 182
slew_limit_heading  word    1 deg = 182
slew_limit_pitch    word    1 deg = 182
default_type        ptr     default weapon/store filename (e.g. "AIM9M.JT")
weight              byte    hundreds of pounds (max 256 = 25,600 lbs)
quantity            word    number of items on this hardpoint
location            byte    see location codes below
```

### Hardpoint Loading Data (HLD) flags

| Value | Meaning |
|-------|---------|
| `$8` | Required load only (gun, built-in sensor — always loaded) |
| `$85` | External HP, symmetrical load, IR-guided missile |
| `$465` | External HP, symmetrical load, active-radar missile, SARH missile, store |
| `$520` | Stealth, internal bay, active-radar missile, other missile, store |
| `$24` | Stealth, internal bay, symmetrical load, active-radar missile |
| `$84` | Stealth, internal bay, symmetrical load, IR-guided missile |
| `$1301` | External HP, other missile, fuel tank, disallow air-to-air |
| `$17e5` | External HP, symmetrical load, multi-role (bombs + missiles + stores) |
| `$5e5` | External HP, symmetrical load, bombs + missiles |

### Hardpoint location codes

| Code | Location |
|------|----------|
| `0` | Centerline |
| `1` | Fuselage |
| `2` | Internal gun |
| `3` | Internal bay |
| `4` | Wing |
| `5` | Wingtip |

### `systemDamage` array

48-byte array immediately after the MTOW field. Each byte is a threshold controlling how
much damage a subsystem can sustain before failing. Common values: `20`/`22` (lightly
protected), `148`/`150` (moderately armored), `36` (structural), `6` (critical systems).

Full field list: `crates/asset/pt/src/lib.rs`.

---

## Round-Trip Notes

- Parse → serialize produces byte-identical files for all OT/NT/PT files in FA_2.LIB.
- Null pointers are written as `ptr NULL`.
- Integer field sign interpretation follows OpenFA's type assignments; wrong signedness
  produces visually wrong values in `info` output.

---

## ft commands

```
# Same pattern for all seven extensions:
ft ot  info   <file.OT>              # human-readable field dump
ft ot  unpack <file.OT>  [-o out.txt] # editable text
ft ot  pack   <in.txt>   -o out.OT   # write back

ft nt  info / unpack / pack
ft pt  info / unpack / pack
ft jt  info / unpack / pack
ft see info / unpack / pack
ft ecm info / unpack / pack
ft gas info / unpack / pack
```

Example: `ft pt info F16C.PT` → thrust, max_speed, fuel, stall speed, ceiling.

---

## Applications

BRF files are plain ASCII — open and edit directly after `ft unpack`, no further conversion needed.

- **VS Code** — free; multi-file search useful when cross-referencing `.PT` hardpoint names against `.JT` definitions
- **Notepad++** — free, Windows; lightweight for quick field edits
- **Notepad / TextEdit** — free, built-in; sufficient for small edits
