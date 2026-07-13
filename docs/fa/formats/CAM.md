---
format: CAM
name: Campaign Definition
extensions: [".CAM"]
category: mission
endianness: little
spec:
  status: complete
codec:
  direction: read
  rationale: "engine-code container (campaign DLL): fx_lib reads the section geometry and embedded config strings; writing would mean emitting compiled DLL code, which is fighters-legacy territory, not asset tooling"
  lib: [lib/src/cam.cpp]
  commands: [cam]
  tests: [tests/test_cam.cpp]
  fuzz: [fuzz/fuzz_cam.cpp]
  gui: [gui/src/editors/cam_editor.cpp]
  fixtures:
    synthetic: true
    real_manifest: true
    real_install: false
related: [P, M, MC, BRF]
---

# CAM — Campaign Definition (.CAM)

FA_2.LIB contains 6 `.CAM` files — one per built-in campaign. Pilot save files
(`.P`) store the active campaign by this filename. Each is a **Win32 PE DLL**
loaded by the FA engine at runtime.

## Tools

### fx

```
fx cam info    <file.CAM>            # container check + CODE section geometry
fx cam strings <file.CAM> [-n MIN]   # embedded campaign string tables
```

Read-only by design: the campaign *configuration* is what the tooling
surfaces; the surrounding DLL machine code is engine territory
(fighters-legacy), not asset tooling.

## File Layout

All multi-byte integers are little-endian.

Win32-style DLL: `MZ` stub + a **Phar Lap `PL\0\0`** image (PE32 section
layout with a COFF section table — `pe_code_section` in `lib/src/pe.cpp`
reads it; verified against BALTIC.CAM: sections `CODE`/`.idata`/`.reloc`/
`$$DOSX`, CODE at raw `0x400`, VMA `0x1000`). The standard DOS stub message
`"!This program cannot be run in DOS mode."` is present, followed by standard
PE sections `.idata` and `.reloc`. `BALTIC.CAM` decompresses to **8704
bytes**; `UKRAINE.CAM` is larger to accommodate its 50-mission list.

