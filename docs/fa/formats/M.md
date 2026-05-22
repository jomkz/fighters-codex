# Mission File (.M)

## Overview

`.M` files define individual missions using a `[textFormat]` container with `[key value]`-style bracketed tokens. The related `.MM` theater map format is distinct — see [MM.md](MM.md).

## File Structure

```
[textFormat]
[brief]
[mapName BALTICMAP]
[layer BALTIC 0]
[time 6 0]
[wind 270 15]
[clouds 20]
[sides 0 1 0 1 0 0 0 0]
[objects
    [pos 12345.0 0.0 67890.0]
    [id {GUID-or-name}]
    [type FIGHTER]
    [side 0]
    ...
\t]
```

### Token format

- `[key value]` — single-line key-value pair; value may be a string, number, or space-
  separated list
- `[key\n ... \t]` — bracketed block: contents terminated by `\t]` on its own line
- Blocks may be nested

### Top-level keys

| Key | Value | Description |
|-----|-------|-------------|
| `brief` | (no value) | Mission has a briefing |
| `mapName` | string | Theater map identifier |
| `layer` | name index | Terrain layer |
| `time` | hour min | Start time of day |
| `wind` | dir_deg speed | Wind conditions |
| `clouds` | percent | Cloud cover 0–100 |
| `sides` | 8 integers | Team assignments for each side slot |
| `objects` | block | Object placement list |

### Object block fields

| Key | Value |
|-----|-------|
| `pos` | x y z (feet) |
| `id` | unique identifier string |
| `type` | object type name (references .OT/.PT files) |
| `side` | integer team index |
| `heading` | degrees |
| `speed` | initial speed |
| `alt` | initial altitude (feet) |
| `alias` | signed integer; used with `w_preferredTargetId2` to link waypoints to targets |
| `nationality3` | integer country code |
| `flags` | hex flags controlling object behavior |
| `skill` | AI skill level |
| `react` | reaction flags |
| `searchDist` | detection range |

Field list varies by object type; unknown fields are preserved verbatim on round-trip.

### Preferred target system

Waypoints can designate a specific object as their attack target using
`w_preferredTargetId2`. The value is the object's `alias` field encoded as a **negated
unsigned 16-bit hex** value:

```
hex_value = uint16_t(alias)      (equivalently: 0x10000 + alias, since alias is negative)
```

| Alias | `w_preferredTargetId2` |
|-------|----------------------|
| âˆ’1 | `$ffff` |
| âˆ’2 | `$fffe` |
| âˆ’255 | `$ff01` |
| âˆ’256 | `$ff00` |
| âˆ’257 | `$feff` |
| âˆ’288 | `$fee0` |

The pattern continues linearly; alias âˆ’N = `$(10000 âˆ’ N)` in hex. Up to 288 preferred
target slots are supported (`$ffff` through `$fee0`). Does not apply to map objects or
the player object.

Example:
```
; Waypoint
w_preferredTargetId2 $ff01

; Target object with matching alias
obj
    type TRUCK.NT
    alias -255
```

## Round-Trip Notes

- Parse â†’ serialize produces byte-identical files for all 517 `.M` files in FA_2.LIB.
- Tab-indented blocks must use a real tab character, not spaces.
- The `\t]` terminator is literally `<TAB>]` (not backslash-t).

## fx commands

```
fx mission info   <file.M>              # map name, time, object count
fx mission unpack <file.M> [-o out.txt] # editable text
fx mission pack   <in.txt>  -o out.M    # write back
```

## .MT — Mission Text / Briefing

`.MT` files are plain ASCII companions to each `.M` file containing the mission briefing
and debrief text displayed in-game. See [MT.md](MT.md) for the full format specification.

## Applications

`.M` files require `fx mission unpack` â†’ edit â†’ `fx mission pack`. `.MT` files
are plain ASCII and can be opened directly.

- **VS Code** — free, cross-platform; multi-file search useful for tracking object names and map references across missions
- **Notepad++** — free, Windows; lightweight for quick briefing text edits
