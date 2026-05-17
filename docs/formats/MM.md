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

---

## Header Keywords

| Keyword | Arguments | Description |
|---------|-----------|-------------|
| `textFormat` | *(none)* | File type marker — always the first line |
| `map` | `<name>.T2` | Terrain map file to load |
| `layer` | `<name>.LAY <index>` | Cloud/atmosphere layer file and slot index |
| `clouds` | `<int>` | Cloud density (0 = none) |
| `wind` | `<speed> <direction>` | Wind speed and bearing |
| `view` | `<x> <y> <z>` | Initial camera world-space position |

---

## Sides Block

Defines faction alignment. Four variants observed across all 75 files:

| Keyword | Entry count | Notes |
|---------|-------------|-------|
| `sides` | unknown | Rare |
| `sides2` | 19 | Smaller theaters |
| `sides3` | unknown | |
| `sides4` | 64 | Most common; 64 × `$00`/`$80` values |

Each entry is a tab-indented hex byte (`$00` or `$80`). Likely a flat faction relationship table — `$80` = hostile, `$00` = neutral/friendly.

---

## Object Block (`obj`)

Each `obj` block defines one static scene object. Terminated by a lone `.` line.

| Field | Arguments | Description |
|-------|-----------|-------------|
| `type` | `<name>.OT` | Object type reference |
| `pos` | `<x> <y> <z>` | World-space position |
| `angle` | `<yaw> <pitch> <roll>` | Orientation in degrees |
| `flags` | `<hex>` | Object state/behaviour flags |
| `speed` | `<int>` | Initial speed |
| `alias` | `<int>` | Unique object ID (negative integers) |
| `nationality` | `<int>` | Primary faction code |
| `nationality2` | `<int>` | Secondary faction code |
| `nationality3` | `<int>` | Tertiary faction code |
| `name` | `<str>` | Display name |
| `color` | `<int>` | Color index |
| `icon` | `<int>` | Map icon ID (`-1` = no icon) |
| `react` | `<hex> <hex> <hex>` | Reaction/engagement flags |
| `searchDist` | `<int>` | AI detection radius |
| `skill` | `<int>` | AI skill level |
| `tdic` | `<int>` | Tile dictionary index |
| `special` | *(none)* | Marks object as a special/scripted entity |
| `w_for` | `<int>` | Waypoint owner reference |

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

---

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

---

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

The key `k<col3><row3>` zero-pads column and row to 3 digits each (e.g. `k000004` = col 0, row 4). Present in maps that reference these tiles from scripts or other systems.

---

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

---

## Tile Dictionary (`tdic`)

Follows the `tmap` section. Each `tdic` block defines a 4×8 binary passability/collision grid for a tile variant.

```
tdic <id>
    <b0> <b1> <b2> <b3>
    <b0> <b1> <b2> <b3>
    ... (8 rows)
```

Values are `0` (passable) or `1` (blocked). `<id>` observed as 256 in all cases.

---

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 75 |

## TODO — Deep Dive

- Confirm `sides` entry count semantics (is the number in `sides4` a version/type or faction count?)
- Determine world-space coordinate scale and origin for `pos` / `view` values
- Document all `flags` bit assignments for `obj` blocks
- Clarify `tmap_named` second and third argument semantics (position vs tile_id/variant)
- Survey all `w_goal` values to enumerate waypoint goal types
- Confirm `tdic` id=256 meaning (tile type index into T2?)

## Related

- [T2.md](T2.md) — terrain height/color/type maps referenced via `map`
- [LAY.md](LAY.md) — cloud layer files referenced via `layer`
- [MISSION.md](MISSION.md) — `.M` individual missions placed within a theater
- [CAM.md](CAM.md) — campaign definitions that group `.MM` theaters
- [BRF.md](BRF.md) — `.OT` object type definitions referenced by `obj` blocks
