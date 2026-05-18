# Technical Info -- Aircraft Tech Sheet (.INF)

`.INF` files contain the technical information sheet for an aircraft, displayed in-game on the aircraft selection screen and in the mission planner. One `.INF` per aircraft; not all aircraft have one.

## Location

| LIB | Count |
|-----|-------|
| FA_3.LIB | 269 |

## Format

**Custom dot-command markup** — plain text, not RTF. Lines beginning with `.` are formatting directives; all other lines are body text.

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

The renderer is the in-game text display, not a standard RTF or HTML control. Only the directives listed above have been observed.

## Content Structure

All observed `.INF` files follow the same layout (sourced from *Jane's All The World's Aircraft*):

1. **Header**: "Jane's All The World's Aircraft" right-aligned
2. **Title**: Aircraft designation, centred
3. **Body sections**: TITLE, TYPE, PROGRAMME, DESIGN FEATURES, FLYING CONTROLS, STRUCTURE, LANDING GEAR, POWER PLANT, ACCOMMODATION, ARMAMENT, DIMENSIONS: EXTERNAL, WEIGHTS AND LOADINGS, PERFORMANCE
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
   These key-value pairs are likely parsed by the engine to populate the stat display.

## Extraction and Editing

Extract via `ft lib unpack` (DCL compressed in FA_3.LIB, flags=4). Edit in any plain text editor. Re-pack into a custom LIB and configure FA to load it.

## Related

- [BRF.md](BRF.md) — `.PT` aircraft flight model records paired with each `.INF`
- [PIC.md](PIC.md) — aircraft skin textures also in FA_3.LIB
