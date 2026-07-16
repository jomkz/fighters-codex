---
format: P
name: Pilot Save
extensions: [".P"]
category: system
endianness: little
spec:
  status: partial
  gaps:
    - kind: re-gameplay
      issue: 29
      note: "0xB0-0xC1 (18 B) — rank/score tier fields"
    - kind: re-gameplay
      issue: 29
      note: "0xCF-0x5AE (1,344 B) — mission log content"
    - kind: re-gameplay
      issue: 29
      note: "0x2018-0x20B7 (160 B) — between kill tallies and accuracy"
    - kind: re-gameplay
      issue: 29
      note: "0x21F8-0x25DF (~1,000 B) — fort/campaign-phase stats"
codec:
  direction: round-trip
  byte_identical: true
  lib: [lib/src/plt.cpp]
  commands: [plt]
  tests: [tests/test_plt.cpp]
  fuzz: [fuzz/fuzz_plt.cpp]
  gui: [gui/src/editors/plt_editor.cpp]
  fixtures:
    synthetic: true
    real_manifest: false
    real_install: true
related: [BRF, M, CAM]
---

# P — Pilot Save (.P)

`PLTnnn.P` files (e.g. `PLT441.P`, `PLT628.P`) are **binary pilot save files**
written by the FA engine. They store the persistent state for each pilot slot.
Unlike all other FA data, pilot files are stored **directly in the FA install
directory**, not inside any `.LIB` archive. The filename number does not imply
a slot sequence — it appears to be a randomly assigned identifier. All
observed files are exactly **9,696 bytes**.

## Tools

### fx

```
fx plt info  PLT441.P        # identity block: name, rank, campaign, ordnance
fx plt dump  PLT441.P        # full stats block: missions, kills, weapon accuracy
```

The identity block (`0x01`–`0xAF`) is fully mapped and editable via the fxs
PLT editor. The stats counters (`0x1F80`–`0x21F7`) are confirmed from RE and
displayed by `fx plt dump` and the fxs stats pane. The four gap regions
remain unmapped (see Open Questions).

The `fx_lib` write API (`plt_read` → edit → `plt_write`, [api.md](../../api.md)
§ plt.h) serializes a pilot file back to bytes, overlaying only the mapped
fields and passing the unmapped regions through verbatim — a `plt_read` →
`plt_write` round-trip is byte-identical (see § Round-Trip Notes). Editing is
done in `fx_lib`, not from the CLI: there is no `fx plt` write verb, so the
tool never takes an output path for a save file.

### Other Tools

- **FATK** — free (abandonware, 1998); original GUI tool with full pilot editing support; requires a compatibility layer on 64-bit Windows
- **HxD** — free, Windows; use with the field table below for manual patching
- **010 Editor** `$` — paid; binary templates will allow a fully labelled struct view once all gaps are mapped

## File Layout

All multi-byte integers are little-endian.

`_campaignPilot` global base VA = `0x004f8bb8` (from FA.SMS). File size
0x25E0 = 9,696 bytes. All offsets below are confirmed from decompile
(`DumpAllFunctions.txt`) or computed from base VA.

### Identity block (0x00–0xAF) — confirmed

| Offset | Size | Type | Field |
|--------|------|------|-------|
| `0x00` | 1 | u8 | Type / version tag — observed: `0x0F` |
| `0x01` | 63 | char[] | Pilot name, null-padded |
| `0x40` | 32 | char[] | Callsign, null-padded |
| `0x61` | 13 | char[] | Callsign voice file (e.g. `^ACID.5K`), null-padded |
| `0x6E` | 13 | char[] | Nose art ID (e.g. `NOSE01`), null-padded |
| `0x7B` | 13 | char[] | Left wing decal ID (e.g. `LEFT03`), null-padded |
| `0x88` | 13 | char[] | Right wing decal ID (e.g. `RIGHT03`), null-padded |
| `0x95` | 13 | char[] | Pilot portrait ID (e.g. `PILOT02`), null-padded |
| `0xA2` | 14 | char[] | Rank string (e.g. `2nd Lieutenant`), null-padded |

### Text and display region (0xB0–0xDAD) — partially mapped

