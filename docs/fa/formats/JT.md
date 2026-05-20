# JT — Weapon / Ordnance Definition (.JT)

FA_2.LIB contains 135 .JT files. Each defines one weapon/ordnance type (missiles, bombs, guns, rockets). Loaded at runtime by the FA object system.

**Format:** Brent's Relocatable Format (plain text). NOT a Win32 PE DLL. File sizes after decompression vary (e.g. AIM9M.JT=2775 bytes, GAU8.JT=similar).

**Location:** FA_2.LIB | **Count:** 135

**Related:** SH.md (3D shapes), SEE.md (seeker definitions referenced in PROJ_TYPE), NT.md (NPC vehicles that carry weapons), OT.md (static objects), BRF.md (aircraft that carry weapons)

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

## Notes

- Gun rounds (e.g. GAU8.JT) use seeker mode byte=0 (unguided), wide cone angles (word 16380), and short ranges.
- Missiles use narrow cones and long ranges.
- The `~` prefix (e.g. `~BLDR.JT`, `~MOOSE.JT`, `~MOTHB.JT`, `~VOMIT.JT`) indicates theater-specific or campaign-specific weapon variants.

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

Confirmed bit roles from `_PROJLock@24` (0x004c2f20), `_PROJHitChance@28` (0x004c3380), `@HARDFindJammer@4`, `_DAMAGEDoHit@12` (0x0040f970), `_PROJMoveProc` (0x4c11b0), and `_PROJAdd_40`:

| Bit | Hex | Meaning | Evidence |
|-----|-----|---------|---------|
| 0 | 0x000001 | Guided missile / rocket marker | `_DAMAGEDoHit@12`: bit 7=0 AND bit 0=1 → play missile-hit sound; AIM-9M `0x4f` has bit 0=1 ✓ |
| 1 | 0x000002 | Precision/CEP control: if set, 75% chance no scatter at launch; if clear, scatter always applied | `_PROJAdd_40`: `(flags & 2) == 0 \|\| !_Percent_4(75%)` → randomize bomb impact point; missiles have bit set (low CEP), unguided bombs clear (always scatter) |
| 4 | 0x000010 | Pk modifier gate | `_PROJLock@24` range-based Pk calc |
| 5 | 0x000020 | Guidance algorithm selector: 0 → `FUN_004c1630` (standard pursuit), 1 → `FUN_004c1660` (alternate) | `_PROJMoveProc`: `if ((flags & 0x20) == 0) FUN_004c1630() else FUN_004c1660()` |
| 6 | 0x000040 | Engine/propulsion tracking: when set, `_PROJEngineState_0()` runs each tick and adjusts thrust speed per phase (boost/cruise/coast) | `_PROJMoveProc`: `if ((flags & 0x40) != 0) { engine_state → speed update }` |
| 7 | 0x000080 | Cannon / gun-round marker | `_DAMAGEDoHit@12`: bit 7=1 → play gun-hit sound; 20mm `0xc4` has bit 7=1 ✓ |
| 9 | 0x000200 | Dual-lobe seeker lock processing | gates search/track lobe dispatch in `_PROJLock@24` |
| 10 | 0x000400 | Active-radar / special guidance mode | gates `PushCurObj` + `HARDBestSeeker` in `_PROJLock@24` |
| 12 | 0x001000 | Seeker re-acquisition far-range bias: when set, initial CF70 (re-acq distance) set at 75–100% of launch distance (90% roll) | `_PROJAdd_40`: `(flags & 0x1000) != 0 && _Percent_4(90%)` → CF70 = rand[75%, 100%] × launch_dist |
| 13 | 0x002000 | Seeker re-acquisition near-range bias: when set (bit 12 check failed), initial CF70 set at 0–25% of launch distance | `_PROJAdd_40`: `(flags & 0x2000) != 0 && _Percent_4(90%)` → CF70 = rand[0%, 25%] × launch_dist |
| 16 | 0x010000 | Air-to-air capable / search-mode init | AA missiles and 20mm; `@HARDFindJammer@4` |
| 17 | 0x020000 | Air-to-ground capable / track-mode init | AG missiles, bombs, and 20mm |
| 21 | 0x200000 | Lock-count reduction modifier | `_PROJHitChance@28` |

