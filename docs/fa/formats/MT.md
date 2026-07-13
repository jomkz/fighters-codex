---
format: MT
name: Mission Briefing Text
extensions: [".MT"]
category: mission
endianness: none
spec:
  status: complete
codec:
  direction: round-trip
  byte_identical: true
  lib: [lib/src/mt.cpp, lib/src/txt.cpp]
  commands: [mt]
  tests: [tests/test_mt.cpp]
  fuzz: [fuzz/fuzz_mt.cpp]
  fixtures:
    synthetic: true
    real_manifest: true
    real_install: false
related: [M, TXT]
---

# MT — Mission Briefing Text (.MT)

FA_2.LIB contains 363 `.MT` files — roughly one per mission. Each stores the
full text content for the pre-mission briefing and post-mission debrief
screens. Format is **plain ASCII text** using a shared directive/markup engine
with `.TXT` files.

## Tools

### fx

```
fx mt info <file.MT>       # mission id/title/type + section count + round-trip
```

Parsing rides the shared directive engine in `lib/src/txt.cpp` (the same
line-preserving parser as [TXT](TXT.md)); `lib/src/mt.cpp` adds the
section-1 header semantics. All 363 `.MT` files in FA_2.LIB round-trip
byte-identically.

## File Layout

Plain ASCII text, CRLF line endings. Directives begin with `.` and may be
chained on one line, separated by spaces. Plain text between directives is
rendered in the current active style. Directives may also appear inline within
a content line (e.g. `.header TITLE .body` renders "TITLE" in header style
then switches to body).

### Directives

| Directive | Description |
|-----------|-------------|
| `.section <N>` | Begin numbered section (see Section Semantics below) |
| `.header` | Switch to header render style |
| `.body` | Switch to body render style |
| `.center` | Center-align subsequent text |
| `.left` | Left-align subsequent text |
| `.underline` | Enable underline |
| `..underline` | Disable underline (`..` prefix deactivates the named directive) |
| `.page` | Page break within a section — inserts a new screen without starting a new section |

Directives apply until overridden. Alignment and style are independent —
`.center .underline .header` sets all three simultaneously.

### Section Semantics

Sections 1–5 are observed across all 363 files:

| Section | Purpose |
|---------|---------|
| 1 | Mission identifier — plain text line (format: `--<ID>  (<filename>)`) followed by title and mission type |
| 2 | Pre-mission briefing — location, date/time, objectives, threat data |
| 3 | Debrief (primary outcome — typically success for single-player, Blue success for multiplayer) |
| 4 | Debrief (secondary outcome — failure, or Red success for multiplayer) |
| 5 | Debrief (draw / objectives incomplete) |

Most single-player missions use sections 1–4. Multiplayer and some campaign
missions use all 5.

### Section 1 ID Line

The first content line of section 1 follows the format:

```
--<MISSIONID>  (<filename>)
```

e.g. `--AB01  (bextra01)`. The `--` prefix is the engine's cue (inferred) to
parse this line as the mission ID rather than display text.

### Example — BEXTRA01.MT (complete section 2)

```
.section 2
.center .underline .header
WEAPONS FREE
..underline .left .body

TARTU AIRBASE
DATE :  February 22
LOCAL  TIME :  1200
WEATHER :  Cloudy

.header .underline
MISSION OBJECTIVE
..underline .body
Destroy all guerrilla structures and vehicles in the area.

.header .underline
THREAT SUPPRESSION DATA
..underline .body
GROUND  OPPOSITION :  Shoulder-launched SAMs
AIR  OPPOSITION :  Possible U.N Rafales, Mirage 2000s
```

## Related

**Formats:** [M](M.md) — `.M` mission files that reference these briefing
texts by filename; [TXT](TXT.md) — uses the same directive engine; adds
`.button` and `.picture` for UI screens.
