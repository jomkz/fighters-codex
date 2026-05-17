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
- The `~` prefix has two distinct uses: (1) **destruction state replacement** — `~BNK5.OT` is "Damaged Shelter 5" with a destroyed model (`dbk5.SH`), zeroed `dmg_armor`, and bit 5 of `ot_flags` cleared; placed by the engine at the same world position after the base object is destroyed. (2) **campaign/theater variant** — `~COLTWR.OT` is an enhanced version of COLTWR with an added loop sound and bit 22 set, used in a specific campaign. Check the `~` prefix section in ARCHITECTURE.md for the full list.

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

### `obj_class` word — Confirmed values

| Value | Class | Representative files |
|-------|-------|---------------------|
| `$40` | Scenery / terrain feature | TREE1-2, ROAD*, LOT, CITY1-3, CRATER, CROPA/B, FLAG*, RES1-2, HOUS*, HOOCH, MOOSEA/B |
| `$100` | Ground structure | BLDG1-3, BNK1-9, STRIP1-7, HANGR, SILO, TOWER, COMM, BUNKER, SHELT |

`$40` covers both pure scenery (TREE, ROAD) and targetable-but-civilian structures (CITY blocks, urban terrain). `$100` covers all military and airfield structures.

Naval and vehicle classes appear in NT files only (`$2000` = ship, `$1000` = SAM, etc.).

### `ot_flags` dword — Observed bit map

Full survey of all 170 OT files. Bit positions confirmed by cross-category comparison.

| Bit | Mask | Confirmed | Observed pattern |
|-----|------|-----------|-----------------|
| 0 | `$1` | **targetable** | Absent on TREE, ROAD, LOT, CRATER, DEST, REACTB (pure scenery / already-destroyed states) |
| 5 | `$20` | **primary/military variant** | Absent on plain civilian variants (BLDG2 `$501` vs BLDG1 `$521`) and ~ destroyed-state replacements |
| 8 | `$100` | **has geometry / collideable** | Present on bridges, most structures, rocks; absent on flat terrain tiles (ROAD, LOT, TREE) |
| 10 | `$400` | **civilian or dual-use infrastructure** | Set on BLDG*, HOUS*, urban blocks, airport buildings (APTLA/B/C), NUCOB, REACTR; also on SAM infrastructure sites (SA3SITE, HAWKSITE) — not exclusively civilian |
| 11 | `$800` | **animated / non-static scenery** | Set on FLAG*, CROP*, CITY1-3, BR*END (bridge endpoints), SUPPLY, PRDR1/2 (rotating radar dish), MOOSEA/B, MICRO/MICROM (comm dish), CRANE, COMM, BRD1 |
| 15 | `$8000` | **airfield surface** | Exclusive to STRIP1-7 and DTSTRP; never set on other OT types |
| 17 | `$20000` | **military / hardened structure** | Set on BNK*, HANGR*, COMM, NUCCT, PRDR*, RELAY, TOWER, COLTWR, CTWR*, and other military targets |
| 20 | `$100000` | **damaged runway surface** | DTSTRP only; contrasts with bit 21 on intact STRIP* |
| 21 | `$200000` | **intact runway surface** | STRIP1-7; absent on DTSTRP (damaged variant) |
| 22 | `$400000` | **prominent/large variant flag** | Set on large or strategically significant structure variants: SHELT, BUNKER, HANGRB ("Hangar 2"), CTOWR ("Control Tower, Large"), FUEL, OILW, DOCK1, COMM, REACTR, CTYBKA/B, APTLA, OILW, ~COLTWR; absent from their smaller counterparts (HANGR, CTWR1, CTYBKC–G, APTB1) |

Bits 9, 12–14, 16, 18–19, 23+ not observed in any OT file in FA_2.LIB.

**Example flag combinations:**

| Object type | `ot_flags` | Bits active |
|-------------|-----------|-------------|
| TREE1, ROAD | `$0` | none |
| SILO | `$21` | 0, 5 |
| ROCKA | `$101` | 0, 8 |
| BLDG2 (Warehouse) | `$501` | 0, 8, 10 |
| BLDG1 (Factory) | `$521` | 0, 5, 8, 10 |
| SA3SITE, HAWKSITE | `$401` | 0, 10 |
| SUPPLY | `$820` | 5, 11 |
| FLAG* | `$800` | 11 |
| CITY1 | `$900` | 8, 11 |
| PRDR1/2 (Radar) | `$20d21` | 0, 5, 8, 10, 11, 17 |
| IND1, RES1, HGRP1 | `$121` | 0, 5, 8 |
| BNK1 | `$20121` | 0, 5, 8, 17 |
| HANGR | `$20121` | 0, 5, 8, 17 |
| HANGRB (large) | `$420121` | 0, 5, 8, 17, 22 |
| BUNKER, SHELT | `$400021` | 0, 5, 22 |
| COMM (C&C) | `$400921` | 0, 5, 8, 11, 22 |
| STRIP1 | `$208021` | 0, 5, 15, 21 |
| DTSTRP | `$108021` | 0, 5, 15, 20 |

### Hitpoint scale

All confirmed in the `word hitpoints` field (field index 23). Values appear to be absolute damage points. Compare `dmg_armor` across JT weapon types against object `hit_points` to calibrate survivability.

## TODO

- Confirm exact semantics of bits 5, 8, 10, 11, 22 via Ghidra targeting / collision functions — current labels are inferred from observed patterns, not verified in code
