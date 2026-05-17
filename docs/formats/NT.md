# NT — NPC / Vehicle Definition (.NT)

FA_2.LIB contains 84 .NT files. Each defines one non-player-controlled vehicle or unit type (tanks, ships, soldiers, SAM launchers, etc.).

**Format:** Brent's Relocatable Format (plain text). NOT a Win32 PE DLL. File sizes after decompression vary (e.g. M1.NT=1805 bytes, ZSU23.NT=similar).

**Location:** FA_2.LIB | **Count:** 84

**Related:** JT.md (weapons on hardpoints), SH.md (3D shapes), OT.md (static counterparts), AI.md / BI.md (AI system that drives _GVProc behavior)

---

## Structure

Two-section structure: OBJ_TYPE (physical object base — same layout as JT/OT) followed by NPC_TYPE.

### Section 1: OBJ_TYPE

```
    byte 3                  ; object category (3 = NPC/vehicle)
    word 210                ; hitpoints
    word 162                ; object subtype ID
    ptr ot_names
    dword $821              ; capability flags
    word $400               ; flags2
    ptr shape
    ... (reserved fields)
    dword 1980              ; year introduced
    word 78                 ; (params)
    word 100 ... (performance params)
    byte 21
    byte 6
    dword 0
    word 100

;---------------- movement info ----------------
    word 2730               ; max speed
    word 0 ... (accel/turn params)
    word 50                 ; turn rate
    word 50
    dword ^50 dword ^50
    dword ^0 dword ^0
    symbol _GVProc          ; ground vehicle AI callback

;---------------- sound info ----------------
    ptr loopSound
    ptr secondSound         ; (optional — e.g. turret sound separate from engine)
    ... (sound attenuation params)
```

### Section 2: NPC_TYPE

```
;---------------- START OF NPC_TYPE ----------------
    dword $0                ; AI flags
    dword 0                 ; (reserved)
    byte 20                 ; aggressiveness
    byte 60                 ; (AI skill param)
    byte 40                 ; (AI param)
    word 32767              ; threat range
    word 0                  ; (flags)
    byte 1                  ; hardpoint count
    ptr hards

;---------------- END OF NPC_TYPE ----------------

:hards
;-------- hardpoint 0
    word $8                 ; hardpoint flags
    word 0                  ; position x
    word 30                 ; position y
    word 0 word 0 word 0    ; position z, angles
    word 0
    word 8190               ; firing cone
    ptr defaultTypeName0    ; default weapon type
    byte 0                  ; (flags)
    word 32767              ; ammo count (32767 = unlimited)
    byte 0
```

### Labels Section

```
:ot_names
    string "M-1"
    string "M-1 Abrams"
    string "M1.NT"
:shape
    string "m1.SH"
:loopSound
    string "&TANK.11K"
:secondSound
    string "&TURRET.11K"
:defaultTypeName0
    string "M1.JT"         ; weapon loaded on hardpoint 0
    end
```

---

## Notes

### AI Callbacks

| Symbol | Used by |
|--------|---------|
| `_GVProc` | Ground vehicles (tanks, APCs, trucks, soldiers) |
| `_SHIPProc` (likely) | Naval units |

### Hardpoints

Each hardpoint references a .JT file as its default weapon type. `ammo count = 32767` indicates unlimited ammunition. Multiple hardpoints are indexed sequentially (`defaultTypeName0`, `defaultTypeName1`, ...).

---

## File Inventory

| Category | Examples |
|----------|---------|
| Tanks | M1, T72, T80, T90, TYPE69 |
| APCs | M2, M113, BTR80, BMP2 |
| AAA | ZSU23, ZSU57, M163 |
| SAM launchers | SA2A, SA3, SA6, SA7, SA9, SA13-16, SA19, HAWK, ROLAND, MIM23, FIM92, HQ61 |
| Ships | IOWA, KIROV, KIEV, OSCAR, NIMZ, SFLUSH, OLEKMA, TICON, KNOX, KRIVAK, SOVR, JIANC, JIANE, PMORN, SESHDW |
| Vehicles | HUMVEE, TRUCK, LTRACK |
| Air units | CYCL (helicopter), EJECT (ejection seat) |
| Naval small | BARGE, CARGO, CARGO2, RBOAT, WASP, LCAC |
| Personnel | SOLDIER, TROOPS, CATGUY |
| Misc | SCUD, GCI, OILR, SACRAM, RUNNER |

---

## Calibration

### Hardpoint flags (`word $8`)

`$8` = bit 3 set. Method:

1. Collect hardpoint flags across NT files: tank main gun, AAA cannon, SAM launcher, naval gun. Objects with multiple hardpoint types (e.g. a ship with guns + SAMs) will have different flags per hardpoint.
2. Hardpoints that can fire at air targets vs. ground targets likely have distinct bits.
3. `word $8` on the M-1 tank gun is a baseline — compare against `ZSU23.NT` (AAA, anti-air) which should have an air-engagement bit.

### NPC_TYPE AI params

| Field | Known range | Hypothesis |
|-------|-------------|-----------|
| `aggressiveness` | 20 (M-1) | 0–100 scale; higher = attacks without provocation |
| `byte 60` | 60 (M-1) | Skill / accuracy; higher = better aim |
| `byte 40` | 40 (M-1) | Reaction time or acquisition speed |
| `word threat_range` | 32767 | Detection range in internal units (32767 = max / unlimited) |

Calibrate skill vs. aggressiveness by comparing a passive vehicle (`TRUCK.NT`) against an aggressive one (`ZSU23.NT`).

### Proc symbols

| Symbol | Observed in | Role |
|--------|-------------|------|
| `_GVProc` | M1, ZSU23, TRUCK, IOWA, KIROV | Ground vehicle / naval unit AI (shared) |
| `_PROJProc` | (via JT) | Projectile physics |

`IOWA.NT` and `KIROV.NT` both use `_GVProc`, not a separate ship proc. Either the ground vehicle proc handles naval movement, or the proc is a general NPC dispatcher that routes by `obj_class`.

### `obj_class` word — Confirmed values

| Value | Class | Observed in |
|-------|-------|-------------|
| `$40` | Scenery / terrain feature | TREE1.OT |
| `$100` | Ground structure | BLDG1.OT, STRIP1.OT |
| `$2000` | Naval vessel | IOWA.NT, KIROV.NT |

The `_GVProc` symbol on naval units despite `obj_class $2000` suggests the proc may dispatch based on this field internally.

## TODO

- Decode hardpoint flags (see methodology above)
- Confirm AI param semantics by comparing passive vs. aggressive NT files (`TRUCK.NT` vs `ZSU23.NT`)
- Identify additional `obj_class` values (ground vehicles, aircraft, SAM launchers)
- Decode `ot_flags` bit map via cross-category comparison
