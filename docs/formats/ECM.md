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
    word 0              ; flags
    byte $0             ; sub-type / mode
    word $1f0           ; ECM power / frequency band (0 = none, $1f0 = 496 = full suite)
    byte 30             ; effectiveness param 1a
    byte 35             ; effectiveness param 1b
    byte 95             ; effectiveness param 1c
    byte 24             ; effectiveness param 2a
    byte 30             ; effectiveness param 2b
    byte 35             ; effectiveness param 2c
    byte 159            ; effectiveness param 3a
    byte 31             ; effectiveness param 3b
    byte 30             ; effectiveness param 3c
    word 100            ; overall ECM strength (100 = full)
    byte 0
    byte 0
    byte 40             ; chaff/flare effectiveness?
    word 100
    byte 0
:si_names
    string ""           ; short name (empty — ECM has no display name)
    string ""           ; long name (empty)
    string "F15.ECM"    ; filename (self-reference)
    end
```

### AV8.ECM Comparison (Harrier — limited ECM)

- `word $0` instead of `word $1f0` — no ECM power
- Last byte group: `byte 0` instead of `byte 30` — effectiveness zeroed
- `word 0` and `word 0` instead of `word 100` — no strength

### Effectiveness Byte Groups

The 9 effectiveness bytes appear in three groups of 3. Likely semantics:

| Group | Params | Probable Function |
|-------|--------|-------------------|
| 1 | 1a, 1b, 1c | Radar warning receiver (RWR) detection bands |
| 2 | 2a, 2b, 2c | Jamming effectiveness against different radar types |
| 3 | 3a, 3b, 3c | Missile decoy effectiveness |

Exact semantics require FA.EXE disassembly.

### ECM Power Field

`word $1f0` (496 decimal) appears to encode the ECM frequency band or power level. `word 0` indicates no ECM suite. The field may be a bitmask of supported frequency bands.

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

## TODO

- Decode the effectiveness byte triple structure (RWR bands? jamming categories?)
- Decode `word $1f0` frequency field — bitmask or index into a frequency table?
- Cross-reference with FA.EXE ECM evaluation code via FA.SMS symbols