Four anchor fields confirmed from `FUN_004674f0` (pilot card display, VA
0x4674f0); gaps between them remain unmapped.

| Offset | Size | Type | Field |
|--------|------|------|-------|
| `0xB0` | 18 | ? | **Unknown** (see Open Questions) |
| `0xC2` | ~13 | char[] | Secondary identity string — printed on pilot card after rank (squadron, unit, or location); exact length unknown |
| `0xCF` | 1344 | ? | **Unknown** — mission log region (see Open Questions) |
| `0x5AF` | var | char[][] | Mission log — up to 10 null-terminated entries read sequentially; each up to 3 lines; likely mission history |
| `0xD7F` | 13 | char[] | Campaign `.CAM` filename (e.g. `EGYPT.CAM`) — confirmed: `DAT_004f9937 = _campaignPilot + 0xD7F`, written by campaign init |
| `0xD8C` | 32 | char[] | Campaign display name (e.g. `Egypt 1998`) — confirmed: `DAT_004f9944 = _campaignPilot + 0xD8C`, written by campaign init |
| `0xDAC` | 2 | u16 | Pilot status enum — confirmed: `DAT_004f9964 = _campaignPilot + 0xDAC`; `0`=Available, `1`=On mission, `2`=MIA, `3`=KIA, `4`=Retired From Active Duty |

### Campaign data strings (0xDAE–0x1C5F) — partially mapped

Variable-length null-terminated strings packed sequentially from 0xDAE:
- Assigned aircraft `.PT` reference (e.g. `F22.PT`)
- Available aircraft pool (`.PT` references)
- Sensor/ECM loadout (`.SEE`, `.ECM` references)
- Other campaign-specific strings

### Aircraft pool (0x0DB0–0x1C5F) — confirmed

**20 slots × 0xBC (188) bytes**, immediately before the store table and ending exactly where
it begins — the two tables tile the campaign block with nothing between them.
`_AddCampaignPlane` (`0x480C90`) walks `0x4F9968` in 0xBC steps while the `s16` at `+0x0E`
is non-zero, and `_CampaignPlanesLeft` counts the same field; `_campaignPilot` is
`0x4F8BB8`, so `0x4F9968` is file offset `0x0DB0`.

