# SEE — Seeker / Sensor Definition (.SEE)

FA_2.LIB contains 51 .SEE files. Each defines the sensor parameters for one seeker type — radar, infrared, targeting pod, or visual acquisition cone. Loaded at runtime by the FA weapon guidance and AI targeting system.

## Format

Brent's Relocatable Format (plain text). NOT a Win32 PE DLL. F15R.SEE decompresses to 470 bytes.

All directives use the standard BRF syntax: `byte`, `word`, `dword`, `ptr`, `string`, `end`. Labels use `:label_name`. Hex values use `$XX` prefix. Fixed-point/relative values use `^XXXXX` prefix.

## Structure

### F15R.SEE — Radar Seeker Example

```
[brent's_relocatable_format]
    byte 10             ; type identifier (10 = seeker/sensor)
    ptr si_names
    word 0              ; seeker identifier / capability flags
    byte $0             ; seeker sub-type
    byte 3              ; seeker type (0=visual, 1=IR, 2=radar active, 3=radar, ...)
    byte $1             ; mode flags
    byte 30             ; (param — likely acquisition time or track rate)
    byte 0 ... byte 0   ; (4 bytes reserved/unused)
    ; --- Primary lobe ---
    word 10920          ; azimuth half-angle (≈ 60° at 182 units/°)
    word 10920          ; elevation half-angle
    dword ^0            ; min range
    dword ^911400       ; max range (fixed-point internal units)
    dword $80000000     ; min heading error (sentinel = no limit)
    dword $7fffffff     ; max heading error (sentinel = no limit)
    ; --- Secondary lobe ---
    word 8190           ; azimuth half-angle (≈ 45°)
    word 8190           ; elevation half-angle
    dword ^0            ; min range
    dword ^607600       ; max range
    dword $80000000
    dword $7fffffff
    byte 100            ; probability of detection lobe 1 (100 = 100%)
    byte 100            ; probability of detection lobe 2
:si_names
    string ""           ; short name (empty for aircraft radar — not a display item)
    string ""           ; long name (empty)
    string "F15R.SEE"   ; filename (self-reference)
    end
```

### PAVEKNF.SEE — Targeting Pod (AVQ-10 Pave Knife)

- `word 425` — pod identifier / capability code
- `byte 1` — seeker type (IR/EO)
- Cone half-angles: `word 32767` (approximately 180° — omnidirectional field?)
- Short max range: `^60760`
- Both lobes identical
- `si_names` contains display label strings (unlike aircraft radars)

### VIS300.SEE — 300° Visual Acquisition Cone

- `word 0` — no capability flags
- `byte 0` — seeker type (visual)
- Azimuth: `word 27300` (≈ 150° half-angle = 300° total arc)
- Elevation: `word 21840` (≈ 120°)
- Empty `si_names` strings

## Encoding

### Angle Encoding

`word 16380 = 90°` (quarter circle), giving a resolution of 16380 / 90 = **182 units per degree**.

| word value | Degrees |
|-----------|---------|
| 8190 | 45° |
| 10920 | 60° |
| 13650 | 75° |
| 16380 | 90° |
| 21840 | 120° |
| 27300 | 150° |
| 32767 | ~180° |

All values are half-angles; total cone coverage is double the stored value.

### Range Encoding

`dword ^XXXXXX` uses the `^` (relative/fixed-point) prefix. **1 unit = 1 foot** (confirmed via cross-reference of multiple sensor files against published system ranges):

| File | Stored value | ÷ 6076 | Known range |
|------|-------------|--------|-------------|
| AV8L.SEE (laser pod) | ^60760 | 10.0 nm | TIALD pod ~10 nm ✓ |
| AIM9X lobe 1 | ^50000 | 8.2 nm | AIM-9X ~8 nm ✓ |
| AIM120 lobe 1 | ^144000 | 23.7 nm | AIM-120A ~25 nm ✓ |
| AGM65G lobe 1 | ^60000 | 9.9 nm | AGM-65G ~10 nm ✓ |
| AGM84A lobe 1 | ^360000 | 59.3 nm | Harpoon ~60 nm ✓ |
| F4BR.SEE (APQ-72) | ^303800 | 50 nm | APQ-72 ~40–50 nm ✓ |
| E3R.SEE (AWACS) | ^1215200 | 200 nm | E-3 ~200–250 nm ✓ |

