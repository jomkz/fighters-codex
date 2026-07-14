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
    real_install: true
related: [OT, NT, PT, JT, SEE, ECM, GAS]
---

# BRF — Brent's Relocatable Format

BRF is a **plain ASCII text** container for all game type definitions. Seven
file extensions share the same tokenizer; the `struct_type` field
distinguishes them. This page is the family overview — each member format has
its own spec (see Related).

| Extension | struct_type | Contents |
|-----------|-------------|----------|
| `.OT` | 1 | Object type (generic game object) |
| `.NT` | 3 | NPC type (AI unit / crew) |
| `.PT` | 5 | Plane type (aircraft aerodynamics + avionics) |
| `.JT` | 7 | Jettison type (projectile / weapon physics) |
| `.SEE` | 10 | Seeker type (missile guidance) |
| `.ECM` | 9 | ECM pod definition |
| `.GAS` | 8 | Gas / fuel tank definition |

### A BRF file is a text DLL

The engine loads one through **`LoadDLL` (0x41EB60) — the same entry point it uses for a
real Win32 DLL**. `IsBrentDLL` (0x41E8F0) sniffs the magic first line; if it matches,
`LoadBrentDLL` (0x41F240) takes over, and if it does not, the very same function goes on to
parse PE headers and section tables. That is what "Relocatable" means here, and it is the
whole design: a BRF file is a **hand-written object file**, with imports and relocations,
assembled at load time instead of by a linker.

`LoadBrentDLL` is a one-pass assembler with a cursor that only ever advances:

| Keyword | What it emits | Bytes |
|---------|---------------|-------|
| `byte` / `word` / `dword` | the operand(s), little-endian | 1 / 2 / 4 each |
| `string "text"` | the characters **plus a NUL** | `len + 1` |
| `symbol NAME` | `SMAddress(NAME)` — the address of one of the **engine's own** symbols | 4 |
| `ptr NAME` | a placeholder, plus a **relocation** against label `NAME` | 4 |
| `:NAME` | nothing — it **declares a label** at the cursor | 0 |
| `end` | terminates the **file** | — |

Anything else is fatal: `ErrorExit("Unknown command '%s' in LoadBrentDLL")`.

At `end` (or EOF) the loader allocates a handle of exactly `cursor - base` bytes, copies the
image in, and walks the relocation list back-patching every `ptr` with the address of its
label. Both the label and the `ptr` target are passed through `strlwr`, so **they resolve
case-insensitively**.

