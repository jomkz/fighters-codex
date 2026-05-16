# Theater / Map Layout (.MM)

FA_2.LIB contains 75 `.MM` files. Each defines a theater — the top-level scene configuration for a group of missions, referencing terrain, weather, time-of-day, and the object layout. Format is **plain ASCII text** with a keyword/argument syntax.

## Format

Plain ASCII text, CRLF line endings. Begins with the literal token `textFormat` on the first line, followed by keyword–value pairs, one per line. Blocks (such as `obj`) use indented sub-fields.

### Known Keywords (from APA.MM)

| Keyword | Arguments | Description |
|---------|-----------|-------------|
| `textFormat` | *(none)* | File type marker, always first line |
| `map` | `<name>.T2` | Terrain map file to load |
| `layer` | `<name>.LAY <index>` | Cloud/atmosphere layer file and slot |
| `clouds` | `<int>` | Cloud density setting |
| `wind` | `<speed> <direction>` | Wind speed and bearing |
| `view` | `<x> <y> <z>` | Initial camera/view position |
| `sides4` | *(block)* | 4-entry side faction table (hex values `$00`/`$80`) |
| `time` | `<hour> <minute>` | Mission start time of day |
| `historicalera` | `<int>` | Era index (affects available equipment) |
| `obj` | *(block)* | Object placement entry |
| `type` | `<name>.OT` | Object type reference within `obj` block |
| `pos` | `<x> <y> <z>` | Object world-space position within `obj` block |

### Example — APA.MM (excerpt)

```
textFormat
map apa.T2
layer day2.LAY 0
clouds 0
wind 65 7
view 2766 1476602 901701
sides4
    $0
    $80
    ...
time 12 0
historicalera 4
obj
    type STRIP1.OT
    pos 890323 0 462935
```

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 75 |

## TODO — Deep Dive

- Document all keyword variants by surveying all 75 `.MM` files
- Clarify `sides4` block encoding (appears to be faction alignment flags per slot)
- Determine coordinate system units and scale for `view` and `pos`
- Document all `obj` sub-fields beyond `type` and `pos`

## Related

- [T2.md](T2.md) — terrain height/color/type maps referenced via `map` keyword
- [LAY.md](LAY.md) — cloud layer files referenced via `layer` keyword
- [MISSION.md](MISSION.md) — `.M` individual missions that are placed within a theater
- [CAM.md](CAM.md) — campaign definitions that group `.MM` theaters
