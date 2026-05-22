# Pilot Save -- Pilot Profile (.P)

`PLTnnn.P` files (e.g. `PLT441.P`, `PLT628.P`) are **binary pilot save files** written
by the FA engine. They store the persistent state for each pilot slot.

Unlike all other FA data, pilot files are stored **directly in the FA install directory**,
not inside any `.LIB` archive. The filename number does not imply a slot sequence â€” it
appears to be a randomly assigned identifier.

## File Size

All observed files are exactly **9,696 bytes**.

## Field Layout

`_campaignPilot` global base VA = `0x004f8bb8` (from FA.SMS). File size 0x25E0 = 9,696 bytes.
All offsets below are confirmed from decompile (`DumpAllFunctions.txt`) or computed from base VA.

### Identity block (0x00â€“0xAF) â€” fully mapped

| Offset | Size | Type | Field |
|--------|------|------|-------|
| `0x00` | 1 | u8 | Type / version tag â€” observed: `0x0F` |
| `0x01` | 63 | char[] | Pilot name, null-padded |
| `0x40` | 32 | char[] | Callsign, null-padded |
| `0x61` | 13 | char[] | Callsign voice file (e.g. `^ACID.5K`), null-padded |
| `0x6E` | 13 | char[] | Nose art ID (e.g. `NOSE01`), null-padded |
| `0x7B` | 13 | char[] | Left wing decal ID (e.g. `LEFT03`), null-padded |
| `0x88` | 13 | char[] | Right wing decal ID (e.g. `RIGHT03`), null-padded |
| `0x95` | 13 | char[] | Pilot portrait ID (e.g. `PILOT02`), null-padded |
| `0xA2` | 14 | char[] | Rank string (e.g. `2nd Lieutenant`), null-padded |

### Text and display region (0xB0â€“0xDAD) â€” partially mapped

Four anchor fields confirmed from `FUN_004674f0` (pilot card display, VA 0x4674f0); gaps between them remain unmapped.

| Offset | Size | Type | Field |
|--------|------|------|-------|
| `0xB0` | 18 | ? | **Unknown** |
| `0xC2` | ~13 | char[] | Secondary identity string â€” printed on pilot card after rank (squadron, unit, or location); exact length TBD |
| `0xCF` | 1344 | ? | **Unknown** |
| `0x5AF` | var | char[][] | Mission log â€” up to 10 null-terminated entries read sequentially; each up to 3 lines; likely mission history |
| `0xD7F` | 13 | char[] | Campaign `.CAM` filename (e.g. `EGYPT.CAM`) â€” **confirmed**: `DAT_004f9937 = _campaignPilot + 0xD7F`, written by campaign init |
| `0xD8C` | 32 | char[] | Campaign display name (e.g. `Egypt 1998`) â€” **confirmed**: `DAT_004f9944 = _campaignPilot + 0xD8C`, written by campaign init |
| `0xDAC` | 2 | u16 | Pilot status enum â€” **confirmed**: `DAT_004f9964 = _campaignPilot + 0xDAC`; `0`=Available, `1`=On mission, `2`=MIA, `3`=KIA, `4`=Retired From Active Duty |

### Campaign data strings (0xDAEâ€“0x1C5F) â€” partially mapped

Variable-length null-terminated strings packed sequentially from 0xDAE:
- Assigned aircraft `.PT` reference (e.g. `F22.PT`)
- Available aircraft pool (`.PT` references)
- Sensor/ECM loadout (`.SEE`, `.ECM` references)
- Other campaign-specific strings

### Ordnance inventory (0x1C60â€“0x1F7F) â€” confirmed

**50 entries Ã— 16 bytes** = 800 bytes. `DAT_004fa818 = _campaignPilot + 0x1C60`.

| Field | Offset within entry | Size | Type |
|-------|---------------------|------|------|
| Weapon type filename (`.JT`) | +0x00 | 14 | char[] (null-padded) |
| Quantity | +0x0E | 2 | s16 (`-1` if slot unused; `0x7FFF` = unlimited) |

Managed by `_AddCampaignStore` (0x480E10): searches by name, increments/decrements quantity, or allocates a free slot.

### Stats counters (0x1F80â€“0x21F7) â€” confirmed from RE

All fields confirmed via `FUN_00485380` (0x485380, end-of-mission stats flush) and related functions. `_campaignPilot` base `0x4f8bb8` + listed offset = VA of each field.

#### Mission and loss counters (0x1F80â€“0x1FAF)

