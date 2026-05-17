# ECM — Electronic Countermeasures Definition (.ECM)

FA_2.LIB contains 30 .ECM files. Each defines the electronic warfare suite for one aircraft type (e.g. F15.ECM, AV8.ECM, B52.ECM). Loaded at runtime by the FA weapon/sensor system.

## Format

Brent's Relocatable Format (plain text). NOT a Win32 PE DLL. Very small — F15.ECM decompresses to 352 bytes.

All directives use the standard BRF syntax: `byte`, `word`, `dword`, `ptr`, `string`, `end`. Labels use `:label_name`. Hex values use `$XX` prefix.

## Structure

Full F15.ECM example:

```
[brent's_relocatable_format]
    byte 9              ; type identifier (9 = ECM)
    ptr si_names
    word 0              ; quantity (0 = built-in; N = pod carrying capacity)
    byte $0             ; category (0 = built-in suite, 1 = external pod)
    word $1f0           ; ECM power level (0=none, $f0=partial, $1f0=full active jamming)
    byte 30             ; effectiveness A  (variable — aircraft quality)
    byte 35             ; band constant 1  (fixed across all ECM files)
    byte 95             ; band constant 2  (fixed)
    byte 24             ; band constant 3  (fixed)
    byte 30             ; effectiveness B  (variable)
    byte 35             ; band constant 4  (fixed)
    byte 159            ; band constant 5  (fixed)
    byte 31             ; band constant 6  (fixed)
    byte 30             ; effectiveness C (+0x12) = radar Pk reduction (variable)
    word 100            ; overall ECM strength (100 = full; 0 = none)
    byte 0
    byte 0
    byte 40             ; secondary effectiveness (+0x17) = IR Pk reduction (variable)
    word 100            ; secondary strength
    byte 0
:si_names
    string ""           ; short name (empty for all ECM files)
    string ""           ; long name
    string "F15.ECM"    ; filename (self-reference)
    end
```

### Pod vs. Built-In

| Category | `word` (qty) | `byte` (type) | Examples |
|----------|-------------|---------------|---------|
| Built-in suite | 0 | $0 | F14, F15, F22, A10, B52, EA6, MIG21 |
| External pod | N | $1 | ALE40 (qty 100), ALQ72 (qty 224), ALQ167 (qty 310) |

### ECM Power Levels

| `word` | Decimal | Meaning | Examples |
|--------|---------|---------|---------|
| $0 | 0 | No active jamming | MIG21, ALE40 (chaff/flare only) |
| $f0 | 240 | Radar-only active jammer | ALQ72 |
| $1f0 | 496 | Radar + IR jammer | F14, F15, F22, B52, EA6, ALQ167 |

**Power field is a bitmask** (confirmed via `@HARDFindJammer@4` at 0x00452EA0):

| Bit | Hex | Meaning |
|-----|-----|---------|
| 4 | `0x010` | Radar jammer active |
| 8 | `0x100` | IR jammer active |

`@HARDFindJammer@4` iterates hardpoints of type 9 (ECM) and rejects pods where the seeker type does not match the active jammer bit: IR seekers need `0x100`, radar seekers need `0x10`. A pod with `$f0` = `0000 1111 0000` has bit 4 set (radar) but not bit 8 (no IR). A pod with `$0` passes neither check and functions as a passive dispenser only.

The bits 5–7 (the four-band cluster 0xF0) likely represent individual radar frequency-band coverage — the five fixed constants (35, 95, 24, 159, 31) in the effectiveness block may correspond to these band thresholds — but that correlation is not yet confirmed.

### Effectiveness Bytes

The 9-byte block contains three **variable** effectiveness bytes (positions 1, 5, 9) interleaved with five **fixed** band constants (35, 95, 24, 159, 31). All roles confirmed via Ghidra:

| Aircraft | eff_A (+0x0A) | eff_B (+0x0E) | eff_C (+0x12) | +0x17 (IR Pk) | ECM power |
|---------|--------------|--------------|--------------|--------------|-----------|
| EA6 (dedicated EW) | 99 | 99 | 80 | 80 | $1f0 |
| F22, A10, F14, B52 | 50 | 50 | 30–50 | 30–50 | $1f0 |
| F15 | 30 | 30 | 30 | 30 | $1f0 |
| MIG21 | 20 | 20 | 0 | 0 | $0 |
| ALE40 (chaff/flare) | 30 | 30 | 0 | 0 | $0 |
| ALQ72 (radar jammer) | 0 | 0 | 30 | 0 | $f0 |