6076 ft = 1 nautical mile; divide any `^` range value by 6076 to get nm.

### Seeker Type Byte

| Value | Type | Evidence |
|-------|------|----------|
| `byte 0` | Visual | VIS*.SEE files |
| `byte 1` | Laser | AV8L.SEE (Harrier laser designator), KA50L.SEE, SU24L.SEE, SU37L.SEE |
| `byte 2` | IR / EO | AIM9M.JT, AIM9X.JT, AGM65G.JT PROJ_TYPE seeker byte |
| `byte 3` | Radar (active or semi-active) | F4BR.SEE (APQ-72), AV8R.SEE (Blue Fox), E3R.SEE (AWACS), AIM120.JT, AGM84A.JT |

Type byte 1 (laser) confirmed: AV8L.SEE is the AV-8B Harrier laser designator pod; its primary lobe max range `^60760` = 10 nm matches TIALD laser pod operational range.

### Sentinel Values

- `dword $80000000` — minimum heading error sentinel: no lower limit (any heading error passes)
- `dword $7fffffff` — maximum heading error sentinel: no upper limit (any heading error passes)

Confirmed via `_PROJInFOV@40` (0x004c2860): the heading error test explicitly bypasses the check when the sentinel values are present. A lobe with both sentinels set accepts any target heading relative to the sensor. Setting `word 32767` for both half-angles (`0x7FFF`) additionally triggers an unconditional pass that skips the angle check entirely — this is the PAVEKNF.SEE omnidirectional cone.

## Dual-Lobe Structure

Each SEE file defines two detection lobes, each with independent azimuth/elevation half-angles, range limits, heading error limits, and probability of detection.

**`byte $1` at offset 5** (after seeker type byte) appears to be a **dual-mode enable flag**:
- `byte $1` set: F4BR.SEE (APQ-72), E3R.SEE (AWACS) — both have meaningfully different primary/secondary lobe parameters
- `byte $0`: AV8L.SEE, AV8R.SEE — both lobes have the same range (lobes differ only in cone angle)

For radar seekers with `byte $1`, the lobes appear to represent **search mode vs. track mode**:

| File | Lobe 1 (primary / search) | Lobe 2 (secondary / track) |
|------|--------------------------|---------------------------|
| F4BR.SEE | 50 nm, 60° half-angle | 25 nm, 45° half-angle |
| AV8R.SEE | 90 nm, 60° half-angle | 50 nm, 45° half-angle |

Primary lobe is wider and longer-range (search); secondary lobe is narrower and shorter-range (lock-on/track zone). Exact trigger condition (what causes the engine to switch lobes) requires FA.EXE disassembly.

## File Inventory

### Aircraft Radar

| File | Aircraft |
|------|----------|
| A6R.SEE | A-6 Intruder |
| A7R.SEE | A-7 Corsair II |
| A10R.SEE | A-10 Thunderbolt II |
| AC130R.SEE | AC-130 Spectre (radar) |
| AC130I.SEE | AC-130 Spectre (IR) |
| AV8R.SEE | AV-8 Harrier (radar) |
| AV8IR.SEE | AV-8 Harrier (IR) |
| AV8L.SEE | AV-8 Harrier (laser?) |
| B52R.SEE | B-52 Stratofortress |
| E2R.SEE | E-2 Hawkeye |
| E3R.SEE | E-3 Sentry (AWACS) |
| E6R.SEE | E-6 Mercury |
| E8R.SEE | E-8 J-STARS |
| EA6R.SEE | EA-6 Prowler |
| F4R.SEE | F-4 Phantom II |
| F4BR.SEE | F-4B Phantom II |
| F4JR.SEE | F-4J Phantom II |
| F8R.SEE | F-8 Crusader |
| F14R.SEE | F-14 Tomcat |
| F15R.SEE | F-15 Eagle |
| F18R.SEE | F/A-18 Hornet |
| F22R.SEE | F-22 Raptor |
| F104R.SEE | F-104 Starfighter |
| KA50L.SEE | Ka-50 Hokum (laser) |
| MIG21R.SEE | MiG-21 Fishbed |
| MIG27R.SEE | MiG-27 Flogger |
| MIG29R.SEE | MiG-29 Fulcrum (radar) |
| MIG29I.SEE | MiG-29 Fulcrum (IR) |
| MIG31R.SEE | MiG-31 Foxhound |
| SEAHARR.SEE | Sea Harrier |
| SU24R.SEE | Su-24 Fencer (radar) |
| SU24L.SEE | Su-24 Fencer (laser) |
| SU27R.SEE | Su-27 Flanker (radar) |
| SU27I.SEE | Su-27 Flanker (IR) |
| SU35R.SEE | Su-35 |
| SU37R.SEE | Su-37 Terminator (radar) |
| SU37I.SEE | Su-37 Terminator (IR) |
| SU37L.SEE | Su-37 Terminator (laser) |
| TU26R.SEE | Tu-26 Backfire |
| TU95R.SEE | Tu-95 Bear |
| TU160R.SEE | Tu-160 Blackjack |
| YAK141R.SEE | Yak-141 Freestyle |