| Field | Offset within slot | Size | Type |
|-------|--------------------|------|------|
| Aircraft type filename (`.PT`) | `+0x00` | 14 | char[] (null-padded) |
| In use | `+0x0E` | 2 | s16 — **zero means the slot is empty**; this, not the name, is what the engine tests |
| *(unmapped)* | `+0x10` | 30 | per-airframe campaign state; not yet mapped |
| Repair state | `+0x2E` | 2 | s16 — percent under repair: `_CallCampaignProc` cmd 3 writes `dam×100/damMax + PT+0x1B4`, clamped to 100, after a home landing (#492) |
| Damage array copy | `+0x30` | 139 | the plane's live `_dam` per-system damage block, banked on a home landing and restored on the next sortie; a bail (`!AlmostHome && !AtFriendlyAP`) zeroes the whole slot instead (#492) |

A pilot owns one slot per airframe, so the same type appears more than once (`PLT937.P`
holds three `F22.PT`, three `F117.PT`, one `B2.PT`, four `F31E.PT`).

### Store inventory (0x1C60–0x1F7F) — confirmed

**50 entries × 16 bytes** = 800 bytes. `DAT_004fa818 = _campaignPilot + 0x1C60`.
Not only weapons: drop tanks (`.GAS`), sensor pods (`.SEE`) and ECM pods (`.ECM`) are
drawn from the same table.

| Field | Offset within entry | Size | Type |
|-------|---------------------|------|------|
| Store type filename (`.JT`, `.GAS`, `.SEE`, `.ECM`) | `+0x00` | 14 | char[] (null-padded) |
| Quantity | `+0x0E` | 2 | s16 — `0x7FFF` = unlimited (`_AddCampaignStore` returns early on it) |

**A free slot is one with no name**, not one with a quantity of `-1`: this spec used to say
"`-1` if slot unused", but every campaign save carries its guns (`GSH301.JT`, `M61.JT`,
`GAU12.JT`, …) as *named* entries with a quantity of `-1`, while the genuinely unused slots
at the end of the table are blank. Whatever `-1` means to the engine, it does not mean
"empty" ([#491](https://github.com/jomkz/fighters-codex/issues/491)).

> `lib/src/plt.cpp` used to recover both tables by scanning the campaign block for
> printable strings, capped at `0x0F00` — 3,840 bytes short of the store table — and read
> the quantity as a single byte after the name. A fully-equipped campaign pilot therefore
> decoded to **no aircraft, no ordnance and no sensors at all**. The round-trip suite could
> not see it: `fx plt` never *writes* these fields, so an unedited repack stayed
> byte-identical. `PLT937.P` holds 11 airframes and 35 stores; `fx plt info` reported none.

Managed by `_AddCampaignStore` (0x480E10): searches by name,
increments/decrements quantity, or allocates a free slot.

### Stats counters (0x1F80–0x21F7) — confirmed

All fields confirmed via `FUN_00485380` (0x485380, end-of-mission stats flush)
and related functions. `_campaignPilot` base `0x4f8bb8` + listed offset = VA
of each field.

**Mission and loss counters (0x1F80–0x1FAF):**

| Offset | VA | Size | Field |
|--------|----|------|-------|
| `0x1F80` | `DAT_004fab38` | u32 | Missions flown (total) |
| `0x1F84` | `DAT_004fab3c` | u32 | Wingman missions |
| `0x1F88` | `DAT_004fab40` | u32 | Missions failed — copied to `_campaignFailures` (0x54e418) before campaign proc |
| `0x1F8C` | `DAT_004fab44` | u32 | Total shots fired — accumulated from per-mission `DAT_0054ddc4` |
| `0x1F90` | `DAT_004fab48` | u32 | Ejections / bail-outs |
| `0x1F94` | `DAT_004fab4c` | u32 | Wingman KIA |
| `0x1F98` | `DAT_004fab50` | u32 | Player aircraft damage % accumulated |
| `0x1F9C` | `DAT_004fab54` | u32 | Wingman aircraft damage % accumulated |
| `0x1FA0` | `DAT_004fab58` | u32 | Player landing count |
| `0x1FA4` | `DAT_004fab5c` | u32 | Wingman landing count |
| `0x1FA8` | `DAT_004fab60` | u32 | Player landing quality score |
| `0x1FAC` | `DAT_004fab64` | u32 | Wingman landing quality score |

**Kill tallies by target class (0x1FB0–0x2017):**

13 kill categories; each has **player u32** then **wingman u32** (8 bytes per
category). Category dispatch from `_KillStats_12` (0x485820) based on
`obj_class` word bits:

| Offset | VA (player) | Category | `obj_class` bits |
|--------|-------------|----------|------------------|
| `0x1FB0` | `DAT_004fab68` | Air — aircraft / fighters | `0x8000` set |
| `0x1FB8` | `DAT_004fab70` | Air — type B (fighters subtype) | `0x4000` set |
| `0x1FC0` | `DAT_004fab78` | Aircraft destroyed by crash or BA weapon | obj byte 0 = `0x04` with OBJ_TYPE+0xba bit 3 |
| `0x1FC8` | `DAT_004fab80` | Naval vessels | `0x2000` set |
| `0x1FD0` | `DAT_004fab88` | SAM launchers | `0x1000` set |
| `0x1FD8` | `DAT_004fab90` | AAA guns | `0x800` set |
| `0x1FE0` | `DAT_004fab98` | Armor / tanks | `0x400` set |
| `0x1FE8` | `DAT_004faba0` | APCs | `0x200` set |
| `0x1FF0` | `DAT_004faba8` | Vehicles / trucks | `0x100` set |
| `0x1FF8` | `DAT_004fabb0` | Infantry | `0x40` set |
| `0x2000` | `DAT_004fabb8` | Friendly fire | same faction, other conditions |
| `0x2008` | `DAT_004fabc0` | Air — non-`0x8000` (non-fighter aerial) | `0x8000` absent, aerial |
| `0x2010` | `DAT_004fabc8` | Capital ships | naval + hitpoints > 999 |

Wingman slot for each = player VA + 4.

**Weapon accuracy stats (0x20B8–0x21F7) — confirmed:**

8 weapon-type groups; each group = **player slot** (0x14 bytes) + **wingman
slot** (0x14 bytes) = 0x28 bytes per group.

Each slot = 5 × u32: `[damage_total, shots_fired, hits, type3, kills]`.
Dispatched by `FUN_004856f0` (0x4856f0) based on `OBJ_TYPE` flags; accumulated
by `FUN_004854a0` (0x4854a0).

| Offset | VA | Group |
|--------|----|-------|
| `0x20B8` | `DAT_004fac70` | Air-to-air gun (OBJ_TYPE bit `0x10000`) |
| `0x20E0` | `DAT_004fac98` | Air-to-air missile (OBJ_TYPE bit `0x20000`) |
| `0x2108` | `DAT_004facc0` | Ground attack (OBJ_TYPE bits `0x20080`) |
| `0x2130` | `DAT_004face8` | Naval attack (OBJ_TYPE bit `0x10`) |
| `0x2158` | `DAT_004fad10` | Kill by aircraft (shooter = obj byte 0 `0x04`) |
| `0x2180` | `DAT_004fad38` | Kill type B |
| `0x21A8` | `DAT_004fad60` | Kill type C |
| `0x21D0` | `DAT_004fad88` | Kill type D |

Wingman slot for each = player VA + 0x14.

The regions at `0x2018–0x20B7` and `0x21F8–0x25DF` are unmapped — see Open
Questions.

## Engine Notes

Confirmed engine functions (FA.SMS + `DumpAllFunctions.txt`):

| VA | Symbol/Name | Description |
|----|-------------|-------------|
| `0x467180` | `PilotSave(PILOT*, short)` | Write pilot save — takes a `PILOT*` and a short slot index; serialises the full struct to `PLTnnn.P` |
| `0x4674f0` | `PilotBuildPaper` | Pilot dossier ("paper") builder — interleaves `SHWPILOT.TXT` template lines with record fields (`+0xC2`, `+0x5AF`, `+0xD8C`, `+0xDAC`, `+0x1F88`) and renders through the shell format engine (#492) |
| `0x467860` | `PilotPaperAddLine` | Template line copy — copies until a control byte (styled-text terminator) |
| `0x480E10` | `_AddCampaignStore` | Adds or increments an ordnance entry in the ordnance inventory table at `+0x1C60` |
| `0x481320` | `_CampaignSave` | Saves `_campaignPilot` to disk (copies to RM, then `_SaveFile` with full 0x25E0 bytes) |
| `0x484D90` | `_EndOfMissionStats@0` | Computes per-mission damage %, landing, protection, and player/wm state into temp globals |
| `0x485040` | `_EndOfFortMissionStats@0` | Computes fort-related kill/suppression stats into named temp globals |
| `0x485380` | `FUN_00485380` | Flushes all per-mission temp stats into permanent PILOT struct fields at `+0x1F80` onwards |
| `0x4854E0` | `_WpnStats@28` | Per-shot weapon stats accumulator; updates `shots_fired`, `hits`, `damage_total`, `kills` in temp buffer |
| `0x485820` | `_KillStats@12` | Records kill into the correct kill-category slot (13 categories at `+0x1FB0`) based on target's `obj_class` |
| `0x485A40` | `_LandingStats@12` | Accumulates landing count and quality score into temp globals |

`PilotSave` saves the full struct as a single 9,696-byte block via
`_SaveFile`. The stats counters are accumulated by the functions above into
`_campaignPilot` directly.

## Round-Trip Notes

`plt_write` is a **byte-exact passthrough serializer**: it starts from a copy
of the original file bytes (`PltFile::raw`) and overlays only the fixed-offset
mapped fields — the identity block (`0x00`–`0xAF`) and, when present, the stats
counters (`0x1F80`–`0x21F7`). Everything else — the four unmapped gap regions
and the variable-length campaign/ordnance region (`0xB0`–`0x1F7F`) — is copied
through unchanged. A `plt_read` → `plt_write` round-trip is therefore
byte-identical, and Phase 6 (#29) can map the gaps without touching the codec.

Two details keep the round-trip exact even before the gaps are understood:

- **Unedited string fields are left verbatim.** An identity field is only
  rewritten when its value differs from what the original bytes decode to.
  Untouched fields — including any non-zero bytes left *past* a field's null
  terminator (a shorter callsign written over a longer one in-engine) — pass
  through unmodified rather than being re-zero-padded.
- **The variable-length campaign region is never re-encoded.** `PltInfo`'s
  `cam_file` / `aircraft` / `ordnance` views come from a heuristic forward
  scan; they are read-only display state. The bytes themselves pass through, so
  the scan's imprecision cannot perturb the file.

Validation: `plt_repack` (read → write) is byte-identical across all 7 real
pilot files in the reference install (`PLT441/628/937/991/992/993/994.P`,
`FX_FA_ROOT`-gated) and the synthetic fixtures in `tests/test_plt.cpp`, which
also exercise full-width fields, stale terminator bytes, and single-field
edits.

## Open Questions

Static Ghidra analysis (`AnalyzePLT.java`, 46,985-line output) and binary diff
of three fresh pilot saves (PLT441.P, PLT628.P, PLT937.P) are exhausted for
all four gaps — every one needs pilot saves taken after actual gameplay:
complete 5+ standard missions (gaps 2 and 3), a fort-assault mission (gap 4),
and a rank advance (gap 1), then diff with **HxD** (side-by-side compare →
Differ) or **010 Editor** using the field tables above. A binary probe test
(2026-05-21, four `PROBE_GAP*` pilots) confirmed the pilot records screen
reads none of the gap regions.

### 1. Gap 0xB0–0xC1 (18 bytes)

No named DAT_ label or MOV/CMP instruction targeting VA
`0x004f8c68`–`0x004f8c79` found in any function in the game executable. All zeros in all
three fresh saves. Struct context suggests these bytes are written only after
campaign assignment — possibly a score tier index, medal count, or secondary
rank fields.

*Status: open — re-gameplay (#29)*

### 2. Gap 0xCF–0x5AE (1,344 bytes)

This region holds variable-length null-terminated mission log text; decompile
of `FUN_004674f0` shows the pilot card reader scanning from `0x5AF` backwards,
implying the entries grow downward from `0x5AE`. No fixed-offset accesses
within the region. All zeros in fresh saves (no missions flown). Structure
known, content unsampled: each log entry is one or more null-terminated lines
terminated by a `0x01` styled-text byte.

*Status: open — re-gameplay (#29)*

### 3. Gap 0x2018–0x20B7 (160 bytes)

`findFunctionsReadingOffsets` returned only false positives — video-decoder
functions accessing `param_1 + 0x2044` where `param_1` is a frame buffer, not
`_campaignPilot`. No genuine PILOT struct access in this range was found
anywhere in the analyzed code. All zeros in fresh saves. Sits between the
confirmed kill-tally block (ends 0x2017) and the confirmed weapon-accuracy
block (starts 0x20B8). Could be additional kill subcategories (objective
kills, suppression counts), a mission-result history array, or reserved
padding.

*Status: open — re-gameplay (#29)*

### 4. Gap 0x21F8–0x25DF (~1,000 bytes)

`_EndOfFortMissionStats@0` (0x485040) and all callers write exclusively to
scratch globals at `0x005xxxxx` — no flush path into this region was found in
the decompile. All zeros in fresh saves. Fort/campaign-phase stats scratch
globals (`_statsFortKills__3PAJA`, `_statsFortAircraftUsed__3PAJA`, etc.) are
confirmed present but their flush path into the PILOT struct was not
identified. This region is populated only after completing campaign
fort-assault missions.

*Status: open — re-gameplay (#29)*

## Related

**Formats:** [BRF](BRF.md) — `.PT` (plane type) and `.JT` (ordnance) records
referenced by name; [M](M.md) — mission files whose outcomes feed the stats
counters; [CAM](CAM.md) — the campaign whose filename is stored at `+0xD7F`.
