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
    byte 30             ; effectiveness C  (variable)
    word 100            ; overall ECM strength (100 = full; 0 = none)
    byte 0
    byte 0
    byte 40             ; secondary effectiveness / chaff rate?
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
| $f0 | 240 | Partial active jamming | ALQ72 |
| $1f0 | 496 | Full active jamming suite | F14, F15, F22, B52, EA6, ALQ167 |

### Effectiveness Bytes

The 9-byte block contains three **variable** effectiveness bytes (positions 1, 5, 9) interleaved with five **fixed** band constants (35, 95, 24, 159, 31). The variable bytes scale with aircraft ECM quality:

| Aircraft | Byte 1 | Byte 5 | Byte 9 | ECM power |
|---------|--------|--------|--------|-----------|
| EA6 (dedicated EW) | 99 | 99 | 80 | $1f0 |
| F22, A10, F14, B52 | 50 | 50 | 30–50 | $1f0 |
| F15 | 30 | 30 | 30 | $1f0 |
| MIG21 | 20 | 20 | 0 | $0 |
| ALE40 (chaff/flare) | 30 | 30 | 0 | $0 |
| ALQ72 (jammer pod) | 0 | 0 | 30 | $f0 |

Observations:
- The five fixed bytes (35, 95, 24, 159, 31) appear in all ECM files regardless of quality — likely band-frequency thresholds or system constants.
- ALE40 (chaff/flare dispenser, no active jamming) has non-zero byte 1 and byte 5 but zero byte 9 — suggesting byte 9 correlates with active jamming or IR decoy effectiveness.
- ALQ72 (jammer pod, no chaff) has zeros in byte 1 and byte 5 but non-zero byte 9.
- Exact semantics of the three variable positions require FA.EXE disassembly.

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

### Effectiveness byte semantics

The variable positions (bytes 1, 5, 9) are identified by cross-referencing aircraft types. Their specific roles (radar warning, jamming, decoy) require FA.EXE disassembly:

1. Load FA.EXE in Ghidra with `ImportFASms` labels applied.
2. Search FA.SMS for symbols containing `ECM` (e.g. `?ECMEvaluate@@`, `?GetJammingFactor@@`).
3. In the identified function, find where it reads the effectiveness bytes — the arithmetic (multiply? compare threshold?) against the five fixed constants reveals their role.

### ECM power field (`word $1f0`)

`$1f0` = 496 = `0001 1111 0000` binary. The three-level structure ($0/$f0/$1f0) suggests either an enumerated power level or a bitmask of frequency bands (bits 4–8 set = five bands). The intermediate value $f0 (ALQ72, an older jammer pod) representing "partial" capability supports an additive band-bit interpretation.

## TODO

- Decode roles of effectiveness bytes 1, 5, 9 via Ghidra ECM evaluation function
- Confirm whether `$1f0` is a bitmask (five frequency bands) or an enumerated power level
- Cross-reference five fixed constants (35, 95, 24, 159, 31) against known RWR band frequencies
