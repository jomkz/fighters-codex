# GAS — External Fuel Tank Definition (.GAS)

FA_2.LIB contains 4 .GAS files. Each defines one external fuel tank type that can be carried as a stores item. Loaded at runtime by the FA weapon/stores system.

## Format

Brent's Relocatable Format (plain text). NOT a Win32 PE DLL. Very small — F150.GAS decompresses to 204 bytes.

All directives use the standard BRF syntax: `byte`, `word`, `dword`, `ptr`, `string`, `end`. Labels use `:label_name`. Hex values use `$XX` prefix.

## Structure

```
[brent's_relocatable_format]
    byte 8              ; type identifier (8 = fuel tank)
    ptr si_names
    word <capacity>     ; fuel capacity (internal units)
    byte $1             ; flags (always $1)
    dword <mass>        ; full-tank mass (internal units, ≈ 6.6× gallon count)
:si_names
    string "<short>"    ; short display name
    string "<long>"     ; long display name
    string "<file>.GAS" ; filename (self-reference)
    end
```

## All Four Tanks

| File | word (capacity) | dword (mass) | Short name | Long name |
|------|----------------|--------------|------------|-----------|
| F150.GAS | 108 | 990 | "150 gallon tank" | "150 Gallon External Fuel Tank" |
| F250.GAS | 198 | 1650 | "250 gallon tank" | "250 Gallon External Fuel Tank" |
| F350.GAS | 248 | 2300 | "350 gallon tank" | "350 Gallon External Fuel Tank" |
| F500.GAS | 315 | 3300 | "500 gallon tank" | "500 Gallon External Fuel Tank" |

## Field Notes

### Mass (dword)

The `dword` mass value is consistently approximately 6.6× the gallon count:

| Tank | Gallons × 6.6 | dword |
|------|---------------|-------|
| 150 gal | 990 | 990 |
| 250 gal | 1650 | 1650 |
| 350 gal | 2310 | 2300 |
| 500 gal | 3300 | 3300 |

Jet fuel (JP-8) weighs approximately 6.6 lb/US gallon, so `dword` is most likely **fuel weight in pounds** (full tank).

### Capacity (word)

The `word` capacity unit is not yet decoded. It is not gallons directly — 150 gallons maps to word 108, suggesting a conversion factor near 0.72 or an internal volume unit. May be tenths of some measure, or a simulator-internal fuel unit.

### Flags (byte $1)

Always `$1` across all four files. Likely a stores category flag indicating this item is a fuel tank rather than a weapon.

**Location:** FA_2.LIB | **Count:** 4

## Related Formats

- BRF.md — Aircraft flight model; defines base fuel capacity and consumption rates
- JT.md — Stores system that GAS files participate in alongside weapons

## Calibration

### Capacity word

The `word` values (108, 198, 248, 315) do not map linearly to US gallons. Method:

1. Open an aircraft `.PT` BRF file that can carry the 150-gal tank (e.g. `F16.PT`). Read its internal fuel capacity field.
2. Check the total fuel figure shown on the FA HUD with the 150-gal tank loaded.
3. Subtract internal fuel from total — the remainder maps to word 108 → derive the conversion factor.
4. Alternatively: search FA.SMS for `GAS`, `fuel`, or `tank` symbols. The function that reads the capacity `word` and adds to the aircraft's fuel pool will show the conversion arithmetic.

Hypothesis: if internal fuel uses the same unit, values may be in some simulator-internal volume tick (108 internal units per 150 US gallons ≈ 0.72 units/gallon).

### Mass dword

Already well-constrained: mass ≈ 6.6× gallon count ≈ **fuel weight in pounds** (JP-8 ≈ 6.6 lb/US gal). Consistent across all four tanks to within rounding.

## TODO

- Decode capacity `word` unit via `.PT` internal fuel cross-reference or FA.SMS fuel symbols
- Confirm mass `dword` = pounds via FA.EXE physics code
