# JT — Weapon / Ordnance Definition (.JT)

FA_2.LIB contains 135 .JT files. Each defines one weapon/ordnance type (missiles, bombs, guns, rockets). Loaded at runtime by the FA object system.

**Format:** Brent's Relocatable Format (plain text). NOT a Win32 PE DLL. File sizes after decompression vary (e.g. AIM9M.JT=2775 bytes, GAU8.JT=similar).

**Location:** FA_2.LIB | **Count:** 135

**Related:** SH.md (3D shapes), SEE.md (seeker definitions referenced in PROJ_TYPE), NT.md (NPC vehicles that carry weapons), OT.md (static objects), BRF.md (aircraft that carry weapons)

---

## Structure

Two-section structure: OBJ_TYPE (physical object base) followed by PROJ_TYPE (projectile/seeker).

### Section 1: OBJ_TYPE

```
[brent's_relocatable_format]

;---------------- START OF OBJ_TYPE ----------------
;---------------- general info ----------------

    byte 7                  ; object category (7 = projectile)
    word 315                ; hitpoints
    word 52                 ; object subtype ID
    ptr ot_names
    dword $6                ; capability flags
    word $80                ; flags2
    ptr shape               ; 3D model reference
    dword 0                 ; (12 reserved dword/word fields)
    ...
    dword 1983              ; year introduced
    word 59                 ; (additional physics params)
    word 0
    word 100 ... word 100   ; (performance percentages)
    byte 30                 ; (more params)
    byte 3
    dword 190
    word 0

;---------------- movement info ----------------

    word 0 ... (speed/accel/maneuver params)
    symbol _PROJProc        ; utilProc (projectile physics callback)

;---------------- sound info ----------------

    ptr loopSound
    dword 0 ... (sound attenuation params)
    (word values for sound range/falloff)
```

### Section 2: PROJ_TYPE

```
;---------------- START OF PROJ_TYPE ----------------

    dword $1204f            ; warhead/capability flags
    word 1                  ; warhead count
    byte 10                 ; seeker type
    ptr si_names
    word 0                  ; seeker flags
    byte $0                 ; sub-type
    byte 2                  ; seeker mode (0=unguided, 1=radar, 2=IR, etc.)
    byte $0
    byte 0 ... (8 bytes target-selection params)
    ; Seeker lobe 1 (primary):
    word 8190               ; azimuth half-angle
    word 8190               ; elevation half-angle
    dword ^0                ; min range
    dword ^50000            ; max range
    dword $80000000         ; min heading error
    dword $7fffffff         ; max heading error
    ; Seeker lobe 2 (secondary):
    word 8190
    word 8190
    dword ^4000
    dword ^24000
    dword $80000000
    dword $7fffffff
    ; Warhead/agility params (many byte values):
    byte 50 ... byte 85
    ; Fire params:
    word 546 word 1 word 16
    byte 100 ... byte 34
    ptr fireSound
    word 6000 word 0 word 750 word 35
```

### Labels Section

```
:ot_names
    string "AIM-9M"                ; short display name
    string "AIM-9M Sidewinder"     ; long display name
    string "AIM9M.JT"              ; filename (self-reference)
:shape
    string "sidem.SH"              ; 3D shape file
:loopSound
    string "&missile.11k"          ; looping motor sound
:si_names
    string "AIM-9M"                ; (duplicate of ot_names)
    string "AIM-9M Sidewinder"
    string "AIM9M.JT"
:fireSound
    string "&ltmiss.11k"           ; launch sound
    end
```

---

## Notes

- Gun rounds (e.g. GAU8.JT) use seeker mode byte=0 (unguided), wide cone angles (word 16380), and short ranges.
- Missiles use narrow cones and long ranges.
- The `~` prefix (e.g. `~BLDR.JT`, `~MOOSE.JT`, `~MOTHB.JT`, `~VOMIT.JT`) indicates theater-specific or campaign-specific weapon variants.

---

## File Inventory

| Category | Examples |
|----------|---------|
| Missiles (IR/radar) | AIM-9M/X, AA-10/11/12, AM-39, R-530/550, MICA, HQ-61 |
| Bombs | MK-82/84, GBU-10/28/29/30, FAB-250/500, PAVEWAY, CBU-87/89 |
| Guns | GAU8, GAU12, GSH-23/30, M61, DEFA, ADEN, BK27 |
| Rockets | LAU-10/61, B8, B13 |
| SAM rounds | SA-2/3/6/7/9/13-16/19 |
| Naval weapons | SSN-9, SEA_SPAR, ASROC |
| Campaign variants | ~BLDR, ~MOOSE, ~MOTHB, ~VOMIT |

---

## Calibration

### Warhead capability flags (`dword`)

