---
format: INF
name: Aircraft Tech Sheet
extensions: [".INF"]
category: 3d
endianness: none
spec:
  status: partial
  gaps:
    - kind: re-static
      issue: 54
      note: "directive set may be incomplete; footer parsing by engine unconfirmed"
codec:
  direction: round-trip
  byte_identical: true
  lib: [lib/src/inf.cpp]
  commands: [inf]
  tests: [tests/test_inf.cpp]
  fuzz: []
  gui: [gui/src/editors/inf_editor.cpp]
  fixtures:
    synthetic: true
    real_manifest: false
related: [BRF, PIC, LIB]
---

# INF — Aircraft Tech Sheet (.INF)

`.INF` files contain the technical information sheet for an aircraft, displayed
in-game on the aircraft selection screen and in the mission planner. One `.INF`
per aircraft; not all aircraft have one. FA_3.LIB (Disc 2) carries 269 of them,
DCL-compressed (flags=4).

## Tools

### fx

```
fx inf dump <file.INF>     # parsed directive/body listing
```

Extract via `fx lib unpack` first. Edit in any plain text editor, re-pack into
a custom LIB, and configure FA to load it.

### fx-gui

Opening an `.INF` record shows a **Styled** tab — each directive section
rendered with its in-game alignment and title/body weight, editable per
section (text, alignment, title/body style, insert/delete) — and a **Source**
tab with the raw dot-command text. Edited sections are recomposed through
`fx::inf_rebuild_section`; untouched sections keep their exact source bytes.
See [gui.md](../../gui.md#technical-info-editing-inf).

## File Layout

Plain text; no binary fields.

**Custom dot-command markup** — plain text, not RTF. Lines beginning with `.`
are formatting directives; all other lines are body text.

```
.body .right
Jane's All The World's Aircraft

.title .center
A-10 THUNDERBOLT II
.body .left

TITLE:
FAIRCHILD REPUBLIC THUNDERBOLT II
...
```

### Known Directives

| Directive | Effect |
|-----------|--------|
| `.body .right` | Body text, right-aligned (used for the "Jane's" header) |
| `.body .left` | Body text, left-aligned (main content) |
| `.title .center` | Title text, centred (aircraft name) |

The renderer is the in-game text display, not a standard RTF or HTML control.
Only the directives listed above have been observed.

### Content Structure

All observed `.INF` files follow the same layout (sourced from *Jane's All The
World's Aircraft*):

1. **Header**: "Jane's All The World's Aircraft" right-aligned
2. **Title**: Aircraft designation, centred
3. **Body sections**: TITLE, TYPE, PROGRAMME, DESIGN FEATURES, FLYING CONTROLS,
   STRUCTURE, LANDING GEAR, POWER PLANT, ACCOMMODATION, ARMAMENT, DIMENSIONS:
   EXTERNAL, WEIGHTS AND LOADINGS, PERFORMANCE
4. **Structured footer** (machine-readable performance data):
   ```
   LENGTH (m): 16.26
   HEIGHT (m): 4.47
   WINGSPAN (m): 17.53
   MAX T.O. WEIGHT (kg): 21,500
   MAX WING LOAD (kg/m/2): 449.88
   T.O. RUN (m): 1372
   LANDING RUN (m): 762
   MAX RATE CLIMB (m/min): 1828
   ```
   These key-value pairs are likely parsed by the engine to populate the stat
   display.

## Round-Trip Notes

`inf_parse` keeps each section's exact source bytes (`InfSection::raw`);
`inf_serialize` concatenates them, so parse → serialize is byte-identical by
construction — line endings, blank-line runs, stat-footer separators (both
`: ` and space formats), and unterminated or DOS-EOF tails all survive.
`tests/test_inf.cpp` asserts it on synthetic samples; verified across all
**269/269** `.INF` entries in FA_3.LIB (Disc 2). The stats map is a derived
view — extraction no longer removes footer lines from section text.

## Open Questions

### 1. Directive set and footer consumption

Only three directives (`.body .right`, `.body .left`, `.title .center`) have
been observed across the corpus, and the structured footer is *inferred* to be
engine-parsed for the stat display; the renderer and its directive dispatch in
FA.EXE have not been traced.

*Status: open — re-static (#54)*

## Related

**Formats:** [BRF](BRF.md) — `.PT` aircraft flight model records paired with
each `.INF`; [PIC](PIC.md) — aircraft skin textures also in FA_3.LIB;
[LIB](LIB.md) — the container (FA_3.LIB, DCL-compressed entries).