**Byte 9 (+0x12) = radar jamming effectiveness** (confirmed). `_PROJHitChance@28` (0x004c3380) reads `ecm_ptr+0x12` when the shooter has a radar seeker (`missile+0xb4 == 3`): `Pk_final = (100 − eff_C) × Pk_base / 100`.

**Secondary byte (+0x17) = IR jamming effectiveness** (confirmed). Same function reads `ecm_ptr+0x17` for IR seekers (`missile+0xb4 == 2`).

**Byte 1 (+0x0A) = chaff effectiveness; Byte 5 (+0x0E) = flare effectiveness** (confirmed). `_DAMAGEDoHit@12` (0x0040f970) case 9 reads these independently across three hit-probability bands when the player is hit:

```c
// 0–24%: active jammer hit — full lock break
if ((*(ushort *)(entity + 0x08) & 0x110) != 0) { *(ecm_struct + 4) = 0; }
// 25–64%: chaff dispenser hit — radar lock disruption
if (entity[0x0A] != 0) { *(ecm_struct + 6) = 0; }
// 65–99%: flare dispenser hit — IR lock disruption
if (entity[0x0E] != 0) { *(ecm_struct + 7) = 0; }
```

ALE40 has eff_A=30 and eff_B=30 (both dispensers present); ALQ72 has eff_A=0 and eff_B=0 (active jammer only, no passive dispensers). ✓

## File Inventory

| File | Notes |
|------|-------|
| A6.ECM | Intruder |
| A7.ECM | Corsair II |
| A10.ECM | Thunderbolt II |
| AC130.ECM | Spectre gunship |
| ALE40.ECM | Expendable countermeasures dispenser |
| ALQ167.ECM | ECM pod |
| ALQ72.ECM | ECM pod |
| AV8.ECM | Harrier (limited ECM) |
| B52.ECM | Stratofortress |
| EA6.ECM | Prowler (dedicated EW aircraft) |
| F4.ECM | Phantom II |
| F8.ECM | Crusader |
| F14.ECM | Tomcat |
| F15.ECM | Eagle |
| F18.ECM | Hornet |
| F22.ECM | Raptor |
| F104.ECM | Starfighter |
| KA50.ECM | Hokum |
| MIG21.ECM | Fishbed |
| MIG27.ECM | Flogger |
| MIG29.ECM | Fulcrum |
| MIG31.ECM | Foxhound |
| SEAHAR.ECM | Sea Harrier |
| SU24.ECM | Fencer |
| SU27.ECM | Flanker |
| SU37.ECM | Terminator |
| TU26.ECM | Backfire |
| TU95.ECM | Bear |
| TU160.ECM | Blackjack |
| YAK141.ECM | Freestyle |

**Location:** FA_2.LIB | **Count:** 30

## Related Formats

- [SEE.md](SEE.md) — Seeker/sensor definitions; ECM degrades seeker performance
- BRF.md — Aircraft definitions that reference ECM suites
- JT.md — Weapon seekers that ECM jams

## Calibration

### Effectiveness byte semantics — Resolved

- **+0x0A = chaff effectiveness**: confirmed via `_DAMAGEDoHit@12` — non-zero means aircraft has chaff dispensers.
- **+0x0E = flare effectiveness**: confirmed via `_DAMAGEDoHit@12` — non-zero means aircraft has flare dispensers.
- **+0x12 = radar jamming effectiveness**: confirmed via `_PROJHitChance@28`. Applied as `(100 − byte) × Pk / 100`.
- **+0x17 = IR jamming effectiveness**: confirmed via `_PROJHitChance@28` for IR-guided weapons.

### ECM power field (`word $1f0`) — Resolved

`$1f0` is a **bitmask**. Bits 4 and 8 are the operative flags (confirmed via `@HARDFindJammer@4`). See the Power Levels table above. The band-bit hypothesis for bits 5–7 (the 0xF0 cluster) is plausible but not confirmed.

## TODO

- Cross-reference five fixed constants (35, 95, 24, 159, 31) against known RWR band frequencies to confirm band-bit map
