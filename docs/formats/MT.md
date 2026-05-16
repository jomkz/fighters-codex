# Mission Briefing Text (.MT)

FA_2.LIB contains 363 `.MT` files — roughly one per mission. Each stores the full text content for the pre-mission briefing and post-mission debrief screens. Format is **plain ASCII text** with a section/markup syntax shared with `.TXT` files.

## Format

Plain ASCII text, CRLF line endings. Content is divided into numbered sections using `.section N`. Within each section, layout directives (`.header`, `.body`, `.center`, `.left`, `.underline`) control text rendering style. Directives can be chained on one line separated by spaces; `..` resets the preceding directive(s).

### Directives

| Directive | Description |
|-----------|-------------|
| `.section <N>` | Begin numbered section (1 = pre-mission, 2 = briefing, 3 = debrief success, 4 = debrief failure) |
| `.header` | Render following text in header style |
| `.body` | Render following text in body style |
| `.center` | Center-align following text |
| `.left` | Left-align following text |
| `.underline` | Underline following text |
| `..` | Reset preceding directive (e.g. `..underline` turns off underline) |

Non-directive lines are plain text rendered in the current style.

### Example — BEXTRA01.MT (excerpt)

```
.section 1
--AB01  (bextra01)
WEAPONS  FREE  (The Baltics)
Single Player Mission  (ATFGOLD)
.section 2
.center .underline .header
WEAPONS FREE
..underline .left .body

TARTU AIRBASE
DATE :  February 22
...
.section 3
.center .underline .header
DEBRIEF
..underline .left .body
...RESOLUTION :  Success
.section 4
...RESOLUTION :  Failure
```

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 363 |

## TODO — Deep Dive

- Confirm the full set of directive keywords by surveying all 363 files
- Clarify section numbering beyond 4 (are there 5+ section files?)
- Document the `--` prefix convention on section 1 mission ID lines

## Related

- [MISSION.md](MISSION.md) — `.M` mission files that reference these briefing texts
- [TXT.md](TXT.md) — campaign description text using the same section/directive syntax
