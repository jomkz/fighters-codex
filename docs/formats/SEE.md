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

`dword ^XXXXXX` uses the `^` (relative/fixed-point) prefix. Exact scale requires calibration against known weapon/radar ranges in nm or km.

### Seeker Type Byte

| Value | Type |
|-------|------|
| `byte 0` | Visual (VIS*.SEE) |
| `byte 1` | IR/EO (targeting pods: PAVEKNF, PAVESPK) |
| `byte 2` | Unknown |
| `byte 3` | Radar (aircraft radar seekers: F15R, F14R, MIG29R, etc.) |

### Sentinel Values

- `dword $80000000` — minimum heading error sentinel (no lower limit)
- `dword $7fffffff` — maximum heading error sentinel (no upper limit)

## Dual-Lobe Structure

Each SEE file defines two detection lobes, each with independent azimuth/elevation half-angles, range limits, heading error limits, and probability of detection. Possible interpretations:

- Main search lobe vs. boresight/lock-on lobe
- Two radar operating modes (search vs. track)
- Inner and outer acquisition zones

Exact semantics require FA.EXE disassembly of the targeting/guidance loop.

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

## TODO

- Decode range unit scale — calibrate `^XXXXXX` against known weapon/radar ranges in nm or km
- Decode seeker type byte fully — `byte 2` value unobserved or unidentified
- Determine how dual-lobe structure maps to game mechanics (main vs. boresight? search vs. track modes?)
- Verify sentinel values `$80000000` / `$7fffffff` interpretation against FA.EXE guidance code
