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

## TODO

- Decode numeric fields more precisely (warhead capability flags, seeker mode enum, agility params).
- Cross-reference seeker type byte against SEE.md seeker definitions.
