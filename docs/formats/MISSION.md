# M / MM -- Mission and Map Files (.M / .MM)

> Format documented by the [OpenFA project](https://gitlab.com/openfa/openfa).
> Primary reference: `crates/asset/mmm/`. Our implementation is in `lib/src/mission.cpp`.

---

## Overview

`.M` files define individual missions; `.MM` files define map/theater data. Both use
the same `[textFormat]` container with `[key value]`-style bracketed tokens.

---

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

### Top-level keys (.M)

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
| −1 | `$ffff` |
| −2 | `$fffe` |
| −255 | `$ff01` |
| −256 | `$ff00` |
| −257 | `$feff` |
| −288 | `$fee0` |

The pattern continues linearly; alias −N = `$(10000 − N)` in hex. Up to 288 preferred
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

---

### MM terrain tile lines (`tmap`)

`.MM` files may contain `tmap` lines at the bottom of the file that place terrain tiles
on the map. Each line has four fields:

```
tmap <x> <y> <tile_id> <orientation>
```

| Field | Description |
|-------|-------------|
| `x` | Left-to-right position; increments of 4 (4, 8, 12, …) |
| `y` | Bottom-to-top row; increments of 4 (0, 4, 8, …) |
| `tile_id` | Tile index as shown in the FA Toolkit for that map |
| `orientation` | 0=North, 1=East, 2=South, 3=West |

Positions can be skipped — gaps represent open water. The terrain grid cannot exceed
256×256; tiles placed beyond this boundary appear only when the player is close (no
ground renders underneath at distance).

Each map has its own tile set; tile IDs from one map are not portable to another.
Edit `tmap` lines in a text editor after creating the `.MM` in the FA Toolkit.

Example:
```
tmap 4  0 25 3
tmap 8  0 33 2
tmap 12 0 44 1
tmap 4  4 22 2
tmap 128 0 35 1    ; gap between 12 and 128 = open water
```

---

## Round-Trip Notes

- Parse → serialize produces byte-identical files for all 592 M/MM files in FA_2.LIB.
- Tab-indented blocks must use a real tab character, not spaces.
- The `\t]` terminator is literally `<TAB>]` (not backslash-t).

---

## ft commands

```
ft mission info   <file.M|.MM>              # map name, time, object count
ft mission unpack <file.M|.MM> [-o out.txt] # editable text
ft mission pack   <in.txt>     -o out.M     # write back

ft mm     info   <file.MM>
ft mm     unpack <file.MM> [-o out.mm.txt]
ft mm     pack   <in.txt>  -o out.MM
```

---

## .MT -- Mission Text / Briefing

`.MT` files are plain ASCII companions to `.M` files. They contain the mission briefing
and debrief text displayed in-game and are editable with any text editor.

### Structure

```
.section 1
*MISSION_INTERNAL_NAME
Mission Title Line
Author / Source
.section 2
.center .underline .header
SECTION HEADING
..underline .left .body
Body text paragraph...

.section 3
.center .underline .header
DEBRIEF
..underline .left .body
Success debrief text...
.section 4
.center .underline .header
DEBRIEF
..underline .left .body
Failure debrief text...
```

### Sections

| Section | Content |
|---------|---------|
| 1 | Title block: internal name (`*NAME`), display title, author |
| 2 | Pre-mission briefing (situation, objectives, threat data) |
| 3 | Debrief — mission success |
| 4 | Debrief — mission failure |

### Markup directives

Directives appear on their own line and affect the text that follows them on the
same line (space-separated) or on subsequent lines until reset.

| Directive | Effect |
|-----------|--------|
| `.section N` | Begin a new section (1–4) |
| `.center` | Center-align |
| `.left` | Left-align |
| `.header` | Header style (larger / bold) |
| `.body` | Body style (normal) |
| `.underline` | Underline the following text |
| `..underline` | Reset underline (double-dot prefix resets a directive) |

Multiple directives may appear on the same line: `.center .underline .header`.
