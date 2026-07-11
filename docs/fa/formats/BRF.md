---
format: BRF
name: Brent's Relocatable Format
extensions: []
family: BRF
category: typedef
endianness: none
spec:
  status: complete
codec:
  direction: round-trip
  byte_identical: true
  lib: [lib/src/brf.cpp]
  commands: []
  tests: [tests/test_brf.cpp]
  fuzz: [fuzz/fuzz_brf.cpp]
  gui: [gui/src/editors/brf_editor.cpp]
  fixtures:
    synthetic: true
    real_manifest: false
related: [OT, NT, PT, JT, SEE, ECM, GAS]
---

# BRF — Brent's Relocatable Format

BRF is a **plain ASCII text** container for all game type definitions. Seven
file extensions share the same tokenizer; the `struct_type` field
distinguishes them. This page is the family overview — each member format has
its own spec (see Related).

The "Relocatable" in the name refers to pointer relocation. Each BRF file
opens with a pointer table — the `:name` lines before the first `\tend` — that
enumerates every `ptr` field in the record by symbolic name. This is a
relocation table: in the plain-text format, `ptr` fields hold quoted filename
strings or `NULL`; in the engine's in-memory representation they become actual
pointers. At load time the engine walks the pointer table and resolves each
named field to a memory address, making the loaded record independent of where
it was placed — relocatable. The mechanism is the same concept as relocation
entries in a linker object file, applied to game data.

| Extension | struct_type | Contents |
|-----------|-------------|----------|
| `.OT` | 1 | Object type (generic game object) |
| `.NT` | 3 | NPC type (AI unit / crew) |
| `.PT` | 5 | Plane type (aircraft aerodynamics + avionics) |
| `.JT` | 7 | Jettison type (projectile / weapon physics) |
| `.SEE` | 10 | Seeker type (missile guidance) |
| `.ECM` | 9 | ECM pod definition |
| `.GAS` | 8 | Gas / fuel tank definition |

## Tools

### fx

```
# Same pattern for all seven extensions:
fx ot  info   <file.OT>              # human-readable field dump
fx ot  unpack <file.OT>  [-o out.txt] # editable text
fx ot  pack   <in.txt>   -o out.OT   # write back (byte-identical)

fx nt  info / unpack / pack
fx pt  info / unpack / pack
fx jt  info / unpack / pack
fx see info / unpack / pack
fx ecm info / unpack / pack
fx gas info / unpack / pack
```

Example: `fx pt info F16C.PT` → thrust, max_speed, fuel, stall speed, ceiling.

### Other Tools

BRF files are plain ASCII — open and edit directly after `fx unpack`, no
further conversion needed.

- **VS Code** — free; multi-file search useful when cross-referencing `.PT` hardpoint names against `.JT` definitions
- **Notepad++** — free, Windows; lightweight for quick field edits
- **Notepad / TextEdit** — free, built-in; sufficient for small edits

## File Layout

Plain text; no binary fields. (Hex values use the `$XX` prefix; fixed-point /
relative values use the `^XXXXX` prefix.)

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

`\tend` terminates each block. The pointer table (`:name` lines) comes first,
then the main field block, then optional named subsections.

### OT Fields (Object Type)

OT versioning is determined by field count: V0=49, V1=51, V2=63, V3=64.

Key fields (abridged):