Three consequences the codec got wrong until #491, none of which a round-trip test can see
(serialization replays the file's own lines):

- **A `:label` block is not "a table of strings".** It is a labelled offset into the same
  image, and it may hold **numeric** fields. The aircraft records rely on that: `:hards` is
  the inline hardpoint array — 24 bytes per station, each holding a `ptr` to that station's
  default store — and `:env` is the flight envelope. A parser that assumes every block is a
  string table silently drops both, for all 145 shipped aircraft.
- **A `string` is `len + 1` bytes, not 4.** Everything after one lands at the wrong offset.
- **`end` ends the file, not a block.** It is the first keyword the loader tests, and it
  jumps straight to the allocate-and-finish path. All 534 shipped records carry exactly one,
  as their last token.

`symbol` is an **import**: `SMAddress` resolves the name against the executable's own symbol
table, so the shipped data names engine functions directly. Every `symbol` in the retail
records is a **class proc** — and each one is a selector that falls through to its parent,
which is how the class hierarchy is expressed:

| Symbol | Records | Recovered as |
|--------|---------|--------------|
| `_OBJProc` | 161 | the base class proc |
| `_PLANEProc` | 145 | aircraft |
| `_PROJProc` | 135 | projectiles |
| `_GVProc` | 73 | ground vehicles — delegates to `_OBJProc` |
| `_STRIPProc` | 13 | airstrips |
| `_CARRIERProc` | 5 | carriers |
| `_CATGUYProc` | 1 | the catapult crewman |
| `_EJECTProc` | 1 | the ejected pilot |

`tests/test_brf.cpp` resolves all 534 of them against `db/symbols/` — the data naming a
function the symbol database has not claimed is a hole in the reconstruction, and that is how
`_GVProc` came to be claimed.

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

Plain text; no binary fields. Hex values use the `$XX` prefix; negatives are written
`^XXXXX` (meaning `-XXXXX`).

**The record comes first, the labelled blocks after it** — the shape all 534 shipped records
have, and the only shape that loads. (An earlier version of this page said the pointer table
came first and that `\tend` terminated each block. It does not: `end` ends the file, so a
file written that way would load as nothing but its strings.)

```
[brent's_relocatable_format]

;---------------- START OF OBJ_TYPE ----------------
    byte 1                  ; the record's own fields, emitted at the cursor
    word 166                ; type_size -- this record's total size, in bytes
    ...
    symbol _OBJProc         ; an import: the class proc
;---------------- END OF OBJ_TYPE ----------------

:hards                      ; a label -- and a NUMERIC block: the inline hardpoint array
;-------- hardpoint 0
    word $8
    ...
    ptr defaultTypeName0    ; the station's default store

:ot_names                   ; a label -- a string block, past the end of the record
    string "A-10"
    string "A-10 Thunderbolt"
    string "A10.PT"
:shape
    string "a10.SH"
    end
```

Sections are **comment-delimited** (`;---- START OF PLANE_TYPE ----`); the only bracketed
name in the file is the magic line. The record is self-describing: it declares its own
sections, and every field declares its own width — so a field's offset is a fact read from
the file, never a schema imposed on it.

### Tokens

| Kind | Value syntax | Emits |
|------|-------------|-------|
| `byte` | `5`, `$ff`, `^3` | 1 byte |
| `word` | as above | 2 bytes, little-endian |
| `dword` | as above | 4 bytes, little-endian |
| `ptr` | a **label name** (`shape`, `hards`) | 4 bytes, relocated to that label |
| `symbol` | an engine symbol (`_OBJProc`) | 4 bytes, `SMAddress(name)` |
| `string` | `"text"` | `len + 1` bytes (NUL-terminated) |

`byte`/`word`/`dword` keep consuming operands **for as long as the next token is a number**,
so `word 1 2 3` emits three words. No shipped file uses the repeat form, but the loader
accepts it and `fx` decodes it.

`ptr` never holds a filename or `NULL` — it holds the name of a `:label`, and the filename
it appears to carry is a `string` inside the block it points at.

### The record, and the image

The `word` at image offset 1 is **`type_size`: the record's own total size**. It is a prefix
of the assembled image — the string blocks live past its end:

| | Root fields | + inline `hards` | = `type_size` |
|---|---|---|---|
| `.OT` (170 records) | 166 | — | **166** |
| `.JT` (135) | 166 + 149 (`PROJ_TYPE`) | — | **315** |
| `.NT` (84) | 166 + 20 (`NPC_TYPE`) | 0–6 stations | **186 + 24n** |
| `.PT` (145) | 166 + 20 + `PLANE_TYPE` | 1–20 stations | varies |

Every record begins with `OBJ_TYPE`; each class appends its own section, and the sections
name the hierarchy outright — `.PT` carries `OBJ_TYPE` → `NPC_TYPE` → `PLANE_TYPE`, so a
plane *is* an NPC *is* an object. `tests/test_brf.cpp` asserts the identity above against
every shipped record: the fields the decoder produced must tile the image exactly, and must
sum to the size the record declares for itself.

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

`tests/test_brf.cpp` runs a census over **every one of the 534 shipped `.OT`/`.NT`/`.PT`/`.JT`
records** (under `FX_FA_ROOT`), asserting what the *decode produced*, not just that a repack
matches:

- parse → serialize is byte-identical (this page claimed that for years; no test performed it);
- every field, root and block, **tiles the assembled image exactly** — no gaps, no overlap;
- the fields sum to the **`type_size` the record declares for itself**, with the inline
  `hards` array contiguous with the root fields, and a whole number of 24-byte stations;
- every `ptr` **resolves** to a declared label (case-insensitively, as the loader does);
- every `symbol` names a function claimed in `db/symbols/`.

The round-trip alone proved none of this. It passed throughout the years the codec was
dropping every field inside a `:label` block, because serialization replays the file's own
lines — see [#491](https://github.com/jomkz/fighters-codex/issues/491).

### Notes

- `ptr` holds a **label name**, never a filename and never `NULL`. (The `ptr NULL` this page
  used to describe appears in no shipped record.)
- Integer field sign interpretation must match the type assignments in the
  spec; wrong signedness produces visually wrong values in `info` output.

## Related

**Formats:** the seven member specs — [OT](OT.md), [NT](NT.md), [PT](PT.md),
[JT](JT.md), [SEE](SEE.md), [ECM](ECM.md), [GAS](GAS.md).