### Targeting Pods

| File | System |
|------|--------|
| PAVEKNF.SEE | AVQ-10 Pave Knife |
| PAVESPK.SEE | Pave Spike |

### Ground / Offboard Sensors

| File | System |
|------|--------|
| AAS38.SEE | AAS-38 FLIR pod |
| GCIR.SEE | GCI radar |
| REDCR.SEE | Red Crown radar |

### Visual Acquisition Cones

| File | Coverage |
|------|----------|
| VIS180.SEE | 180° arc |
| VIS240.SEE | 240° arc |
| VIS300.SEE | 300° arc |
| VIS340.SEE | 340° arc |

**Location:** FA_2.LIB | **Count:** 51

## Related Formats

- JT.md — PROJ_TYPE seeker section references SEE definitions
- [ECM.md](ECM.md) — ECM degrades seeker performance against SEE parameters
- BRF.md — Aircraft definitions reference radar SEE files

## Calibration

### Range unit — Resolved

Range unit confirmed as **1 foot**. See table in the Range Encoding section above. Note: the earlier F15R.SEE hypothesis (11400 units/nm) was incorrect — it assumed APG-63 range is 80 nm when the stored value implies ~150 nm maximum lobe extent. The per-foot calibration is consistent across 7 independent data points.

### Seeker type byte — Resolved

Full enum confirmed. See Seeker Type Byte table above.

### Dual-lobe semantics — Resolved

Primary lobe = search mode; secondary lobe = track/lock mode. Trigger confirmed via `_PROJLock@24` (0x004c2f20):

The engine maintains two runtime state flags on the projectile/object at struct offset `+0xa6`:
- `0x10000` — search mode active: engine calls `FUN_004c2eb0` (search-lobe timer check)
- `0x20000` — track mode active: engine calls `FUN_004c31f0` (track-lobe check, stricter)

Both lobe-check functions receive the **current target struct** (`DAT_0050ce80`) and a timer-window parameter (0x28 = 40 game ticks).

`FUN_004c2eb0` (search lobe, 0x004c2eb0): when the target has not yet been acquired (`target+0x10 & 0x80 == 0`), the function checks that the target is detectable (`target+0xde & 0x400`) and initialises a lock-hold timer at `target+0x11a` to `now + 40 ticks`. Once the target is marked acquired (`+0x80` set), the function verifies the timer has not expired; if `target+0x11a ≤ now`, the check fails and lock is dropped.

`FUN_004c31f0` (track lobe, 0x004c31f0): identical timer logic, but when the target is acquired (`+0x80` set) it additionally requires `target+0xde & 0x100000` (active-tracking flag). If that flag is absent, the track check fails even if the timer is still valid.

`_PROJInFOV@40` (0x004c2860) then performs the range and heading-error comparison using the lobe selected by `param_3`:
- `param_3 == 0` → primary lobe data (SEE offset `+0x0F`)
- `param_3 != 0` → secondary lobe data (SEE offset `+0x23`)

### F15R.SEE range calibration — Note

`^911400` ÷ 6076 ≈ 150 nm. The APG-63 published FA range is ~80 nm at typical RCS; the stored value represents maximum theoretical lobe extent under ideal conditions, not the pilot-selectable radar range.

## TODO

- Confirm exact trigger that transitions the missile state flag from `0x10000` (search) to `0x20000` (track) and sets `target+0xde & 0x100000`. `_PROJLockUpdate@0` (0x004c0960) is NOT the source — it only counts in-range missiles per seeker type. The transition is in the main missile-service / weapon-update path (not yet decompiled).