All CAM files import from `main.dll` (= the game executable — see
[architecture.md](../architecture.md#overlay-system--win32-pe-dlls)) and
export a campaign-specific set of functions that the engine calls to drive
campaign state.

### CODE Section Binary Layout — confirmed

`AnalyzeCAMDLL.java` scanned the executable blocks (CODE section). Full string
dump of UKRAINE.CAM CODE section (`0x1000–0x19ff`, 2560 bytes):

| CODE offset | Content | Role |
|-------------|---------|------|
| `0x1075` | `"Fighters Anthology (CD 1)"` | Required disc label |
| `0x108f` | `"FA_4C.LIB"` | LIB archive for CD assets |
| `0x112e` | `"SU33"` | Aircraft type available in campaign |
| `0x1166`–`0x11c9` | `F150.GAS`–`F500.GAS` (4 entries) | Fuel tank BRF files |
| `0x11ea`–`0x141d` | `AA11.JT`–`AAS38.SEE` (18 weapon entries) | Weapon/sensor store pool |
| `0x1477` | `"F22N"` | Second aircraft type (F-22 Night) |
| `0x149a`–`0x158f` | `U01I`–`U50I` (50 × 5 bytes) | Mission slot IDs — initial/available state keys |
| `0x1594`–`0x16eb` | `~U01.M`–`~U50.M` (50 × 7 bytes) | Mission filename list (7 bytes each, null-terminated) |
| `0x16eb`–`0x17bb` | *(0xD1 bytes — all `0x00`)* | Null padding — confirmed by direct hex dump; no decoded content |
| `0x17bc`–`0x17d2` | `UMEDAL`, `UDEAD`, `UWON`, `ULOST` | Campaign outcome state IDs |
| `0x17e3`–`0x1873` | `U01O`, `U03O`, `U05O`, … `U49O` (25 × odd missions) | Secondary mission outcome IDs (odd-numbered missions only) |

**KURILE.CAM** uses the same layout with prefix `K` and 35 missions; its CODE
section is `0x1000–0x17ff` (2048 bytes). Mission list starts at `0x14e1`
(shorter weapon table). VIETNAM.CAM (`T` prefix, 25 missions), EGYPT.CAM
(`E`), BALTIC.CAM (`B`), VLAD.CAM (`V`) follow the same schema.

### Embedded Data

Each `.CAM` DLL embeds its campaign configuration directly as flat arrays and
null-terminated strings:

**CD and LIB reference:**
```
Fighters Anthology (CD 1)
FA_4C.LIB
```
The campaign specifies which installation disc and LIB archive it requires for
its assets.

**Aircraft availability** — named aircraft type identifiers listed before the
weapon table; e.g. UKRAINE.CAM includes `SU33`, `F22N`; BALTIC.CAM includes
`E2000`, `GRIPEN`, `RAFALE`, `ASTOVLF`. These are the aircraft the campaign
makes available to the player.

**Weapon / stores tables** — a sequence of BRF asset filenames (JT, GAS, SEE,
ECM extensions) defines the weapon and sensor loadout pool for the campaign:

```
GSH301.JT   M61.JT    GAU12.JT  GSH30.JT  ADEN.JT   ...
F150.GAS    F250.GAS  F350.GAS  F500.GAS
AAS38.SEE   ALQ167.ECM
AA11.JT     AA12.JT   AIM7.JT   AIM9M.JT  AIM120.JT ...
```

**Mission list** — null-terminated strings of the form `~<prefix><NN>.M`, one
per mission in sequence order. The `~` prefix on mission filenames is the
game's notation for LIB-resident mission files.

**Campaign state identifiers** — short string keys track per-mission and
campaign-wide state:

| Pattern | Example | Meaning |
|---------|---------|---------|
| `<pre>NNI` | `U01I`, `U02I` | Mission initial (available/locked) state |
| `<pre>NNO` | `U01O`, `U03O` | Mission objective complete flag (odd-indexed in UKRAINE) |
| `<pre>MEDAL` | `UMEDAL` | Campaign medal awarded |
| `<pre>DEAD` | `UDEAD` | Player death recorded |
| `<pre>WON` | `UWON` | Campaign won flag |
| `<pre>LOST` | `ULOST` | Campaign lost flag |

### Reference Chain: .CAM → .M → .MM / .LAY / .MC

The `.CAM` DLL holds only its own mission list (`~<prefix>NN.M` strings). It
does **not** reference `.MM` theater files, `.LAY` sky files, or `.MC`
condition files directly. Those references are carried by each `.M` mission
file via three keywords:

| Keyword | Target | Example |
|---------|--------|---------|
| `map` | `.MM` theater file | `map ukr.T2` → loads `UKR.MM` |
| `layer` | `.LAY` sky file | `layer day2.LAY 0` |
| `code` | `.MC` condition DLL | `code u01` → loads `U01.MC`; `code extra01` → loads `EXTRA01.MC` |

The `.MC` file to load is determined by the `code` directive in the `.M` file,
not by the `.CAM` DLL. Most missions use a unique per-mission `.MC` (`U01.MC`,
`K16.MC`, etc.); bonus missions share the generic `EXTRA01.MC` gate via
`code extra01`.

## File Inventory

| File | Missions | Theater prefix |
|------|----------|----------------|
| BALTIC.CAM | 40 (`~B01.M`–`~B40.M`) | `B` |
| EGYPT.CAM | 40 (`~E01.M`–`~E40.M`) | `E` |
| KURILE.CAM | 35 (`~K01.M`–`~K35.M`) | `K` |
| UKRAINE.CAM | 50 (`~U01.M`–`~U50.M`) | `U` |
| VIETNAM.CAM | 25 (`~T01.M`–`~T25.M`) | `T` |
| VLAD.CAM | 40 (`~V01.M`–`~V40.M`) | `V` |

All six live in FA_2.LIB.

## Engine Notes

### Loading Mechanism

`FUN_00428412` (0x428412) is the canonical the game executable campaign/mission loader.
Called from the mission-map screen handler `FUN_00422a71` (when
`_curScreen == 3`) and from `FUN_0042a71a`.

Execution sequence:
1. `_MISSIONShutdown_0()` — teardown prior mission
2. `_MISSIONInit1_0()` — engine pre-init
3. Select `.mc_M` file: `s__mc_nato_M_004f0ca8` or `s__mc_M_004f0ca0` based on
   `_natoFighters` flag
4. `_CallMissionProc_8(pcVar2, 0)` — load the campaign DLL
5. Copy mission name string; call `_CallMissionProc_8(&_missionName, 0)` for
   named missions
6. `_MISSIONInit2_0()` — post-DLL init; zeros six globals; calls
   `FUN_00422828`, `FUN_004242a0(0)`, `FUN_00428340`
7. `_T_NamedTmaps_0()` / `_T_InitDictionary_0()` — terrain dictionary
   initialization

`_CallMissionProc_8` (0x481940) is the central mission-DLL dispatcher. Its
callers: `FUN_00428412`, `_ChooseScoreInit` (0x441c60), `_MISSIONTextProc@16`
(0x481c10), `_MISSIONCheckSuccess@0` (0x486860), and `?usnfmain@@YAXXZ`
(0x403700 — main loop).

### DLL Entry Point and Command Protocol

`AnalyzeCAMDLL.java` confirmed the dispatch function for KURILE.CAM
(representative of all CAM DLLs). The single exported entry point is at PE
code offset `0x1000` (`FUN_00001000`). The game executable calls it via
`_CallMissionProc_8`, passing a command byte as the first stack argument
(`in_stack_00000004`).

**Command byte protocol:**

| Cmd | Action |
|-----|--------|
| `0x00` | No-op / init query — returns immediately |
| `0x01` | String match + copy: scan `DAT_000014e1` (mission name string) against current `_missionName`, then copy result to `DAT_000017b6` |
| `0x02`–`0x03` | No-op |
| `0x04` | Flag check: if `DAT_000017bc != 0`, set `DAT_000017a4 = 1` and invoke the campaign callback subfunction |
| `0x05`–`0x08` | No-op |

Data globals are stored in the PE `.data` section at offsets around
`0x1700`–`0x17C0`. The mission list string table starts at `DAT_000014e1`.

### Import Table (Functions CAM DLL Calls from the game executable)

The `.idata` section of each CAM DLL lists the the game executable functions it calls back
into. Note: these appear in the CAM DLL's **import table** (calls out to
The game executable) — they are not functions exported by the DLL.

**Common imports (all campaigns):**

| Function | Role |
|----------|------|
| `_AddCampaignPlane` | Add aircraft to campaign fleet |
| `_AddCampaignStore` | Add weapon/store to campaign pool |
| `_CampaignPlanesLeft@0` | Return remaining aircraft count |
| `_CheckCD` | Verify correct CD inserted |
| `_DoFadeout@0` | Trigger screen fade transition |
| `_GetKeySlow` | Wait for key input |
| `_InitCampaignPilot` | Initialize pilot state for campaign start |
| `_SeqContinue` | Resume a cutscene sequence |
| `_SeqStart` | Start a cutscene sequence |
| `_campaignFailed` | Handle campaign failure outcome |
| `_campaignFailures` | Access failure count |
| `_campaignSucceeded` | Handle campaign success outcome |
| `_missionName` | Global: current mission name string pointer |
| `_playerDead` | Handle player aircraft loss |
| `@G_Flush@4` | Flush graphics to screen |

**Campaign-specific imports (KURILE.CAM example):**

| Function | Role |
|----------|------|
| `_KurileMedals` | Kurile-specific medal award logic |
| `_KurilePromotions` | Rank promotion handling |
| `_KurileQuit` | Campaign exit handler |
| `_KurileRescued` | Rescued pilot tracking |

UKRAINE.CAM, VIETNAM.CAM, etc. import analogous `_Ukraine*` / `_Vietnam*`
functions from the game executable.

## Related

**Formats:** [P](P.md) — pilot save files store the active campaign `.CAM`
filename; [M](M.md) — `.M` mission files referenced by `~<prefix>NN.M`
strings; [MC](MC.md) — per-mission condition files; [BRF](BRF.md) — `.JT`,
`.GAS`, `.SEE`, `.ECM` weapon type files listed in the weapon table.

**Engine:** [architecture.md](../architecture.md#overlay-system--win32-pe-dlls)
— the overlay DLL loading architecture.
