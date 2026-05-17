# OT — Static Object Definition (.OT)

FA_2.LIB contains 170 .OT files. Each defines one static (non-moving) scenery or target object — buildings, bridges, trees, flags, runways, radar dishes, etc.

**Format:** Brent's Relocatable Format (plain text). NOT a Win32 PE DLL. File sizes after decompression vary (e.g. BLDG1.OT=1256 bytes, SA3SITE.OT=similar).

**Location:** FA_2.LIB | **Count:** 170

**Related:** SH.md (3D shapes), NT.md (moving targets), T2.md (terrain maps that place OT objects), MISSION.md (mission files that instantiate OT objects as targets)

---

## Structure

Single-section structure (OBJ_TYPE only — no NPC_TYPE or PROJ_TYPE).

### OBJ_TYPE

```
[brent's_relocatable_format]

;---------------- START OF OBJ_TYPE ----------------
;---------------- general info ----------------

    byte 1                  ; object category (1 = static object)
    word 166                ; hitpoints (durability)
    word 0                  ; object subtype ID
    ptr ot_names
    dword $521              ; capability flags
    word $100               ; flags2
    ptr shape               ; 3D model reference
    dword 0 ... (reserved fields, same layout as JT/NT)
    dword 1956              ; year placed / era introduced
    word 195                ; (param)
    word 0
    word 100 ... word 100   ; (performance/damage-resistance params)
    byte 35                 ; (armor/hardness)
    byte 15
    dword 0
    word 0

;---------------- movement info ----------------
    word 0 ... (all zeros — static objects do not move)
    symbol _OBJProc         ; static object callback

;---------------- sound info ----------------
    dword 0 ... (all zeros — most static objects have no ambient sound)
```

### Labels Section

```
:ot_names
    string "Factory 3"
    string "Factory 3"
    string "BLDG1.OT"
:shape
    string "bld1.SH"
    end
```

---

## Notes

- Static objects have no `loopSound` ptr, no `secondSound`, and no hardpoints.
- The movement info block is all zeros.
- `_OBJProc` handles collision detection, destruction events, and visibility only.
- The `~` prefix indicates campaign- or theater-specific variants.

---

## File Inventory

| Category | Examples |
|----------|---------|
| Structures | BLDG1-3, FACTD, FCTYA/B, FACT1, IND1/2, STORE, HANGR/HANGRB, SHELT, CASTLE |
| Nuclear facilities | NUCCT, NUCOB, NUCRD, REACTB, REACTR |
| Runways / Aprons | STRIP1-7 (+ variants), APTB1, APTLA/B/C, APTM, APTOLD, DTSTRP |
| Bridges | BR1/2/3 END+MID, BRD1-4, BRDEND, BRDMID |
| Urban | CITY1-3, TWNBKA-F, CTYBKA-G, SLUM01/02, HOUS/HOUSB, HOOCH, RES1/2 |
| Bunkers / Military | BNK1-9 (+ variants), BUNKER, CMHQ1/2, SHELT, SILO |
| Radar / Comms | PRDR1/2, SRDR1/2, COMM/COMM1/2, RELAY, TOWER, CTOWR, CTWR1/2, COLTWR, KING |
| Terrain features | ROAD/ROAD2/4/ROADC, ROCKA-E (+ variants), TREE1/2, CROPA/B, WNDMLL, MOOSEA/B |
| SAM infrastructure | SA3SITE, HAWKSITE |
| Naval / Port | DOCK1, CONT1-3, CRANE, DKHOS1/2, CGRP1-3, HGRP1-3, SGRP1-3 |
| Misc | CRATER, OILW, SUPPLY, FUEL, WTRBUF, MICRO/MICROM, LOT, REDOM, FLAGO1/2, FLAGR1/2, FLAGY1/2, AV8TNT |
| Campaign variants (~) | ~BNK5, ~BNK6, ~BNK8, ~CLEMT, ~COLTWR, ~KITT, ~MOOSE, ~NIMZ, ~WASP |

---

## Calibration

### `obj_class` word — Partially confirmed

| Value | Class | Observed in |
|-------|-------|-------------|
| `$40` | Scenery / terrain feature (non-targetable) | TREE1.OT |
| `$100` | Ground structure (building, runway) | BLDG1.OT, STRIP1.OT |

Naval and vehicle classes are in NT files (`$2000` = naval vessel for IOWA.NT, KIROV.NT).

### `ot_flags` dword — Partially confirmed

Observed values and evidence:

| Object | `ot_flags` | Notes |
|--------|-----------|-------|
| TREE1.OT | `$0` | No flags — indestructible scenery |
| BLDG1.OT | `$521` | Factory building — destructible target |
| STRIP1.OT | `$208021` | Runway — destructible, mission-critical |

**Bit 0** (`0x1`): set on BLDG1 and STRIP1, absent on TREE1 → **targetable / destroyable**.

Full bit decomposition requires Ghidra cross-reference of the damage/targeting evaluation function. Cross-category method:

1. Collect flags from ROCKA.OT, FLAGO1.OT (likely `$0` or minimal), BLDG1.OT, BNK1.OT, SA3SITE.OT.
2. Bit present on BNK1 but absent on BLDG1 → hardened/fortified flag.
3. Bit present on all targetable objects but absent on indestructible scenery → "valid mission target" flag.

### Hitpoint scale

Compare `word hitpoints` across object types to calibrate damage values. Soft targets (TREE1.OT, word 166) and buildings share the same hitpoint field position.

## TODO

- Complete `ot_flags` bit decomposition via cross-category comparison
- Map hitpoint scale (cannon burst vs. bomb hit survivability)
- Decode `obj_class $40` sub-classes (do all terrain features use $40?)