**Hit-sound dispatch** (`_DAMAGEDoHit@12` projectile-type case):

```c
if ((warhead_flags & 0x80) != 0)      → gun-hit sound    (20mm: 0xc4 has bit 7)
else if ((warhead_flags & 0x01) != 0) → missile-hit sound (AIM-9M: 0x4f has bit 0)
else                                  → bomb-hit sound   (MK-82: 0x12 has neither)
```

Bits 2–3 of the lower byte are present in consistent category-level patterns across all 135 `.JT` files but no function in the full FA.EXE decompile or any analyzed overlay DLL tests them individually. Remaining inferred assignments:

| Bit | Hex | Inferred pattern | Notes |
|-----|-----|-----------------|-------|
| 2 | 0x000004 | Set for missiles, rockets, and gun rounds; clear for gravity bombs | Possible "active ordnance" marker |
| 3 | 0x000008 | Set for guided missiles and gun rounds; clear for most unguided weapons | Possible "terminal-guidance capable" marker |

Bits 1, 5, and 6 were previously listed as inferred — all three are now confirmed from `_PROJMoveProc` / `_PROJAdd_40` decompile (2026-05-19) and moved to the confirmed table above.

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

The PROJ_TYPE section base is at `missile+0xa6` in the runtime entity (confirmed entity base at `cjt` in the FA scratchpad; PROJ_TYPE base at `DAT_0050d30e`). Confirmed reads from `_PROJHitChance@28` (0x004c3380), `FUN_004c3960` (proximity fuze check), `_PROJMoveProc` (0x4c11b0), `_PROJEngineState_0` (0x4c1170), `FUN_004c17a0`, and `FUN_004c1660`:

