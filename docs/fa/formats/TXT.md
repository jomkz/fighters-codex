---
format: TXT
name: In-Game Text and UI Layout
extensions: [".TXT"]
category: text
endianness: none
spec:
  status: complete
codec:
  direction: round-trip
  byte_identical: true
  lib: [lib/src/txt.cpp]
  commands: [txt]
  tests: [tests/test_txt.cpp]
  fuzz: [fuzz/fuzz_txt.cpp]
  gui: [gui/src/editors/txt_editor.cpp]
  fixtures:
    synthetic: true
    real_manifest: true
    real_install: true
related: [CAM, MNU, MT, P]
---

# TXT — In-Game Text and UI Layout (.TXT)

FA_2.LIB contains 8 `.TXT` files. The extension covers three distinct uses of
the same directive engine: campaign selection descriptions, interactive UI
screen templates, and plain-text content (credits). All are **plain ASCII,
CRLF**.

## Tools

### fx

```
fx txt info <file.TXT>     # kind + directive structure + round-trip check
```

The parser is line-preserving — `txt_read`/`txt_write` round-trip any input
byte-identically while exposing the directive structure — and is written for
reuse by the `.MT` briefing codec (#108), which shares the directive engine.

## File Layout

Plain text; no binary fields.

### Directive Engine

`.TXT` and `.MT` share **one** directive engine — the text interpreter at **0x47E1B0**. There
are no "UI-specific additions": `.button` and `.picture` are in the same vocabulary as
`.header`, and a briefing could use them. What differs is which files use which.

The vocabulary is a fact read out of the executable; the interpreter compares each token
against exactly this set, lowercased, and **renders anything else as text**. The complete
table is in [MT.md](MT.md#directives). The UI templates lean on:

| Directive | Description |
|-----------|-------------|
| `.button <label> ..button` | Interactive button; the label is between the open/close tags |
| `.dbutton <label> ..dbutton` | The same, disabled |
| `.picture <name>` | Image — the engine renders the current context image (e.g. nose art) |
| `.page` | Page break — advance to the next screen without starting a new section |

Because an unknown `.`-token is just text, a parser must **match the vocabulary**, not
assume any `.`-token is a directive: a real briefing writes "get the `.ell` out" as prose
(`~K30.MT`), and `fx` used to report that as a directive (#491).

### Campaign Description (6 files)

Two-section structure. Section 2 contains only `END` — a sentinel the engine
uses to detect end-of-file.

```
.section 1
.header
<Campaign title>
.body
<One-paragraph description>
.section 2
END
```

**Example — BALTIC.TXT:**

```
.section 1
.header
The Baltics 2009
.body
Fly missions over Estonia, Latvia, and Lithuania, defending them from aggression.
.section 2
END
```

### UI Layout Template (SHWPILOT.TXT)

Defines the Pilot Service Record screen. Uses `.page` to separate tabs,
`.button` for navigation controls, and `.picture` as the image render target
for nose/tail art. Button content between `.button` and `..button` is the
button label; an empty `.button  ..button` pair defines an editable input
field.

```
.center
.header PILOT SERVICE RECORD .body

.left

NAME: .button  ..button

CALLSIGN: .button  ..button
.page
.center
.header NOSE ART .body

.picture

.button Previous Picture ..button    .button Next Picture ..button
```

### Plain Text (CREDITS.TXT)

No directives. Raw ASCII credits block rendered verbatim.

## File Inventory

| File | Size | Type |
|------|------|------|
| BALTIC.TXT | 147 B | Campaign description |
| EGYPT.TXT | 136 B | Campaign description |
| KURILE.TXT | 171 B | Campaign description |
| UKRAINE.TXT | 151 B | Campaign description |
| VIETNAM.TXT | 154 B | Campaign description |
| VLAD.TXT | 149 B | Campaign description |
| SHWPILOT.TXT | 947 B | UI layout template (Pilot Service Record screen) |
| CREDITS.TXT | 1036 B | Plain text (no directives) |

All 8 live in FA_2.LIB.

## Related

**Formats:** [CAM](CAM.md) — campaign definitions paired with campaign
description `.TXT` files; [MNU](MNU.md) — the campaign selection menu that
renders these descriptions; [MT](MT.md) — mission briefing text through the same
interpreter, and the complete directive table; [P](P.md) — pilot save data
displayed by SHWPILOT.TXT.
