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
    real_install: true
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
section-1 header semantics.

`tests/test_mt.cpp` runs a census over **all 363 `.MT` files** in a real install (under
`FX_FA_ROOT`): each round-trips byte-identically, every directive it uses is one the engine
knows, its sections are the documented 4 or 5 (346 use 4, 17 use 5), and the 361 that carry
an identifier line decode it.

The round-trip is worth exactly what it costs: `txt_write` replays the file's own lines, so
**all 363 would round-trip byte-identically whether or not the decoder understood a thing** —
and for years, while the codec was dropping the mission ID on 263 of them, they did.

## File Layout

Plain ASCII text, CRLF line endings. Directives begin with `.` and may be
chained on one line, separated by spaces. Plain text between directives is
rendered in the current active style. Directives may also appear inline within
a content line (e.g. `.header TITLE .body` renders "TITLE" in header style
then switches to body).

### Directives

The **complete vocabulary**, read out of the executable — the text interpreter (0x47E1B0)
compares each token against exactly these, lowercased. A token it does not recognise is
**rendered as text**, which is why a briefing can write "get the `.ell` out" as prose
(`~K30.MT`) without it meaning anything.

| Directive | Off form | Description |
|-----------|----------|-------------|
| `.section <N>` | — | Begin numbered section (see Section Semantics below) |
| `.page` | — | Page break within a section — a new screen, not a new section |
| `.title` | — | Title render style |
| `.header` | — | Header render style |
| `.body` | — | Body render style |
| `.italic` | `..italic` | Italic |
| `.bold` | `..bold` | Bold |
| `.underline` | `..underline` | Underline |
| `.left` `.right` `.center` `.full` | — | Alignment (`.full` = justified) |
| `.indent_left` `.indent_right` | `.indent_off` | Indent |
| `.picture <name>` | — | Inline image |
| `.sound <name>` | — | Play a sound |
| `.music <name>` | `.music_off` | Play music |
| `.button` | `..button` | Delimit a button (UI templates — see [TXT](TXT.md)) |
| `.dbutton` | `..dbutton` | Delimit a disabled button |

Directives apply until overridden. Alignment and style are independent —
`.center .underline .header` sets all three simultaneously. The `..` prefix turns the named
directive off; it is not a general "close" marker (`.section` and `.page` have no off form).

`tests/test_txt.cpp` asserts that every directive `fx` reports, across all 363 `.MT` and all
8 `.TXT` files in a real install, is one of the above.

### Section Semantics

Sections 1–5 are observed across all 363 files:

| Section | Purpose |
|---------|---------|
| 1 | Mission identifier — a plain text line (`<ID>  (<annotation>)`, see below) followed by title and mission type |
| 2 | Pre-mission briefing — location, date/time, objectives, threat data |
| 3 | Debrief (primary outcome — typically success for single-player, Blue success for multiplayer) |
| 4 | Debrief (secondary outcome — failure, or Red success for multiplayer) |
| 5 | Debrief (draw / objectives incomplete) |

Most single-player missions use sections 1–4. Multiplayer and some campaign
missions use all 5.

### Section 1 ID Line

The first content line of section 1 is shaped:

```
<MISSIONID>  (<annotation>)
```

**Any leading dashes are decoration.** Of the 361 shipped `.MT` files that carry an
identifier line, **244 write a bare `AB01`**, 100 write `--AB01`, and `~FANOTH.MT` writes
`-RB12` with a single dash. What identifies the line is its *shape* — one unspaced token,
then a parenthesised note — not a prefix.

> An earlier version of this page said the `--` was "the engine's cue **(inferred)** to parse
> this line as the mission ID". It is not, and the inference was never checked: the engine
> **never parses this line at all**. It *renders* section 1 through the text interpreter
> (0x47E1B0) like any other text — there is no mission-ID parser in the binary. The codec
> believed the doc, required the `--`, and so lost the ID on **263 of the 363 files**,
> shifting every other field up by one: the title came out as the ID line and the mission
> type as the title. Every one of them still round-tripped byte-identically. See
> [#491](https://github.com/jomkz/fighters-codex/issues/491).

The parenthesised note is an **annotation, not a key**. It is usually the file's own name,
but the designers were not consistent: `~FANOTH.MT` writes a theater (`Panama`) and
`EXAMPLE.MT` names a *different* file (`extra01`). Do not resolve anything with it.

`QUICK.MT` and `QUICKMP.MT` have no identifier line — their section 1 opens with a plain
title (`QUICK MISSION`). A line with no parenthesised note is a title, not an ID.

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
