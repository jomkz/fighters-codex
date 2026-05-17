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

### Capability flags (`dword $521`, `$401`, etc.)

`$521` = `0000 0101 0010 0001` binary. Method:

1. Collect flags from objects across categories: a runway (STRIP1.OT), a building (BLDG1.OT), a radar dish (PRDR1.OT), a flag (FLAGO1.OT).
2. Objects that can be destroyed should share a "destroyable" bit absent in indestructible scenery (trees, flags).
3. Objects that show on radar/mission map likely share a "radar-visible" or "targetable" bit.
4. XOR flags across pairs to isolate individual bit meanings.

Likely bit assignments based on OT object roles:

| Bit | Hypothesis |
|-----|-----------|
| 0 | Can be targeted / is a valid mission target |
| 5 | Explosive / generates explosion on destruction |
| 8 | Strategic value (counts toward mission objective) |
| 10 | Shows on map / radar |

### Hitpoint and hardness scale

Compare `word hitpoints` and hardness `byte` across object types. Soft targets (TREE1.OT) should have low hitpoints and low hardness. Bunkers (BNK1.OT) should have high hitpoints and high hardness. SAM sites are expected to be more durable than generic buildings.

### Destroyable vs. indestructible

A `capability_flags` bit likely marks indestructibility. Candidate: if `ROCKA.OT` (rock/terrain feature) and `TREE1.OT` have a flags bit not present in `BLDG1.OT`, that bit = indestructible scenery marker.

## TODO

- Decode capability flags via cross-category comparison (see methodology above)
- Map hitpoint scale (what hitpoint value survives one cannon burst vs. one bomb hit?)
- Identify indestructible scenery flag bit
