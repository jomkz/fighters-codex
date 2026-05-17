# Campaign Definition (.CAM)

FA_2.LIB contains 6 `.CAM` files — one per built-in campaign. Pilot save files (`.P`) store the active campaign by this filename. Each is a **Win32 PE DLL** loaded by the FA engine at runtime via `LoadLibrary`.

## File Inventory

| File | Missions | Theater prefix |
|------|----------|----------------|
| BALTIC.CAM | — | — |
| EGYPT.CAM | — | — |
| KURILE.CAM | — | — |
| UKRAINE.CAM | 50 (`~U01.M`–`~U50.M`) | `U` |
| VIETNAM.CAM | 25 (`~T01.M`–`~T25.M`) | `T` |
| VLAD.CAM | — | — |

The `~` prefix on mission filenames is the game's notation for LIB-resident mission files.

## Format

Win32 PE DLL (`MZ` stub + PE32 image). The standard DOS stub message `"!This program cannot be run in DOS mode."` is present, followed by standard PE sections `.idata` and `.reloc`. `BALTIC.CAM` decompresses to **8704 bytes**; `UKRAINE.CAM` is larger to accommodate its 50-mission list.

All CAM files import from `main.dll` (the FA engine executable export table) and export a campaign-specific set of functions that the engine calls to drive campaign state.

## Embedded Data

Each `.CAM` DLL embeds its campaign configuration directly in the data section as flat arrays and null-terminated strings. String analysis reveals:

### CD and LIB Reference
```
Fighters Anthology (CD 1)
FA_4C.LIB
```
The campaign specifies which installation disc and LIB archive it requires for its assets.

### Aircraft Availability
Named aircraft type identifiers listed before the weapon table — e.g. UKRAINE.CAM includes `SU33`, `F22N`; BALTIC.CAM includes `E2000`, `GRIPEN`, `RAFALE`, `ASTOVLF`. These are the aircraft the campaign makes available to the player.

### Weapon / Stores Tables
A sequence of BRF asset filenames (JT, GAS, SEE, ECM extensions) defines the weapon and sensor loadout pool for the campaign:

```
GSH301.JT   M61.JT    GAU12.JT  GSH30.JT  ADEN.JT   ...
F150.GAS    F250.GAS  F350.GAS  F500.GAS
AAS38.SEE   ALQ167.ECM
AA11.JT     AA12.JT   AIM7.JT   AIM9M.JT  AIM120.JT ...
```

### Mission List
Null-terminated strings of the form `~<prefix><NN>.M`, one per mission in sequence order:

```
~U01.M  ~U02.M  ...  ~U50.M   (UKRAINE.CAM — 50 missions)
~T01.M  ~T02.M  ...  ~T25.M   (VIETNAM.CAM — 25 missions)
```

### Campaign State Identifiers
Short string keys track per-mission and campaign-wide state:

| Pattern | Example | Meaning |
|---------|---------|---------|
| `<pre>NNI` | `U01I`, `U02I` | Mission initial (available/locked) state |
| `<pre>NNO` | `U01O`, `U03O` | Mission objective complete flag (odd-indexed in UKRAINE) |
| `<pre>MEDAL` | `UMEDAL` | Campaign medal awarded |
| `<pre>DEAD` | `UDEAD` | Player death recorded |
| `<pre>WON` | `UWON` | Campaign won flag |
| `<pre>LOST` | `ULOST` | Campaign lost flag |

## Exported Functions (Campaign Engine API)

All CAM DLLs export the same core set of functions. Campaign-specific functions are also exported.

### Common API (all campaigns)

| Export | Description |
|--------|-------------|
| `_AddCampaignPlane` | Add an aircraft to the campaign fleet |
| `_AddCampaignStore` | Add a weapon/store to the campaign pool |
| `_CampaignPlanesLeft@0` | Return remaining aircraft count |
| `_CheckCD` | Verify correct CD is inserted |
| `_DoFadeout@0` | Trigger screen fade transition |
| `_GetKeySlow` | Wait for key input |
| `_InitCampaignPilot` | Initialize pilot state for campaign start |
| `_SeqContinue` | Resume a cutscene sequence |
| `_SeqStart` | Start a cutscene sequence |
| `_campaignFailed` | Handle campaign failure outcome |
| `_campaignFailures` | Access failure count |
| `_campaignSucceeded` | Handle campaign success outcome |
| `_missionName` | Return display name for current mission |
| `_playerDead` | Handle player aircraft loss |

### Campaign-Specific Examples

| Export | Campaign | Description |
|--------|----------|-------------|
| `_UkraineAddA7` | UKRAINE | Special unlock: add A-7 aircraft to fleet |
| `_UkraineCheckMaxPlanes` | UKRAINE | Enforce fleet size cap |
| `_UkraineMedals` | UKRAINE | Ukraine-specific medal logic |
| `_UkraineQuit` | UKRAINE | Campaign exit handler |
| `_UkraineRescued` | UKRAINE | Rescued pilot tracking |
| `_VietnamMedals` | VIETNAM | Vietnam-specific medal logic |
| `_VietnamPromotions` | VIETNAM | Rank promotion handling |
| `_VietnamQuit` | VIETNAM | Campaign exit handler |
| `_VietnamRescued` | VIETNAM | Rescued pilot tracking |
| `_PlayCobra@4` | VIETNAM | Play Cobra helicopter cutscene |

## Relationship to .MC Files

The 21 `.MC` files in FA_2.LIB appear to be per-mission state checkpoints (e.g. `~U01.MC`, `CATFAIL.MC`, `TRAIN01.MC`), not per-campaign. The `.CAM` DLL likely loads the appropriate `.MC` file when a mission completes to update the campaign progression state. The exact `.MC` format is TBD.

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 6 |

## TODO — Deep Dive

- Disassemble UKRAINE.CAM to confirm the binary layout of the mission state and weapon tables (offsets, sizes, field encoding)
- Identify which `.MC` files correspond to which campaigns and missions
- Determine how `.CAM` references theater `.MM` files (if at all — the `.M` mission files may carry that reference instead)

## Related

- [PLT.md](PLT.md) — pilot save files store the active campaign `.CAM` filename
- [MISSION.md](MISSION.md) — `.M` mission files referenced by `~<prefix>NN.M` strings
- [MC.md](MC.md) — per-mission state checkpoint files
- [BRF.md](BRF.md) — `.JT`, `.GAS`, `.SEE`, `.ECM` weapon type files listed in the weapon table
