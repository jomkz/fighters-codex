---
format: ECM
name: Electronic Countermeasures Definition
extensions: [".ECM"]
family: BRF
category: typedef
endianness: none
spec:
  status: complete
codec:
  direction: round-trip
  byte_identical: true
  lib: [lib/src/brf.cpp, lib/src/ot.cpp]
  commands: [ecm]
  tests: [tests/test_brf.cpp, tests/test_ot.cpp]
  fuzz: []
  gui: [gui/src/editors/brf_editor.cpp]
  fixtures:
    synthetic: true
    real_manifest: true
related: [BRF, SEE, JT]
---

# ECM — Electronic Countermeasures Definition (.ECM)

FA_2.LIB contains 30 `.ECM` files. Each defines the electronic warfare suite
for one aircraft type (e.g. F15.ECM, AV8.ECM, B52.ECM). [BRF](BRF.md) plain
text (NOT a Win32 PE DLL); very small — F15.ECM decompresses to 352 bytes.
Loaded at runtime by the FA weapon/sensor system.

## Tools

### fx

```
fx ecm info   <file.ECM>               # human-readable field dump
fx ecm unpack <file.ECM> [-o out.txt]  # editable text
fx ecm pack   <in.txt>   -o out.ECM    # write back (byte-identical)
```

## File Layout

Plain text; BRF syntax (see [BRF.md](BRF.md)). Hex values use `$XX`.

Full F15.ECM example:

