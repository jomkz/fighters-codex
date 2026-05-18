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
    word <empty_weight> ; empty tank structural weight (lbs)
    byte $1             ; flags (always $1 = fuel-tank category)
    dword <fuel_weight> ; full-tank fuel weight (lbs); used to init fuel store
:si_names
    string "<short>"    ; short display name
    string "<long>"     ; long display name
    string "<file>.GAS" ; filename (self-reference)
    end
```

## All Four Tanks

| File | word (empty_weight lbs) | dword (fuel_weight lbs) | Full weight (word+dword) | Short name | Long name |
|------|------------------------|------------------------|--------------------------|------------|-----------|
| F150.GAS | 108 | 990 | 1098 | "150 gallon tank" | "150 Gallon External Fuel Tank" |
| F250.GAS | 198 | 1650 | 1848 | "250 gallon tank" | "250 Gallon External Fuel Tank" |
| F350.GAS | 248 | 2300 | 2548 | "350 gallon tank" | "350 Gallon External Fuel Tank" |
| F500.GAS | 315 | 3300 | 3615 | "500 gallon tank" | "500 Gallon External Fuel Tank" |

## Field Notes

### Fuel weight / dword (fuel_weight)

The `dword` is **full-tank fuel weight in pounds** (confirmed). It is 6.6× the gallon count (JP-8 density = 6.6 lb/US gal):

| Tank | Gallons × 6.6 | dword |
|------|---------------|-------|
| 150 gal | 990 | 990 |
| 250 gal | 1650 | 1650 |
| 350 gal | 2310 | 2300 |
| 500 gal | 3300 | 3300 |

`FUN_00452c20` (hardpoint load) initializes the fuel store as `dword × quantity × 256` — fixed-point (×256) representation. `_BurnFuel@0` decrements this value by `fuel_rate × 5` each simulation batch.

### Empty weight / word (empty_weight)

The `word` is **empty tank structural weight in pounds** (confirmed via `FUN_004516b0`). The loadout weight model computes: `total_tank_weight = empty_weight × count + (current_fuel >> 8)`. This produces correct full-tank weights:

| Tank | word (empty) + dword (fuel) = full weight |
|------|------------------------------------------|
| 150 gal | 108 + 990 = 1098 lbs |
| 250 gal | 198 + 1650 = 1848 lbs |
| 350 gal | 248 + 2300 = 2548 lbs |
| 500 gal | 315 + 3300 = 3615 lbs |

These match real-world 150–500 gallon external fuel tank weights (full) to within typical simulator rounding.

### Flags (byte $1)

Always `$1` across all four files. Stores-category flag: value 1 = fuel tank (as opposed to weapon).

**Location:** FA_2.LIB | **Count:** 4

## Related Formats

- BRF.md — Aircraft flight model; defines base fuel capacity and consumption rates
- JT.md — Stores system that GAS files participate in alongside weapons

## Calibration

### Fuel weight dword — Confirmed as pounds

Cross-referenced against PT internal fuel fields (`PLANE_TYPE + 83` offset):

| Aircraft | PT `dword` (PLANE_TYPE+83) | Known fuel (lbs) |
|----------|---------------------------|-----------------|
| A-10 | 10700 | 10,700 lbs ✓ |
| F-16C | 6972 | 6,972 lbs (Block 25/30) ✓ |

The GAS `dword` is in the same unit — **fuel weight in pounds**. The ratio 990/150 = 6.6 lb/gal confirms JP-8 density (6.6 lb/US gal) to within rounding.

### Empty weight word — Confirmed as pounds (structural weight)

`FUN_004516b0` (loadout weight calculator) reads the `word` field as the base structural weight and adds it to the remaining fuel: `weight = word × count + (fuel_store >> 8)`. Cross-checking against typical external fuel tank empty weights (120–260 lbs for 150–500 gal aluminum tanks) confirms the `word` values are empty tank structural weights in pounds.