| Offset | VA | Size | Field |
|--------|----|------|-------|
| `0x1F80` | `DAT_004fab38` | u32 | Missions flown (total) |
| `0x1F84` | `DAT_004fab3c` | u32 | Wingman missions |
| `0x1F88` | `DAT_004fab40` | u32 | Missions failed â€” copied to `_campaignFailures` (0x54e418) before campaign proc |
| `0x1F8C` | `DAT_004fab44` | u32 | Total shots fired â€” accumulated from per-mission `DAT_0054ddc4` |
| `0x1F90` | `DAT_004fab48` | u32 | Ejections / bail-outs |
| `0x1F94` | `DAT_004fab4c` | u32 | Wingman KIA |
| `0x1F98` | `DAT_004fab50` | u32 | Player aircraft damage % accumulated |
| `0x1F9C` | `DAT_004fab54` | u32 | Wingman aircraft damage % accumulated |
| `0x1FA0` | `DAT_004fab58` | u32 | Player landing count |
| `0x1FA4` | `DAT_004fab5c` | u32 | Wingman landing count |
| `0x1FA8` | `DAT_004fab60` | u32 | Player landing quality score |
| `0x1FAC` | `DAT_004fab64` | u32 | Wingman landing quality score |

#### Kill tallies by target class (0x1FB0â€“0x2017)

13 kill categories; each has **player u32** then **wingman u32** (8 bytes per category). Category dispatch from `_KillStats_12` (0x485820) based on `obj_class` word bits:

| Offset | VA (player) | Category | `obj_class` bits |
|--------|-------------|----------|------------------|
| `0x1FB0` | `DAT_004fab68` | Air â€” aircraft / fighters | `0x8000` set |
| `0x1FB8` | `DAT_004fab70` | Air â€” type B (fighters subtype) | `0x4000` set |
| `0x1FC0` | `DAT_004fab78` | Aircraft destroyed by crash or BA weapon | obj byte 0 = `0x04` with OBJ_TYPE+0xba bit 3 |
| `0x1FC8` | `DAT_004fab80` | Naval vessels | `0x2000` set |
| `0x1FD0` | `DAT_004fab88` | SAM launchers | `0x1000` set |
| `0x1FD8` | `DAT_004fab90` | AAA guns | `0x800` set |
| `0x1FE0` | `DAT_004fab98` | Armor / tanks | `0x400` set |
| `0x1FE8` | `DAT_004faba0` | APCs | `0x200` set |
| `0x1FF0` | `DAT_004faba8` | Vehicles / trucks | `0x100` set |
| `0x1FF8` | `DAT_004fabb0` | Infantry | `0x40` set |
| `0x2000` | `DAT_004fabb8` | Friendly fire | same faction, other conditions |
| `0x2008` | `DAT_004fabc0` | Air â€” non-`0x8000` (non-fighter aerial) | `0x8000` absent, aerial |
| `0x2010` | `DAT_004fabc8` | Capital ships | naval + hitpoints > 999 |

Wingman slot for each = player VA + 4.

#### Unknown gap (0x2018â€“0x20B7, 0xA0 bytes)

#### Weapon accuracy stats (0x20B8â€“0x21F7) â€” confirmed

8 weapon-type groups; each group = **player slot** (0x14 bytes) + **wingman slot** (0x14 bytes) = 0x28 bytes per group.

Each slot = 5 Ã— u32: `[damage_total, shots_fired, hits, type3, kills]`
Dispatched by `FUN_004856f0` (0x4856f0) based on `OBJ_TYPE` flags; accumulated by `FUN_004854a0` (0x4854a0).

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

### Tail region (0x21F8â€“0x25DF) â€” unmapped

Remaining ~0x3E8 bytes. Likely contains fort/campaign-phase stats, multiplayer scoring, and other end-of-campaign totals. Not yet decoded.

## RE and Differential Analysis â€” Gap Status

Static Ghidra analysis (`AnalyzePLT.java`, 46,985-line output) and binary diff of three existing
pilot saves produced the following findings for each unmapped gap:

### Gap 0xB0â€“0xC1 (18 bytes)

**RE result:** No named DAT_ label or MOV/CMP instruction targeting VA `0x004f8c68`â€“`0x004f8c79`
found in any function in FA.EXE.  
**Differential result:** All zeros in all three saves (PLT441.P, PLT628.P, PLT937.P).  
**Status: unmapped.** Struct context suggests these bytes are written only after campaign
assignment â€” possibly a score tier index, medal count, or secondary rank fields.
Requires a post-gameplay save for differential analysis.

### Gap 0xCFâ€“0x5AE (1,344 bytes)

**RE result:** This region holds variable-length null-terminated mission log text; decompile of
`FUN_004674f0` shows the pilot card reader scanning from `0x5AF` backwards, implying the entries
grow downward from `0x5AE`. No fixed-offset accesses within the region.  
**Differential result:** All zeros in all three saves (fresh pilots, no missions flown).  
**Status: partially understood â€” structure known, content unsampled.** Each log entry is one
or more null-terminated lines terminated by a `0x01` styled-text byte. Differential pass
requires saves taken after completing at least 3â€“5 missions.

### Gap 0x2018â€“0x20B7 (160 bytes)

