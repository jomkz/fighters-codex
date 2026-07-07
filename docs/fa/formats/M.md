---
format: M
name: Mission File
extensions: [".M"]
category: mission
endianness: none
spec:
  status: complete
codec:
  direction: round-trip
  byte_identical: true
  lib: [lib/src/mission.cpp]
  commands: [mission]
  tests: [tests/test_mission.cpp]
  fuzz: [fuzz/fuzz_mission.cpp]
  gui: [gui/src/editors/mission_editor.cpp]
  fixtures:
    synthetic: true
    real_manifest: true
related: [MM, MT, BRF]
---

# M — Mission File (.M)

`.M` files define individual missions as plain-text `textFormat` data — one
keyword per line, with `obj` and `waypoint2` blocks closed by a lone `.` (517
files in FA_2.LIB). The related `.MM` theater map format is distinct — see
[MM.md](MM.md).

## Tools

### fx

```
fx mission info   <file.M>              # map name, time, object count
fx mission unpack <file.M> [-o out.txt] # editable text
fx mission pack   <in.txt>  -o out.M    # write back (byte-identical)
```

### Other Tools

`.M` files require `fx mission unpack` → edit → `fx mission pack`. The
companion `.MT` briefing files are plain ASCII and can be opened directly.

- **VS Code** — free, cross-platform; multi-file search useful for tracking object names and map references across missions
- **Notepad++** — free, Windows; lightweight for quick briefing text edits

## File Layout

Plain ASCII text, CRLF line endings, **one keyword per line** — there is no
bracket syntax. The first line is `textFormat`. Top-level keywords stand alone;
`obj` and `waypoint2` open blocks that a lone `.` line closes. Indentation
(tabs) is cosmetic — the structure is defined by the keywords, not the layout.

```
textFormat
brief
map BALTIC.T2
layer BALTIC.LAY 0
time 6 0
wind 270 15
clouds 20
obj
	type F16.PT
	pos 12345 0 67890
	angle 0 0 0
	nationality2 130
	flags $411
	alias -1
	.
	  … more obj blocks …
waypoint2 4
	w_index 0
	w_goal 1
	w_pos2 0 0 743800 16000 929588
	w_speed 920
	w_index 1
	  …
	.
```

### Token format

- `key value…` — one keyword per line, space-separated values (string, decimal
  integer, or `$`-prefixed hex).
- `obj` … `.` — an object block; the lone `.` closes it.
- `waypoint2 N` … `.` — a waypoint block of **N** waypoints; the lone `.` closes it.
- `sides4` opens the nationality table (indented `$XX` values follow).

### Top-level keys

| Key | Value | Description |
|-----|-------|-------------|
| `textFormat` | (none) | magic first line |
| `brief` / `briefmap` / `armplane` / `selectplane` | (none) | screen flags |
| `map` | file | terrain `.T2` reference |
| `layer` | file index | `.LAY` reference |
| `clouds` | percent | cloud cover 0–100 |
| `wind` | dir speed | wind conditions |
| `time` | hour min | start time of day |
| `sides4` | (block) | nationality table |
| `obj` | (block) | a placed object |
| `waypoint2` | count (block) | a waypoint list |

### Object block (`obj` … `.`)

| Key | Value |
|-----|-------|
| `type` | object type name (references `.OT`/`.NT`/`.PT` files) |
| `pos` | x y z (world units) |
| `angle` | pitch bank roll (degrees) |
| `nationality2` | integer country code |
| `flags` | `$hex` behaviour flags |
| `speed` | initial speed |
| `alias` | signed id; targeted by a waypoint's `w_preferredTargetId2` |
| `skill` | AI skill level |
| `react` | reaction triple (`$hex $hex $hex`) |
| `searchDist` | detection range |
| `name` | optional label |

The field list varies by object type; unknown fields are preserved verbatim.
`mission_parse_objects` ([api.md](../../api.md) § mission.h) promotes
`type`/`pos`/`angle` to typed members and keeps every other field in
`MissionObj::fields`.

### Waypoint block (`waypoint2 N` … `.`)

**N** waypoints, each opened by `w_index` and carrying `w_pos2` (position),
`w_goal`, `w_next`, `w_flags`, `w_speed`, `w_react`, `w_searchDist`,
`w_preferredTargetId2` (target link), and `w_name`. These blocks are emitted
**after all `obj` blocks** and there are always fewer of them than objects, so
which object a block belongs to is *not* encoded adjacently — the engine's
assignment rule is not yet recovered (re-gameplay, #29). `mission_parse_objects`
therefore returns the blocks as a parallel `waypoint_blocks` list rather than
nesting them in objects.

### Preferred target system

Waypoints can designate a specific object as their attack target using
`w_preferredTargetId2`. The value is the object's `alias` field encoded as a
**negated unsigned 16-bit hex** value:

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

The pattern continues linearly; alias −N = `$(10000 − N)` in hex. Up to 288
preferred target slots are supported (`$ffff` through `$fee0`). Does not apply
to map objects or the player object.

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

- Parse → serialize produces byte-identical files for all 517 `.M` files in
  FA_2.LIB; `tests/test_mission.cpp` asserts byte preservation.
- `mission_roundtrip` is a verbatim copy (CRLF-normalized), so it preserves the
  files' tab indentation regardless of whether the parser needs it.
- `mission_parse_objects` extracts the full object + waypoint-block lists; the
  object count matches `mission_parse_info` for all 592 stock missions.

## Related

**Formats:** [MM](MM.md) — the theater map format sharing the mission codec;
[MT](MT.md) — plain ASCII briefing/debrief companion to each `.M` file;
[BRF](BRF.md) — the `.OT`/`.PT` type definitions referenced by `type` fields.