| PROJ_TYPE offset | Global addr | Role (confirmed) | Source |
|-----------------|-------------|-----------------|--------|
| +0x4F | `DAT_0050d35d` | Proximity fuze range (byte) — fuze triggers when `target_arm_size ≥ this`; below 50% = miss | `FUN_004c3960` |
| +0x55 | `DAT_0050d363` | Min maneuver rate (short) — lower clamp in `PROJSpeed` velocity computation | `PROJSpeed` |
| +0x57 | `DAT_0050d365` | Coast/glide speed — target speed during engine state 2 (coast phase, motor off) | `_PROJMoveProc` + `_PROJEngineState_0` |
| +0x59 | `DAT_0050d367` | Boost phase end (ticks after launch) — transition from state 0 (boost) to state 1 (cruise) | `_PROJEngineState_0` |
| +0x5B | `DAT_0050d369` | Motor burnout threshold (ticks after launch) — transition from state 1 (cruise) to state 2 (coast) | `_PROJEngineState_0` |
| +0x5D | `DAT_0050d36b` | Max flight time / missile TTL (ticks) — missile self-destructs when elapsed ≥ this | `_PROJMoveProc` |
| +0x65 | `DAT_0050d373` | Alt-guidance inner range threshold (byte) — if dist>>16 ≥ PROJ_TYPE+0x67 but < this, use inner speed | `FUN_004c1660` (warhead bit 5=1) |
| +0x66 | `DAT_0050d374` | Alt-guidance inner range speed (byte) — `_CreateMove_52` speed at close range | `FUN_004c1660` |
| +0x67 | `DAT_0050d375` | Alt-guidance outer range threshold (byte) — min distance for alt-guidance mode activation | `FUN_004c1660` |
| +0x68 | `DAT_0050d376` | Alt-guidance outer range speed (byte) — `_CreateMove_52` speed at far range | `FUN_004c1660` |
| +0x69 | `DAT_0050d377` | Seeker search angular spread half-width (angle units) — used in `_Rand_4` for scan jitter | `FUN_004c17a0` |
| +0x6B | `DAT_0050d379` | Search reacquisition interval (ticks) — added to `_currentTime` to set next scan-angle update | `FUN_004c17a0` |
| +0x6D | `DAT_0050d37b` | Active search window duration (ticks; 0 = disabled) — while elapsed < this, missile scans via random angles | `_PROJMoveProc` |
| +0x6F | `DAT_0050d37d` | Maneuver rate percentage (byte) — `(this × input_rate) / 100`; upper bound in `PROJSpeed` | `PROJSpeed` |
| +0x70 | `DAT_0050d37e` | Smoke trail flags (bits 0–6 = trail count; bit 7 = continuous smoke flag) | `_PROJMoveProc` |
| +0x71 | `DAT_0050d37f` | Smoke trail param 1 (passed to `_GRAPHICAddSmokeAdder_40`) | `_PROJMoveProc` |
| +0x72 | `DAT_0050d380` | Smoke trail param 2 | `_PROJMoveProc` |
| +0x73 | `DAT_0050d381` | Smoke trail param 3 | `_PROJMoveProc` |
| +0x74 | `DAT_0050d382` | Smoke trail param 4 | `_PROJMoveProc` |
| +0x75 | `DAT_0050d383` | Pk at 0–25% of engagement range (byte) — Pk quartile table entry 0, read by `FUN_004c3890` | `_PROJHitChance@28` |
| +0x76 | `DAT_0050d384` | Pk at 25–50% of engagement range (byte) — Pk quartile table entry 1 | `_PROJHitChance@28` |
| +0x77 | `DAT_0050d385` | Pk at 50–75% of engagement range (byte) — Pk quartile table entry 2 | `_PROJHitChance@28` |
| +0x78 | `DAT_0050d386` | Pk at 75–100% of engagement range (byte) — Pk quartile table entry 3 | `_PROJHitChance@28` |
| +0x79 | `DAT_0050d387` | Approach angle tolerance byte | `_PROJHitChance@28` |
| +0x7A | `DAT_0050d388` | Approach angle weight byte | `_PROJHitChance@28` |
| +0x7B | `DAT_0050d389` | Angular error weight byte (left-shifted 3 for angle table lookup) | `_PROJHitChance@28` |
| +0x7C | `DAT_0050d38a` | Counter-maneuver jam Pk reduction byte — lowers Pk by this % | `_PROJHitChance@28` |
| +0x7D | `DAT_0050d38b` | Altitude/range modifier weight byte | `_PROJHitChance@28` |
| +0x7E | `DAT_0050d38c` | Range gap modifier byte | `_PROJHitChance@28` |
| +0x7F | `DAT_0050d38d` | Pk adjustment byte (applied as signed % of base Pk) | `_PROJHitChance@28` |
| +0x80 | `DAT_0050d38e` | Maneuver agility Pk bonus (ushort) | `_PROJHitChance@28` |

**Engine state machine** (`_PROJEngineState_0` @ 0x4c1170, called from `_PROJMoveProc` when warhead bit 6 is set):

```c
// Returns: 0 = boost, 1 = cruise (motor on), 2 = coast (motor off)
if (elapsed < PROJ_TYPE[+0x59]) return 0;   // boost phase
if (elapsed < PROJ_TYPE[+0x5B]) return 1;   // cruise phase
return 2;                                    // coast phase
// elapsed = _currentT − launch_time (ticks)
// PROJ_TYPE[+0x57] = coast speed target (used when state == 2)
// PROJ_TYPE[+0x5D] = TTL: missile removed when elapsed ≥ this
```