```
[brent's_relocatable_format]
    byte 9              ; type identifier (9 = ECM)
    ptr si_names
    word 0              ; quantity (0 = built-in; N = pod carrying capacity)
    byte $0             ; category (0 = built-in suite, 1 = external pod)
    word $1f0           ; ECM power level (0=none, $f0=partial, $1f0=full active jamming)
    byte 30             ; effectiveness A  (variable — chaff)
    byte 35             ; band constant 1  (fixed across all ECM files)
    byte 95             ; band constant 2  (fixed)
    byte 24             ; band constant 3  (fixed)
    byte 30             ; effectiveness B  (variable — flare)
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

**Power field is a bitmask** (confirmed via `@HARDFindJammer@4` at
0x00452EA0):

| Bit | Hex | Meaning |
|-----|-----|---------|
| 4 | `0x010` | Radar jammer active |
| 5–7 | `0x0E0` | Unused — always set alongside bit 4 in active-radar ECM files; no function tests these bits individually |
| 8 | `0x100` | IR jammer active |

`@HARDFindJammer@4` iterates hardpoints of type 9 (ECM) and rejects pods where
the seeker type does not match the active jammer bit: IR seekers need `0x100`,
radar seekers need `0x10`. A pod with `$f0` = `0000 1111 0000` has bit 4 set
(radar) but not bit 8 (no IR). A pod with `$0` passes neither check and
functions as a passive dispenser only.

Bits 5–7 confirmed unused: exhaustive scan of all AND/TEST instructions in the
ECM/jammer code range (0x452000–0x454000) found zero tests against these bits
of the power word. They are set in all active-radar ECM files (`$f0` = bits
4–7; `$1f0` adds bit 8) but the engine dispatches exclusively on bits 4 and 8.

### Effectiveness Bytes — resolved

The 9-byte block (BRF offsets +0x0A–+0x12) contains three **variable**
effectiveness bytes interleaved with **six fixed jamming-geometry bytes**
forming two groups of three (radar at +0x0B/+0x0C/+0x0D; IR at
+0x0F/+0x10/+0x11). All roles confirmed via Ghidra (`HARDCanLoad` and related
callers):

| Aircraft | eff_A (+0x0A) | eff_B (+0x0E) | eff_C (+0x12) | +0x17 (IR Pk) | ECM power |
|---------|--------------|--------------|--------------|--------------|-----------|
| EA6 (dedicated EW) | 99 | 99 | 80 | 80 | $1f0 |
| F22, A10, F14, B52 | 50 | 50 | 30–50 | 30–50 | $1f0 |
| F15 | 30 | 30 | 30 | 30 | $1f0 |
| MIG21 | 20 | 20 | 0 | 0 | $0 |
| ALE40 (chaff/flare) | 30 | 30 | 0 | 0 | $0 |
| ALQ72 (radar jammer) | 0 | 0 | 30 | 0 | $f0 |

- **+0x0A = chaff effectiveness** — confirmed via `_DAMAGEDoHit@12`; non-zero
  means aircraft has chaff dispensers.
- **+0x0E = flare effectiveness** — confirmed via `_DAMAGEDoHit@12`; non-zero
  means aircraft has flare dispensers.
- **+0x12 = radar jamming effectiveness** — confirmed via `_PROJHitChance@28`
  (0x004c3380), read when the shooter has a radar seeker
  (`missile+0xb4 == 3`): `Pk_final = (100 − eff_C) × Pk_base / 100`.
- **+0x17 = IR jamming effectiveness** — same function reads `ecm_ptr+0x17`
  for IR seekers (`missile+0xb4 == 2`).

`_DAMAGEDoHit@12` (0x0040f970) case 9 reads the dispenser bytes independently
across three hit-probability bands when the player is hit:

```c
// 0–24%: active jammer hit — full lock break
if ((*(ushort *)(entity + 0x08) & 0x110) != 0) { *(ecm_struct + 4) = 0; }
// 25–64%: chaff dispenser hit — radar lock disruption
if (entity[0x0A] != 0) { *(ecm_struct + 6) = 0; }
// 65–99%: flare dispenser hit — IR lock disruption
if (entity[0x0E] != 0) { *(ecm_struct + 7) = 0; }
```

ALE40 has eff_A=30 and eff_B=30 (both dispensers present); ALQ72 has eff_A=0
and eff_B=0 (active jammer only, no passive dispensers). ✓

### Jammer Geometry Bytes — confirmed

The six fixed bytes form two groups used by the jammer placement /
seeker-defeat calculation:

| Offset | Value | Group | Role |
|--------|-------|-------|------|
| `+0x0B` | 35 | Radar | Effectiveness-scaling parameter — passed directly to `PROJRetargetMissilesOnDevice` / `MPLaunchDevice` |
| `+0x0C` | 95 | Radar | Jammer azimuth half-angle — passed as `95 << 8` (fixed-point) to `MakeObjRotationMatrix` param_2 |
| `+0x0D` | 24 | Radar | Jammer elevation half-angle — passed as `24 << 8` (fixed-point) to `MakeObjRotationMatrix` param_3 |
| `+0x0F` | 35 | IR | Effectiveness-scaling parameter — same role as +0x0B |
| `+0x10` | 159 | IR | Jammer azimuth half-angle — passed as `159 << 8` to `MakeObjRotationMatrix` param_2 |
| `+0x11` | 31 | IR | Jammer elevation half-angle — passed as `31 << 8` to `MakeObjRotationMatrix` param_3 |

The radar group is selected when `cStack_29 == 3` (radar seeker); the IR group
when `cStack_29 == 2` (IR seeker). Band codes 0x0C (radar) and 0x0D (IR) are
passed to `GRAPHICAddDevice` as the jamming-band identifier.

**`MakeObjRotationMatrix` confirmed** (from `PROJLaunchDevice` decompile):
takes `(output_buf, azimuth_halfangle<<8, elevation_halfangle<<8,
vertical_tilt)`. It computes the seeker's angular-bounds block (5 dwords) for
cone-overlap testing. When vertical tilt is 0 (all game calls), it initialises
symmetric ±azimuth / ±elevation bounds then calls `sincos` (sin/cos) and
`FUN_004cf2d0` (rotation). The larger the half-angle byte, the wider the
jammer coverage arc — IR (159/31) is much wider in azimuth than radar (95/24).

**Hypothesis disproven:** the original theory that these bytes encode RWR
radar-band frequencies (L, S, C, X, Ka) is incorrect. They are jammer
effectiveness and beam-geometry parameters, consistent across all 30 ECM files
because they describe the jammer hardware characteristics, not
aircraft-specific threat coverage.

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

All 30 live in FA_2.LIB.

## Related

**Formats:** [BRF](BRF.md) — family grammar and the aircraft definitions that
reference ECM suites; [SEE](SEE.md) — seeker/sensor definitions whose
performance ECM degrades; [JT](JT.md) — weapon seekers that ECM jams.