Confirmed flags from cross-referencing weapon types:

| Weapon | `dword` value | Role |
|--------|--------------|------|
| AIM-9M/X, AIM-120 | `$1204f` | Air-to-air missile |
| AGM-65G, AGM-84A | `$2a06f` | Air-to-ground missile |
| MK-82 | `$22012` | Unguided bomb (AG) |
| 20mm cannon round | `$0348c4` | Gun (AA + AG) |

The flags dword is loaded as-is into the runtime missile entity at `missile+0xa6`. At launch, the engine OR-assigns runtime state bits into the same field:
- `$1204f` (AIM-9M, AIM-120): bit 16 set → starts in search mode, transitions to 0x20000 on acquisition
- `$2a06f` (AGM-65G): bit 17 set → starts in track mode (fire-and-forget, locked before launch)

Confirmed bit roles from `_PROJLock@24` (0x004c2f20), `_PROJHitChance@28` (0x004c3380), and `@HARDFindJammer@4`:

| Bit | Hex | Meaning | Evidence |
|-----|-----|---------|---------|
| 16 | 0x010000 | Air-to-air capable / search-mode init | AA missiles and 20mm; `@HARDFindJammer@4` |
| 17 | 0x020000 | Air-to-ground capable / track-mode init | AG missiles, bombs, and 20mm |
| 21 | 0x200000 | Lock-count reduction modifier | `_PROJHitChance@28` |
| 9 | 0x000200 | Dual-lobe seeker lock processing | gates search/track lobe dispatch in `_PROJLock@24` |
| 10 | 0x000400 | Active-radar / special guidance mode | gates `FUN_004629e0` + `FUN_00452e60` in `_PROJLock@24` |
| 4 | 0x000010 | Pk modifier gate | `_PROJLock@24` range-based Pk calc |

Bits 0–8 (lower nibbles) are not yet confirmed individually; the pattern differs: guided missiles `0x4f`, bombs `0x12`, guns `0xc4`, suggesting fuze type and damage model encoding.

### Seeker mode byte — Confirmed

Matches the SEE seeker type byte:

| `byte` | Guidance | Confirmed weapon |
|--------|---------|-----------------|
| 0 | Unguided | MK-82.JT, 20MM_4.JT |
| 1 | Laser-guided | (see *L.SEE laser files) |
| 2 | IR / EO homing | AIM9M.JT, AIM9X.JT, AGM65G.JT |
| 3 | Radar (semi-active / active) | AIM120.JT, AGM84A.JT |

### Range unit in PROJ_TYPE

The `^` range values in PROJ_TYPE seeker lobes use the same 1-foot unit as SEE files. Confirmed data points:
- AIM9X lobe 1 max `^50000` = 8.2 nm; AIM-9X ~8 nm ✓
- AIM120 lobe 1 max `^144000` = 23.7 nm; AIM-120A ~25 nm ✓
- AGM84A lobe 1 max `^360000` = 59.3 nm; Harpoon ~60 nm ✓

### Agility / hit-probability bytes

The PROJ_TYPE section base is at `missile+0xa6` in the runtime entity. Confirmed reads from `_PROJHitChance@28` (0x004c3380) and `FUN_004c3960` (proximity fuze check):

| PROJ_TYPE offset | Runtime offset | Role (confirmed) |
|-----------------|----------------|-----------------|
| +0x4F | missile+0xF5 | Proximity fuze range (byte) — fuze triggers when `target_arm_size ≥ this`; below 50% = miss |
| +0x79 | missile+0x11F | Approach angle tolerance byte |
| +0x7A | missile+0x120 | Approach angle weight byte |
| +0x7B | missile+0x121 | Angular error weight byte (left-shifted 3 for angle table lookup) |
| +0x7C | missile+0x122 | Counter-maneuver jam Pk reduction byte — lowers Pk by this % |
| +0x7D | missile+0x123 | Altitude/range modifier weight byte |
| +0x7E | missile+0x124 | Range gap modifier byte |
| +0x7F | missile+0x125 | Pk adjustment byte (applied as signed % of base Pk) |
| +0x80 | missile+0x126 | Maneuver agility Pk bonus (ushort) |

The 55-byte gap between +0x4F (fuze range) and +0x79 (angle params) spans PROJ_TYPE+0x50–0x78. These bytes include turn rate, g-limit, and other physics params read by the `_PROJProc` callback rather than `_PROJHitChance`.

## TODO

- Confirm bits 0–8 of warhead flags dword — fuze type, damage model (pattern: AIM-9M `0x4f`, MK-82 `0x12`, 20mm `0xc4`)
- Map PROJ_TYPE+0x50–0x78 (turn rate, g-limit, maneuver physics) via `_PROJProc` callback disassembly
