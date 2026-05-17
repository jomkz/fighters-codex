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

### `obj_class` word — Confirmed values (full survey)

| Value | Class | Representative files |
|-------|-------|---------------------|
| `$40` | Personnel / infantry | CATGUY, EJECT, PLTDWN, RUNNER, SOLDIER, TROOPS |
| `$200` | Ground vehicle (non-combat) | HUMVEE, LTRACK, MISTRK, MULE_A/B/C, SFLUSH, SRDR1/2, TANKER, TRUCK |
| `$400` | Armor / tank | BMP2, BTR80, M1, M113, M1975, M2, T72, T80, T90 |
| `$800` | AAA | A_M1939, KS12, KS19, M163, M1939, ZIF31, ZSU23, ZSU57 |
| `$1000` | SAM launcher | 2S6, ASA5, CHAP, FIM92, HAWK, MIS, ROLAND, SA2A, SA3, SA6, SA7, SA9, SA13–16 |
| `$2000` | Naval vessel | BARGE, BUTLER, CARGO, CIM, CLEM, CYCL, FISHBT, IOWA, JIANC, JIANE, JUNK, KIEV, KIROV, KITT, KNOX, KRIVAK, LCAC, NIMZ, OILR, OLEKMA, OSCAR, PMORN, RBOAT, SACRAM, SARAN, SESHDW, SL100, SOVR, TICON, TICON, WASP |

`$40` (personnel) and ground structure `$100` are shared with OT files. Naval units (`$2000`) include CYCL (helicopter) — the game treats helicopters as naval-class objects. Aircraft fighter/bomber values (`$8000`, `$4000`) appear only in PT files.

The `_GVProc` proc symbol is shared across ground, AAA, SAM, and naval units; the proc likely dispatches internally on `obj_class`.

### Hardpoint flags — Confirmed patterns

All hardpoints have bit 3 (`$8`) set — this appears to be the "active weapon slot" marker.

| Flag | Observed in | Interpretation |
|------|-------------|---------------|
| `$8` | All ground-unit hardpoints (M1, ZSU23, SA2A, etc.); IOWA HP 0–1; KIROV HP 2–3 | Standard weapon slot (guns, SAMs, all AA weapons) |
| `$a` | IOWA HP 2–3; KIROV HP 0–1 | Long-range surface-strike missile battery (Tomahawk / SS-N-19) |

Bit 1 (`$2`), combined with bit 3, marks dedicated ship-to-ship/land-attack cruise missile launchers. Anti-air weapons (ZSU23, SA6, SA2A) use `$8` only — AA vs AG targeting is determined by the loaded JT weapon type, not the hardpoint flag.

SA2A has 6 hardpoints (launch tubes), all `$8`. IOWA and KIROV each have 4 hardpoints with a gun/missile split.

### NPC_TYPE AI params — Confirmed (full survey)

| Field | Confirmed range | Interpretation |
|-------|-----------------|---------------|
| `aggressiveness` | 20 (most ground) / 40 (ships, capable SAMs) | Engagement aggression level |
| `skill` | 20–176 | Fire control accuracy; higher = better targeting |
| `reaction` | 20–80 | Threat acquisition speed; higher = slower (longer lock-on delay) |

Observed values by unit class:

| Unit type | aggressiveness | skill | reaction | Examples |
|-----------|---------------|-------|----------|---------|
| Ground vehicle (unarmed) | 20 | 60 | 40 | TRUCK, HUMVEE, MULE |
| Armor | 20 | 60 | 40 | M1, T72, BMP2, BTR80 |
| Basic MANPADS / short-range SAM | 20 | 60 | 40 | SA7, SA9, SA13-16, FIM92 |
| Basic AAA | 20 | 80 | 40 | ZSU23 |
| Advanced AAA (heavy caliber) | 40 | 80–100 | 40–60 | KS12, KS19, ZSU57, M163 |
| Advanced SAM | 40 | 144 | 60 | SA2A, SA3, SA6 |
| Naval vessel | 40 | 100 | 80 | IOWA, KIROV, most ships |

`TRUCK.NT` (no weapons at all) and `M1.NT` share identical AI params (20 / 60 / 40) — confirming these fields drive general NPC movement and threat-response behavior rather than weapon accuracy alone.

### `ot_flags` dword — Observed NT patterns

| Flag | Units | Observed pattern |
|------|-------|-----------------|
| `$821` | Most ground units (tanks, AAA, SAM, vehicles) | Bits 0, 5, 11 |
| `$131` | Most naval vessels | Bits 0, 4, 5, 8 |
| `$521` | Small boats (FISHBT, JUNK, RBOAT) | Bits 0, 5, 8, 10 |
| `$c21` | Infantry (SOLDIER, RUNNER) | Bits 0, 5, 6, 7 |
| `$801` | Passive units (MULE_A/B/C, EJECT) | Bits 0, 11 |
| `$c8331` | Aircraft carrier deck (CLEM, KITT, NIMZ) | Bits 0, 4, 5, 8, 11, 19, 22 |
| `$1c8131` | Large carrier (KIEV) | Bits 0, 4, 5, 8, 11, 19, 20, 22 |
| `$2000821` | Towed AA guns (KS12, KS19, M1939) | Bits 0, 5, 11, 25 |
| `$4000821` | SA2A only | Bits 0, 5, 11, 26 |

Bit 11 (`$800`) appears on all ground units and absent on naval units; bit 8 (`$100`) is the inverse. Bits 19, 20, 22 on carrier ships likely encode flight-deck capability. Bit 25 and 26 meanings require Ghidra.

### Proc symbols

| Symbol | Observed in | Role |
|--------|-------------|------|
| `_GVProc` | M1, ZSU23, TRUCK, IOWA, KIROV | NPC AI dispatcher (shared across all categories) |
| `_PROJProc` | (via JT) | Projectile physics |

## TODO

- Confirm ot_flags bit semantics for bits 4, 5, 8, 11, 19, 20, 22, 25, 26 via Ghidra — current labels inferred from category patterns
- Confirm hardpoint bit 1 ($2) meaning (naval surface-strike missile vs. other interpretations) via Ghidra weapon evaluation function
