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
| `$8` | All ground-unit hardpoints; majority of naval hardpoints | Standard weapon slot — present on all weapon types |
| `$a` | Subset of naval hardpoints on ships with 4+ HPs | Bit 1 (`$2`) set in addition to bit 3; exact meaning unconfirmed |

Bit 1 (`$2`) is **not** a surface-strike missile marker — confirmed via BRF survey. IOWA `$a` HPs carry SEA_SPAR and PHALANX (same weapons as its `$8` HPs), KIROV `$a` HPs carry AAA30 guns. Smaller ships (SARAN, KRIVAK with 3 HPs) have no `$a` hardpoints at all. SSN9 (Soviet anti-ship missile on SARAN) uses `$8` with a zero-cone field, not `$a`. Semantic meaning of bit 1 requires Ghidra fire-control dispatcher trace.

SA2A has 6 hardpoints (launch tubes), all `$8`. Ships with 4 hardpoints mix `$8` and `$a` in varying ratios (e.g. IOWA 2+2, TICON 1+3).

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

Full survey of all 84 NT files.

| Flag | Units | Bits active |
|------|-------|------------|
| `$821` | Most ground units (tanks, AAA, SAM, vehicles) | 0, 5, 11 |
| `$131` | Most naval vessels | 0, 4, 5, 8 |
| `$331` | OILR (oil rig) | 0, 4, 5, 8, 9 |
| `$521` | Small boats (FISHBT, JUNK, RBOAT) | 0, 5, 8, 10 |
| `$c21` | Infantry (SOLDIER, RUNNER) | 0, 5, 10, 11 |
| `$d31` | GCI (ground-controlled intercept radar) | 0, 4, 5, 8, 10, 11 |
| `$801` | Passive units (MULE_A/B/C, EJECT, PLTDWN) | 0, 11 |
| `$400` | CATGUY (carrier deck officer — visual only) | 10 |
| `$c8331` | Conventional carriers (CLEM, KITT, NIMZ) | 0, 4, 5, 8, 9, 15, 18, 19 |
| `$108331` | VSTOL carrier (WASP) | 0, 4, 5, 8, 9, 15, 20 |
| `$1c8131` | Hybrid carrier (KIEV) | 0, 4, 5, 8, 15, 18, 19, 20 |
| `$2000821` | Emplaced AA artillery (KS12, KS19, M1939) | 0, 5, 11, 25 |
| `$4000821` | SA2A only | 0, 5, 11, 26 |

**Bit semantics (inferred from category pattern; Ghidra confirmation pending):**

| Bit | Mask | Observed pattern |
|-----|------|-----------------|
| 0 | `$1` | Targetable — absent on CATGUY (purely visual) and A_M1939 (zone marker) |
| 4 | `$10` | Naval vessel — present on all ships; absent on all ground vehicles |
| 5 | `$20` | Armed/active unit — absent on passive MULE/EJECT; present on all combat-capable units |
| 8 | `$100` | Large physical hull — present on large ships; absent on small boats and ground vehicles |
| 9 | `$200` | Large platform — large ships (carriers, oil rig, GCI); absent on standard destroyers/cruisers |
| 10 | `$400` | Civilian/light type — infantry (SOLDIER, RUNNER), small civilian craft (FISHBT, JUNK, RBOAT), CATGUY |
| 11 | `$800` | Ground-mobile unit — present on all ground vehicles; absent on all naval vessels except carriers (which have additional bits 15–20) |
| 15 | `$8000` | Flight deck present — all carrier types (CLEM, KITT, NIMZ, KIEV, WASP) |
| 18 | `$40000` | Carrier landing gear (arrestor wire?) — CLEM, KITT, NIMZ, KIEV; absent on VSTOL-only WASP |
| 19 | `$80000` | Carrier launch gear (catapult?) — CLEM, KITT, NIMZ, KIEV; absent on VSTOL-only WASP |
| 20 | `$100000` | VSTOL/helicopter deck — KIEV, WASP; absent on conventional carriers without VSTOL |
| 25 | `$2000000` | Emplaced AA artillery — KS12 (85mm), KS19 (100mm), M1939 (37mm); all towed Soviet guns |
| 26 | `$4000000` | SA-2A only — possibly marks large fixed-site strategic SAM |

### Proc symbols

| Symbol | Observed in | Role |
|--------|-------------|------|
| `_GVProc` | M1, ZSU23, TRUCK, IOWA, KIROV | NPC AI dispatcher (shared across all categories) |
| `_PROJProc` | (via JT) | Projectile physics |

## TODO

- Confirm ot_flags bit semantics (bits 4, 5, 8, 9, 10, 11, 15, 18, 19, 20, 25, 26) via Ghidra — current labels inferred from category patterns only
- Confirm hardpoint bit 1 (`$2`) meaning via Ghidra fire-control dispatcher — "surface-strike missile" hypothesis ruled out by BRF survey; pattern unclear