**RE result:** `findFunctionsReadingOffsets` returned only false positives â€” video-decoder
functions accessing `param_1 + 0x2044` where `param_1` is a frame buffer, not `_campaignPilot`.
No genuine PILOT struct access in this range was found anywhere in the analyzed code.  
**Differential result:** All zeros in all three saves.  
**Status: unmapped.** Sits between the confirmed kill-tally block (ends 0x2017) and the
confirmed weapon-accuracy block (starts 0x20B8). Could be additional kill subcategories
(objective kills, suppression counts), a mission-result history array, or reserved padding.

### Gap 0x21F8â€“0x25DF (~1,000 bytes)

**RE result:** `_EndOfFortMissionStats@0` (0x485040) and all callers write exclusively to
scratch globals at `0x005xxxxx` â€” no flush path into this region was found in the decompile.
`findFunctionsReadingOffsets` returned only false positives (explosion-struct offsets,
keystroke comparisons, random constants).  
**Differential result:** All zeros in all three saves.  
**Status: unmapped.** Fort/campaign-phase stats scratch globals (`_statsFortKills__3PAJA`,
`_statsFortAircraftUsed__3PAJA`, etc.) are confirmed present but their flush path into the
PILOT struct was not identified. This region is populated only after completing campaign
fort-assault missions. Requires a post-fort-mission save for differential analysis.

### Recommended next step

All four gaps require pilot saves taken after actual gameplay:
1. Complete 5+ standard missions â†’ diff PLT for gaps 0xCFâ€“0x5AE and 0x2018â€“0x20B7
2. Complete a fort-assault mission â†’ diff for gap 0x21F8â€“0x25DF
3. Advance pilot rank (enough missions/score) â†’ diff for gap 0xB0â€“0xC1

Tools: **HxD** (side-by-side compare â†’ Differ) or **010 Editor** with the template from
the confirmed fields above.

## Confirmed Engine Functions (FA.SMS)

| VA | Symbol | Description |
|----|--------|-------------|
| `0x467180` | `PilotSave(PILOT*, short)` | Write pilot save â€” takes a `PILOT*` and a short slot index; serialises the identity block and stats block to `PLTnnn.P` |

Additional confirmed functions from `DumpAllFunctions.txt`:

| VA | Symbol/Name | Description |
|----|-------------|-------------|
| `0x4674f0` | `FUN_004674f0` | Pilot card display â€” renders pilot dossier text; accesses `+0xC2`, `+0x5AF`, `+0xD8C`, `+0xDAC`, `+0x1F88` (formats missions-failed count into display buffer) |
| `0x467860` | `FUN_00467860` | String copy helper â€” copies until `\x01` byte (styled text terminator) |
| `0x480E10` | `_AddCampaignStore` | Adds or increments an ordnance entry in the ordnance inventory table at `+0x1C60` |
| `0x481320` | `_CampaignSave` | Saves `_campaignPilot` to disk (copies to RM, then `_SaveFile` with full 0x25E0 bytes) |
| `0x484D90` | `_EndOfMissionStats@0` | Computes per-mission damage %, landing, protection, and player/wm state into temp globals |
| `0x485040` | `_EndOfFortMissionStats@0` | Computes fort-related kill/suppression stats into named temp globals |
| `0x485380` | `FUN_00485380` | Flushes all per-mission temp stats into permanent PILOT struct fields at `+0x1F80` onwards |
| `0x4854E0` | `_WpnStats@28` | Per-shot weapon stats accumulator; updates `shots_fired`, `hits`, `damage_total`, `kills` in temp buffer |
| `0x485820` | `_KillStats@12` | Records kill into the correct kill-category slot (13 categories at `+0x1FB0`) based on target's `obj_class` |
| `0x485A40` | `_LandingStats@12` | Accumulates landing count and quality score into temp globals |

`PilotSave` saves the full struct as a single 9,696-byte block via `_SaveFile`. The stats counters are accumulated by the functions above into `_campaignPilot` directly.

## Related

- [BRF.md](BRF.md) â€” `.PT` (plane type) and `.JT` (ordnance) records referenced by name
- [M.md](M.md) â€” `.MM` map/campaign files referenced by name

## Applications

```
fx plt info  PLT441.P        # identity block: name, rank, campaign, ordnance
fx plt dump  PLT441.P        # full stats block: missions, kills, weapon accuracy
```

The identity block (`0x01`â€“`0xAF`) is fully mapped and editable via the fx-gui PLT editor.
The stats counters (`0x1F80`â€“`0x21F7`) are confirmed from RE and displayed by `fx plt dump`
and the fx-gui stats pane. The four gap regions remain unmapped (see above).

- **FATK** â€” free (abandonware, 1998); original GUI tool with full pilot editing support; requires a compatibility layer on 64-bit Windows
- **HxD** â€” free, Windows; use with the field table above for manual patching
- **010 Editor** `$` â€” paid; binary templates will allow a fully labelled struct view once all gaps are mapped
