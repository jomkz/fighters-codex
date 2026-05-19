# Theater / Map Layout (.MM)

FA_2.LIB contains 75 `.MM` files. Each defines a theater — the complete scene description for a group of missions: terrain reference, weather, object placements, waypoints, and terrain tile overrides. Format is **plain ASCII text** with a keyword/argument syntax, CRLF line endings.

## File Structure

Sections appear in this order:

1. **File header** — global scene parameters
2. **`sides` block** — faction alignment table
3. **`time` / `historicalera`** — time of day and era
4. **`obj` blocks** — static object placements
5. **`special` blocks** — named map labels (cities, bodies of water)
6. **`tmap` entries** — sparse terrain tile overrides
7. **`tmap_named` entries** — named tile positions (present in some files)
8. **`waypoint2` blocks** — AI/scripted waypoint sequences (present in some files)
9. **`tdic` blocks** — per-tile collision/passability bitmaps

## Header Keywords

| Keyword | Arguments | Description |
|---------|-----------|-------------|
| `textFormat` | *(none)* | File type marker — always the first line |
| `map` | `<name>.T2` | Terrain map file to load |
| `layer` | `<name>.LAY <index>` | Cloud/atmosphere layer file and slot index |
| `clouds` | `<int>` | Cloud density (0 = none) |
| `wind` | `<speed> <direction>` | Wind speed and bearing |
| `view` | `<x> <y> <z>` | Initial camera world-space position |

## Sides Block

Defines faction alignment. Four variants across all 75 files:

| Keyword | Entry count | File count |
|---------|-------------|------------|
| `sides` | 18 | 9 |
| `sides2` | 19 | 32 |
| `sides3` | 24 | 9 |
| `sides4` | 64 | 25 |

Each entry is a tab-indented hex byte: `$00` = neutral/friendly, `$80` = hostile to the player.

The table is a **flat array indexed by faction code**: entry *i* = hostility of side code *i*. The number suffix (2/3/4) is a format version indicating how many faction codes are defined — each version is a strict superset of the previous (`sides2[0–18]` == `sides3[0–18]` == `sides4[0–18]`; `sides3[0–23]` == `sides4[0–23]`). Theaters added in later FA versions use a higher-numbered variant to cover newly introduced faction codes.

The `nationality` field in `obj` blocks is **not** an index into this table — it is a cosmetic UI code (country flag/emblem in briefings) and can exceed 162, far beyond the 64-entry maximum.

## Object Block (`obj`)

Each `obj` block defines one static scene object. Terminated by a lone `.` line.

| Field | Arguments | Description |
|-------|-----------|-------------|
| `type` | `<name>.OT` | Object type reference |
| `pos` | `<x> <y> <z>` | World-space position |
| `angle` | `<yaw> <pitch> <roll>` | Orientation in degrees |
| `flags` | `<hex>` | Object state/behaviour flags (see below) |
| `speed` | `<int>` | Initial speed |
| `alias` | `<int>` | Unique object ID (negative integers) |
| `nationality` | `<int>` | Primary faction code |
| `nationality2` | `<int>` | Secondary faction code |
| `nationality3` | `<int>` | Tertiary faction code |
| `name` | `<str>` | Display name |
| `color` | `<int>` | Color index |
| `icon` | `<int>` | Map icon ID (`-1` = no icon) |
| `react` | `<hex> <hex> <hex>` | Hostile faction bitmask: three 16-bit words covering faction codes 0–47 (bit *n* set = react to faction *n* as hostile) |
| `searchDist` | `<int>` | AI detection radius |
| `skill` | `<int>` | AI skill level |
| `tdic` | `<int>` | Tile dictionary index |
| `special` | *(none)* | Marks object as a special/scripted entity |
| `w_for` | `<int>` | Waypoint owner reference |
| `map_obj_success_flags` | `<alias> <hex>` | Looks up entity by alias, then writes the hex value into entity `ot_flags` bits 5–7: `entity+1 = (value) \| (entity+1 & 0xffffff1f)`. Used to set or restore per-object mission-objective state. The mission save handler (`FUN_00495e80`) serialises current bit 5–7 state back using this keyword. |

### `flags` Bit Survey

8 distinct values observed across all 75 files (8,373 `obj` blocks total):

| Value | Bits | Object types | Count |
|-------|------|--------------|-------|
| `$0` | — | Decorative buildings, flags, landmarks (no game logic) | 471 |
| `$1` | 0 | Fuel depots, bunkers, strips, flags (friendly?) | 968 |
| `$3` | 0, 1 | Mobile military units — all `.NT` (AA guns, SAMs, fighters) | 232 |
| `$401` | 0, 10 | Fuel depots, control towers, strips, flags | 5,524 |
| `$403` | 0, 1, 10 | Mobile military units — all `.NT` | 241 |
| `$601` | 0, 9, 10 | Named structures (carrier KING.OT, houses, factory) | 24 |
| `$4003` | 0, 1, 14 | Runways (STRIP1, DTSTRP, STRIP5, STRIP7) | 906 |
| `$4403` | 0, 1, 10, 14 | Runways (STRIP4, STRIP) | 2 |

Confirmed bit meanings:
- **Bit 0 (`$1`)** — present on all non-decorative objects; likely "active / participates in game logic"
- **Bit 1 (`$2`)** — set on mobile units (`.NT`) and runways; "mobile or functional structure"
- **Bit 14 (`$4000`)** — set only on runway strips; "landing surface"