```
struct_type         byte    1=OT, 3=NT, 5=PT, 7=JT, 8=GAS, 9=ECM, 10=SEE
type_size           word
instance_size       word
ot_names            ptr     single ptr to the name record, which holds the
                            short name, long name, and filename strings
                            (e.g. "F-16C" / "General Dynamics F-16C Fighting
                            Falcon" / "F16C.OT")
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

(An earlier version of this list showed `short_name`/`long_name`/`file_name`
as three separate `ptr` fields; the binary struct confirmed by
[PT.md](PT.md) byte-counting holds a **single** `ot_names` ptr to the name
record, and the filename is the LIB lookup key, not a stored field.)

**`ot_flags` values:**

| Value | Meaning |
|-------|---------|
| `$6bf3` | Flyable aircraft (player-selectable) |
| `$2bf3` | Non-flyable (AI-only) |
| `$8xxxxxx` prefix | Hidden from in-game reference library |

**`obj_class` values:**

| Value | Meaning |
|-------|---------|
| `$8000` | Fighter |
| `$4000` | Bomber |
| `$2000` | Ship |
| `$1000` | Structure |
| `$0800` | Vehicle / armor |

### PT Fields (Plane Type)

PT extends OT with ~80 additional aerodynamic and avionics fields, beginning
immediately after the NT section in the BRF file. In the source text the block
is introduced by the comment divider `; ---- START OF PLANE_TYPE ----`
(verified against a live `.PT`; the only bracketed name in the file is the
top-of-file `[brent's_relocatable_format]` tag — sections are comment-delimited,
`OBJ_TYPE` → `NPC_TYPE` → `PLANE_TYPE`).

**Carrier / datalink / thrust-vectoring dword** — the first dword of the PT
section is a flag word controlling several systems:

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

**Core aerodynamic fields:**

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
; --- 59 more aero words follow here (0xD6–0x14B): control-authority limit
;     vectors, roll/pitch/yaw axis limits, and the stall/spin block below,
;     all code-traced — see PT.md § The 65-word aerodynamic block ---
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

**Stall / spin fields:** these sit inside the aero block at PT offsets
`0x128–0x13E` (words 47–58), immediately before the gear/landing gate — see
[PT.md § The 65-word aerodynamic block](PT.md#the-65-word-aerodynamic-block-0xca0x14b)
for the exact offset map and the engine readers that confirm each name.

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

**G-envelope section** — each envelope entry covers one G-load level and lists
up to 16 speed/altitude pairs defining the aircraft's performance boundary at
that G:

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

Unused slots are zeroed. A typical FA aircraft has 4 negative-G and 9
positive-G entries.

**Hardpoints** — each PT has up to 9 hardpoints (count varies by aircraft;
F16C has 9, MiG-29 has 8, some light aircraft have fewer). Per-hardpoint
fields:

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
weight              byte    hundreds of pounds (max 255 = 25,500 lbs)
quantity            word    number of items on this hardpoint
location            byte    see location codes below
```

**Hardpoint Loading Data (HLD) flags:**

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

**Hardpoint location codes:**

| Code | Location |
|------|----------|
| `0` | Centerline |
| `1` | Fuselage |
| `2` | Internal gun |
| `3` | Internal bay |
| `4` | Wing |
| `5` | Wingtip |

**`systemDamage` array** — 48-byte array immediately after the MTOW field.
Each byte is a threshold controlling how much damage a subsystem can sustain
before failing. Common values: `20`/`22` (lightly protected), `148`/`150`
(moderately armored), `36` (structural), `6` (critical systems).

## Engine Notes

BRF type initialisation entry points confirmed from FA.SMS (called during game
startup to load each type array into memory):

| VA | Symbol | BRF type loaded |
|----|--------|-----------------|
| `0x4A6EB0` | `SetupOT` | OBJ_TYPE (`.OT` static objects) |
| `0x4A7040` | `SetupNT` | NPC_TYPE (`.NT` vehicles) |
| `0x4A71C0` | `SetupPT` | PLANE_TYPE (`.PT` aircraft) |
| `0x4A7230` | `SetupJT` | PROJ_TYPE (`.JT` weapons) |

These four are the canonical entry points for tracing how BRF fields map to
in-memory struct layouts; [PT.md](PT.md) carries the fully byte-counted
PLANE_TYPE binary layout.

## Round-Trip Notes

- Parse → serialize produces byte-identical files for all OT/NT/PT files in
  FA_2.LIB; `tests/test_brf.cpp` asserts raw-byte preservation.
- Null pointers are written as `ptr NULL`.
- Integer field sign interpretation must match the type assignments in the
  spec; wrong signedness produces visually wrong values in `info` output.

## Related

**Formats:** the seven member specs — [OT](OT.md), [NT](NT.md), [PT](PT.md),
[JT](JT.md), [SEE](SEE.md), [ECM](ECM.md), [GAS](GAS.md).