**Seeker scan mode** (enabled when `PROJ_TYPE[+0x6D] != 0` and target entity type == aircraft):
The missile enters a search scan phase while elapsed flight time < `PROJ_TYPE[+0x6D]`. Each `PROJ_TYPE[+0x6B]` ticks, `FUN_004c17a0` generates a new random scan angle within ±`PROJ_TYPE[+0x69]` and applies it via `_ObjPlusAngleParm_8`. When elapsed ≥ `+0x6D`, scan angles are zeroed (passive flight until seeker acquires or TTL).

`FUN_004c3890` (called by `_PROJHitChance@28`) is the Pk range-interpolation function. It reads the engagement range reference at `PROJ_TYPE+0x31` (secondary lobe max range), divides it into quartiles, and linearly interpolates between the four Pk bytes at +0x75–0x78.

`PROJSpeed` is the velocity-clamp function: applies `PROJ_TYPE+0x6F` % to an input angular rate, clamps between `PROJ_TYPE+0x55` minimum and OBJ_TYPE speed-limit fields at `entity+0x67/+0x6b`.

**`_PROJProc` dispatch** (confirmed via `dumpAtForced`, 2026-05-19):

```c
// _PROJProc @ 0x4c1f50
undefined1 * _PROJProc(undefined1 param_1) {
    switch(param_1) {
    case 1: return &LAB_004c1f90;               // init/startup handler
    case 2: return &_PROJMoveProc;              // movement / physics update  (0x4c11b0)
    case 3: return &_PROJEventProc;             // event handler
    case 4: return &_PROJDamageProc;            // damage processing
    default: return NULL;
    }
}
```

**Alternate guidance algorithm** (`FUN_004c1660` @ 0x4c1660, selected when warhead bit 5=1): reads the target entity position, computes distance, then selects guidance speed from a two-tier distance table (+0x65/+0x66 inner range, +0x67/+0x68 outer range) before calling `_CreateMove_52` in positional mode. Falls through to `FUN_004c1630` (standard pursuit mode) when distance is below the outer range threshold.

**Remaining gap — confirmed not used in FA.EXE:**

Full exhaustive search (Ghidra GUI, 2026-05-20): all four PROJ proc types decompiled
(`_PROJMoveProc`, `_PROJEventProc`, `_PROJDamageProc`, guidance proc at `LAB_004c1f90`),
all reachable sub-functions checked, and Ghidra xref on each gap global address — all returned
zero references. These bytes are not accessed by any code in FA.EXE.

| PROJ_TYPE offset | Global addr | Status |
|-----------------|-------------|--------|
| `+0x50` | `DAT_0050d35e` | Zero xrefs in FA.EXE |
| `+0x51` | `DAT_0050d35f` | Zero xrefs in FA.EXE |
| `+0x52` | `DAT_0050d360` | Zero xrefs in FA.EXE |
| `+0x53` | `DAT_0050d361` | Zero xrefs in FA.EXE |
| `+0x54` | `DAT_0050d362` | Not in `_PROJMoveProc` |
| `+0x56` | `DAT_0050d364` | Not in `_PROJMoveProc` — may be high byte of `+0x55` (short) |
| `+0x58` | `DAT_0050d366` | Not in `_PROJMoveProc` — may be high byte of `+0x57` (short) |
| `+0x5A` | `DAT_0050d368` | Not in `_PROJMoveProc` — may be high byte of `+0x59` (short) |
| `+0x5C` | `DAT_0050d36a` | Not in `_PROJMoveProc` — may be high byte of `+0x5B` (short) |
| `+0x5E`–`+0x64` | `DAT_0050d36c`–`DAT_0050d372` | Not in `_PROJMoveProc` — check `FUN_004c1630` |

**Physics scratchpad (`0x50CE80`–`0x50D267`):** A separate fixed-address block preceding the
entity struct (`cjt` base `0x50D268`) holds per-tick missile motion state. Key globals confirmed
from `_PROJMoveProc` full decompile:

| Global | Role |
|--------|------|
| `DAT_0050ce91` | Missile world position (passed as `&pos` to `_DistToObj@8`, `_GRAPHICAddExp`) |
| `DAT_0050ce95` | Remaining range accumulator — decremented by motor deceleration each tick |
| `DAT_0050cf5e` | Runtime state flags (bit 1 = acquired, bit 2 = fired, bit 3 = skip guidance, bit 4 = alt-guidance, bit 8 = seeker override) |
| `DAT_0050cf62` | Warhead sub-parameter (passed to `_PROJLock@24`, `_PROJSendCollateralDamages`) |
| `DAT_0050cf64` | Target entity slot ID (0 = no target; indexes `_objPtrs[]`) |
| `DAT_0050cf68` | Phase timing reference — `_currentT − this` = elapsed ticks since phase start |
| `DAT_0050cf70` | Max seeker engagement distance (compared to `iVar6` = `_DistToObj` result) |
| `ram0x0050cf80` | Motor deceleration accumulator (capped at 0x5000, subtracted from `ce95` when bit 6 set) |
| `ram0x0050cf82` | Seeker scan azimuth angle (passed to `@ObjPlusAngleParm@8`) |
| `DAT_0050cf84` | Seeker scan elevation angle (passed to `@ObjPlusAngleParm@8`) |
| `DAT_0050cf86` | Guidance update timer — next scheduled scan angle update (compared to `_currentT`) |

**Sub-functions checked (Ghidra GUI, 2026-05-20) — all are one-liners, no gap field reads:**

| Function | Address | Body |
|----------|---------|------|
| `FUN_004c1630` | `0x4c1630` | `_CreateMove@52(&DAT_0050ceb8, 0, 4, target_slot, 4, 0, 1, 0, 1, 0, 8, 0x10, 0x7fff)` — standard pursuit, fixed params |
| `FUN_004c17f0` | `0x4c17f0` | Seeker acquisition eligibility: checks atmosphere layer bit 3 + sun/heading angle constraints; no PROJ_TYPE reads |
| `FUN_004c1720` | `0x4c1720` | `_CreateMove@52(&DAT_0050ceb8, 0, 1, heading, 1, pitch, 1, roll, 1, 0, 8, 0x10, 0x7fff)` — lock handler, uses missile own angles |
| `FUN_004c1760` | `0x4c1760` | `_CreateMove@52(&DAT_0050ceb8, 0, 1, _sunAngle, 1, az, 1, roll, 1, 0, 8, 0x10, 0x7fff)` — acquisition, sun-angle homing |

**Conclusion:** The scattered gap bytes `+0x56`/`+0x58`/`+0x5A`/`+0x5C` are the high bytes of the
confirmed short fields immediately preceding them (`+0x55`/`+0x57`/`+0x59`/`+0x5B`) — not
independent fields. The block `+0x50`–`+0x54` and isolated range `+0x5E`–`+0x64` have zero xrefs
in all of FA.EXE — no missile code, no BRF code, no generic entity loop touches them. These bytes
are either dead/unused fields from an earlier engine version, or are read exclusively by overlay
DLLs. Semantics cannot be determined from FA.EXE alone.

### Confirmed entity offsets 0xF0–0x16F (from DumpPROJDispatch run)

The entity-relative offsets around the PROJ_TYPE base (`missile+0xa6`) were confirmed by scanning entity+0xF0–0x114:

| Runtime offset | Role (confirmed) |
|----------------|-----------------|
| `entity+0xF0` | Target ptr / target ID |
| `entity+0xF4` | Reaction param 0 |
| `entity+0xF6` | Reaction param 1 |
| `entity+0xF8` | Reaction param 2 |
| `entity+0xFA` | Mode byte |
| `entity+0x100` | **Primary per-frame state byte** — most-polled field in the entity update loop |
| `entity+0x101` | Timeout / tick timer |
| `entity+0x10C` | Campaign context reference |
| `entity+0x114` | Init handle |
| `entity+0x16F` | HUD state flags (also `DAT_0050cfef` = player entity + 0x16F per HUD.md) |