Confirmed bit meanings (continued):
- **Bit 9 (`$200`)** — **Confirmed** — `@Reaction@12` (0x464040): when set, entity is immediately rejected as a valid AI target (returns `'\x1f'` rejection code, same as non-targetable or dead). Only 24 objects carry this flag (`$601`): carrier KING.OT, houses, and factory types. Semantics: **"protected from AI targeting / not auto-targetable"** — used for mission-critical scene objects enemies should not autonomously engage.
- **Bit 10 (`$400`)** — **Confirmed** — `_Reaction_12` (0x464040) and `_MaskEvents_4` (0x463ea0): drives civilian/light-type event handling. Set on fuel depots, control towers, runway strips, and flags (`$401`). Shared semantic with NT.md bit 10. Semantics: **"civilian or non-combat structure"** — participates in game logic (bit 0 set) but uses lighter event-system dispatch paths.

### Example

```
obj
    type STRIP1.OT
    pos 890323 0 462935
    angle 0 0 0
    nationality3 185
    flags $4003
    speed 0
    alias -10100
    .
```

## Special Block (`special`)

Named geographic labels shown on the theater map (cities, seas, landmarks). Same field set as `obj` but without `type`.

```
special
    pos 1816892 0 1071944
    name East Falkland
    color 48
    icon -1
    flags $0
    .
```

## Terrain Tile Data

### `tmap` — Sparse tile overrides

Only non-default tiles appear; the grid is otherwise implied from the `.T2` terrain file.

```
tmap <col> <row> <tile_id> <variant>
```

- `col` and `row` are multiples of 4 (grid coordinates in 4-unit steps)
- `tile_id` is an index into the terrain tile set
- `variant` is a sub-tile selector (0–3 observed)

### `tmap_named` — Named tile positions

Like `tmap` but with a symbolic key encoding the position:

```
tmap_named k<col3><row3> <col> <row>
```

The key `k<col3><row3>` zero-pads column and row to 3 digits each (e.g. `k000004` = col 0, row 4). The explicit `<col>` and `<row>` arguments are always identical to the values encoded in the key — they are redundant. Present in maps that reference these tiles from scripts or other systems.

## Waypoint Block (`waypoint2`)

```
waypoint2 <count>
    w_index <int>
    w_flags <int>
    w_goal <int>
    w_next <int>
    w_pos2 <unk> <unk> <x> <altitude> <z>
    w_speed <int>
    w_wng <int> <int> <int> <int>
    w_react <int> <int> <int>
    w_searchDist <int>
    w_preferredTargetId <int>
    w_name <str>
    [next waypoint entry...]
```

`count` is the number of waypoint entries that follow. Each entry begins with `w_index`. `w_pos2` has 5 arguments; the first two are always `0 0` in observed files.

**`w_goal` values**: only `0` and `1` observed across all 75 files (192 waypoints total). `w_goal 0` always appears with `w_flags 1` and `w_speed 0` (stationary anchor/spawn point); `w_goal 1` appears with `w_flags 0`, a non-zero speed, and a `w_react` bitmask (active patrol waypoint). Exact goal-type semantics require Ghidra trace.

## Tile Dictionary (`tdic`)

Follows the `tmap` section. Each `tdic` block defines a 4×8 binary passability/collision grid for a tile variant.

```
tdic <id>
    <b0> <b1> <b2> <b3>
    <b0> <b1> <b2> <b3>
    ... (8 rows)
```

Values are `0` (passable) or `1` (blocked). `<id>` observed as 256 in all cases.

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 75 |

## World-Space Coordinate System (Confirmed)

`?MAPWorldToScreen` (FUN_00422380) maps 3D world-space positions to 2D map screen coordinates:

```c
screen_x = DAT_00536508 + (world_x  - DAT_00536520) / DAT_0053664c;
screen_y = DAT_0053650a + (DAT_00536528 - world_z) / DAT_0053664c;
```

- `world_x` / `world_z` are the first and third components of the int[3] world position vector (Y = `world_z` in the map's X/Z plane; `world_y` = altitude, not used for map display)
- `DAT_00536520` / `DAT_00536528` — world-space map center (origin) for X and Z axes (set at theater load time)
- `DAT_00536508` / `DAT_0053650a` — screen center pixel coordinates
- `DAT_0053664c` — world-units-per-pixel scale factor (runtime zoom level); larger = more zoomed out

The Z-axis inversion (`origin_z - world_z`) means positive world-Z maps to upward on screen, consistent with the engine's +Z = northward convention. `pos` and `view` values in MM files are in these same world-space integer units.

**World-space unit = 1 foot (confirmed).** Calibrated via JT.md seeker-range cross-check: `AIM9X lobe 1 max ^50000` = 8.2 nm; 50,000 feet / 6,076 ft/nm ≈ 8.23 nm ✓. The FA engine uses feet throughout for all world-space coordinates.

## Related

- [T2.md](T2.md) — terrain height/color/type maps referenced via `map`
- [LAY.md](LAY.md) — cloud layer files referenced via `layer`
- [M.md](M.md) — `.M` individual missions placed within a theater
- [CAM.md](CAM.md) — campaign definitions that group `.MM` theaters
- [BRF.md](BRF.md) — `.OT` object type definitions referenced by `obj` blocks
