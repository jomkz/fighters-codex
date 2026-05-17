# FA Game Architecture

A comprehensive overview of what is known about Jane's Fighters Anthology's runtime architecture, file formats, and subsystems. Organized by subsystem for developers picking up RE work or modding.

---

## Runtime Environment

FA.EXE is a Win32 application built for Windows 95/98, approximately 1.2 MB in size. It runs on modern Windows through the compatibility layer without significant modification.

A binary symbol map, **FA.SMS**, ships with the game and is a gold mine for RE work. It contains 3,829 MSVC C++ mangled function and variable names with their virtual addresses, spanning the range `0x00401000`–`0x005937E0`. Loading this map into Ghidra or IDA auto-names nearly every significant function in the executable with no manual effort required. See [formats/SMS.md](formats/SMS.md) for the file structure (4-byte count + N × 8-byte `[VA, strOffset]` records + null-terminated string table).

---

## Asset System — EALIB Archives

All game assets are packed into `.LIB` files using the EALIB container format. See [formats/EALIB.md](formats/EALIB.md) for the full directory entry layout.

**Container structure:** magic `EALIB`, 18-byte directory entries per file. The flags byte in each entry determines compression:

| Flag | Compression |
|------|-------------|
| `0`  | Raw (no compression) |
| `1`  | LZSS |
| `3`  | PXPK |
| `4`  | DCL-Blast (implemented in `lib/src/blast.cpp`) |

**Key LIB files:**

| File | Location | Contents |
|------|----------|----------|
| `FA_1.LIB` | Install | Fonts, icons (~1,986 PICs) |
| `FA_2.LIB` | Install | Main assets — ~5,405 entries covering most game data |
| `FA_3.LIB` | Disk 2 | Aircraft skins and tech sheets (822 PICs, 269 INF files) |
| `FA_4B.LIB` | Install | Digital audio (full install only) |
| `FA_4C.LIB` | Disk 1 | CD audio |
| `FA_7.LIB` | Disk 1 | Briefing FMVs (355 VDO + 355 FBC files) |

**Asset referencing:** Assets are referenced by name with a `~` prefix in other files (e.g. `~f22h` resolves to the F22H asset from whichever LIB contains it). Special filename prefixes carry semantic meaning:

| Prefix | Meaning |
|--------|---------|
| `&` | Looping ambient sound |
| `^` | One-shot voice callout |
| `$` | Weapon icon PIC |
| `_` | Aircraft skin PIC |

---

## Overlay System — Win32 PE DLLs

Many game subsystems are implemented as hot-swappable Win32 PE DLLs loaded via `LoadLibrary()` at runtime. These DLLs import from `main.dll` (the FA engine core) and export named C++ functions.

Most overlay DLLs decompress to **4,608 bytes** — a fixed-size slot in the engine's overlay pool. Exceptions: `.FNT` files vary (e.g. `4X12.FNT` = 12,800 bytes), and `.LAY` files are substantially larger (16,896–20,992 bytes) to accommodate embedded rendering lookup tables.

All overlay DLLs use **Phar Lap PE format** — the signature bytes are `PL\0\0`, not the standard `PE\0\0`. The CODE section is typically a data-only structure (dispatch table, bytecode script, or lookup tables) with no compiled x86 instructions; only `.BI` and `.MC` DLLs contain actual machine code.

**Overlay DLL types:**

| Extension | Count | Purpose |
|-----------|-------|---------|
| `.HUD`    | 46    | Cockpit layout — one per aircraft |
| `.MNU`    | 12    | Menu screens |
| `.DLG`    | 92    | Dialog boxes |
| `.CAM`    | 6     | Campaign logic |
| `.MUS`    | 9     | Music sequencer/playlist |
| `.LAY`    | 24    | Sky and atmosphere rendering |
| `.HGR`    | —     | Hangar screen |
| `.FNT`    | 15    | Font glyph bitmaps |
| `.PTS`    | —     | Aircraft icon lookup |
| `.MC`     | 21    | Mission condition evaluator |
| `.BI`     | 9     | AI runtime library |

For disassembly: load the DLL in Ghidra, import the FA.SMS symbol list to auto-name engine imports, then trace from the exported entry point.

---

## Object Type System — Brent's Relocatable Format (BRF)

Seven file types share a plain-text assembly-like DSL invented by a developer named "Brent". See [formats/BRF.md](formats/BRF.md) for the full format specification. Implemented in `lib/src/brf.cpp`.

**Magic header:** `[brent's_relocatable_format]`

**Directives:** `byte`, `word`, `dword`, `ptr`, `symbol`, `string`, `end`  
**Labels:** `:name`  
**Hex literals:** `$XX`  
**Fixed-point values:** `^XXXXX`

The `struct_type` byte at position 1 distinguishes object categories:

| Type byte | Extension | Object category |
|-----------|-----------|----------------|
| `1`  | `.OT`  | Static object |
| `3`  | `.NT`  | NPC / ground vehicle |
| `5`  | `.PT`  | Playable aircraft |
| `7`  | `.JT`  | Projectile / weapon |
| `8`  | `.GAS` | Fuel tank |
| `9`  | `.ECM` | ECM pod |
| `10` | `.SEE` | Seeker / sensor |

All types share an `OBJ_TYPE` section (hitpoints, shape reference, year, sounds, movement). PT and JT/NT add further sections (aerodynamics, hardpoints, seeker parameters).

**`utilProc` symbol mapping:**

| Symbol | Applies to |
|--------|-----------|
| `_OBJProc`  | Static objects (OT) |
| `_GVProc`   | Ground vehicles and naval vessels (NT) — IOWA and KIROV confirmed; likely dispatches by `obj_class` internally |
| `_PROJProc` | Projectiles (JT) |
| PT-specific | Playable aircraft |

**`^` fixed-point range fields:** Confirmed 1 unit = 1 foot across sensor and weapon files (e.g. `^60760` = 10 nm, `^360000` = 60 nm). 6,076 units = 1 nautical mile.

**`obj_class` word — confirmed values:**

| Value | Class |
|-------|-------|
| `$40`   | Scenery / terrain feature (non-targetable) |
| `$100`  | Ground structure (building, runway) |
| `$2000` | Naval vessel |

The OpenFA project (GitLab, Rust) is the primary external reference for BRF format details, particularly for OT, NT, and PT types.

---

## AI System

The AI system consists of two tightly paired file types per object category. See [formats/AI.md](formats/AI.md) and [formats/BI.md](formats/BI.md).

**.AI files** (plain text): goto-based scripts. Each line follows the pattern:

```
LABEL: CONDITION ACTION
```

The interpreter evaluates conditions and dispatches to named action and condition functions by string lookup at runtime.

**.BI files** (Win32 PE DLL): the AI runtime library. Exports two families of functions by name:

- `_CTDo_*` — action functions: `btoh`, `circle`, `exit`, `homeangle`, `homepos`, `immelman`, `invert`, `jink`, `maneuver`, `move`, `movetoalt`, `restart`, `wm_approach`, `wm_break`, `yoyo`
- `_CTEval_*` — condition functions: `alt`, `altdiff`, `speed`, `tgt`, `tgtahead`, `tgtisfighter`, `engagep`, and others

FA_2.LIB contains 9 `.AI`/`.BI` pairs, one per major object category (e.g. `F.AI`/`F.BI` for fighters).

---

## Campaign & Mission System

### Campaign Logic (.CAM)

Six `.CAM` files, each a Win32 PE DLL implementing campaign logic for one theater. Each DLL embeds mission lists (e.g. `~U01.M`–`~U50.M` for the Ukraine campaign), weapon and sensor tables, aircraft type names, and campaign state strings. See [formats/CAM.md](formats/CAM.md).

Exported API includes: `_AddCampaignPlane`, `_InitCampaignPilot`, `_SeqStart`, plus campaign-specific functions such as `_UkraineAddA7` and `_VietnamMedals`.

### Mission Definitions (.M, .MT, .MM, .MC, .SEQ)

| Extension | Count | Description |
|-----------|-------|-------------|
| `.M`   | 517 | Mission definition files. See [formats/MISSION.md](formats/MISSION.md) |
| `.MM`  | 75  | Theater/map layout files. See [formats/MM.md](formats/MM.md) |
| `.MC`  | 21  | Mission condition evaluator DLLs. See [formats/MC.md](formats/MC.md) |
| `.MT`  | 363 | Mission briefing text |
| `.SEQ` | 126 | Cutscene sequencer scripts. See [formats/SEQ.md](formats/SEQ.md) |

`.MC` DLLs poll game state each tick via engine imports: `@OBJAlias@8`, `_Dist@8`, `_OnTheGround@0`, `_PopCurObj@0`, `_PushCurObj@4`, `_playerId`. They are only present for missions with non-trivial trigger logic.

### Pilot Save Files (.PLT)

Binary, approximately 3.4 KB. Stores pilot name, rank, stats, campaign progress, and loadout. The campaign block contains the CAM filename, aircraft `.PT` reference, ordnance inventory (`.JT` filename + `u8` quantity per slot), and sensor/ECM loadout. See [formats/PLT.md](formats/PLT.md). Field layout from offset `0xB0` to the campaign block start is not yet fully mapped.

---

## HUD & UI System

### Cockpit HUD (.HUD)

46 `.HUD` files, one per aircraft — each a Win32 PE DLL binding cockpit assets. References per-aircraft assets by `~<ac>h` (HUD overlay PIC), `~<ac>s` (symbol set PIC), `~<ac>_p` (propulsion panel), `~<ac>_w` (weapons panel), plus the `hudsym` and `winfont` fonts. Gauge positions are stored as signed s16 offset pairs relative to a per-aircraft **anchor point** (e.g. A7: 320,100 / F22: 249,100 / F14: 349,114). All HUD files have a fixed CODE virtual size of `0x2BB`. See [formats/HUD.md](formats/HUD.md).

### Fonts (.FNT)

15 `.FNT` files — Win32 PE DLLs with embedded glyph bitmap data. Filenames encode context (`HUD*`, `WIN*`, `MAP*`) and variant (`00`/`01`/`11`). `4X12.FNT` is 12,800 bytes (larger than the standard 4,608 due to glyph payload). See [formats/FNT.md](formats/FNT.md).

### Menu Screens (.MNU)

12 `.MNU` files — Win32 PE DLLs containing all UI label strings in the PE data section. Import `_DrawAction`, `_DrawRocker`, `_DrawText` from `main.dll`. One DLL per major menu screen. See [formats/MNU.md](formats/MNU.md).

### Dialog Boxes (.DLG)

92 `.DLG` files — Win32 PE DLLs for dialog boxes. Import `_DrawAction`, `_DrawRocker`, `_DrawEditBox`, `_DrawText`, `_DrawFormattedText`, `_DrawCampaignList`, `_cancelString`, `_okString` from `main.dll`. See [formats/DLG.md](formats/DLG.md).

### Bitmap Assets (.PIC, .PAL)

`.PIC` files are 8-bit indexed bitmaps. Distribution across LIBs:

| LIB | PIC count | Contents |
|-----|-----------|---------|
| FA_1.LIB | 1,986 | UI elements, icons |
| FA_2.LIB | 1,158 | General game assets |
| FA_3.LIB | 822   | Aircraft skin textures |

`.PAL` is the global color palette. See [formats/PIC.md](formats/PIC.md) and [formats/PAL.md](formats/PAL.md).

---

## Audio System

| Extension | Count | Description |
|-----------|-------|-------------|
| `.XMI`   | 78 | Extended MIDI sequences for music and events. See [formats/XMI.md](formats/XMI.md) |
| `.MUS`   | 9  | Win32 PE DLL music sequencer/playlist. See [formats/MUS.md](formats/MUS.md) |
| `.11K`   | —  | Raw uncompressed PCM at 11 kHz |
| `.5K`    | —  | Raw uncompressed PCM at 5 kHz |
| `.8K`    | —  | Raw uncompressed PCM at 8 kHz |

PCM files use filename prefixes: `&` for looping ambient, `^` for one-shot voice callouts. Thousands of audio files are distributed across `FA_2.LIB` and `FA_4B`/`4C`/`4D.LIB`. See [formats/AUDIO.md](formats/AUDIO.md).

`.MUS` CODE sections are **bytecode scripts** (not x86 code). Key opcodes: `FF` = playlist ID string, `FA <sub> <u32>` = setup/config, `FB <mode> <idx>` = play XMI track, `FE <u32>` = conditional branch, `FD <u24>` = loop/jump, `FC` = shuffle marker. All 9 playlists have been decoded; game-state IDs include `"air"`, `"deck"`, `"launch"`, `"valk"`, `"brief"`, `"menu"`, `"eject"`, `"succ"`, `"home"`. See [formats/MUS.md](formats/MUS.md).

---

## Terrain & 3D System

### Terrain Maps (.T2)

16 `.T2` files — one per theater map. See [formats/T2.md](formats/T2.md).

**Structure:** `BIT2` magic, 128-byte header, then N_tiles × 195 bytes. Each tile contains 65 × 3-byte records:

- Record 0: tile summary
- Records 1–64: 8×8 sub-tile grid

Each 3-byte record encodes `[surface_class, elevation_band, texture_variant]`. `0xFF` = water.

### 3D Shapes (.SH)

1,275 `.SH` files covering aircraft, vehicles, weapons, buildings, and terrain features. See [formats/SH.md](formats/SH.md).

### Sky & Atmosphere (.LAY)

24 `.LAY` files — Win32 PE DLLs implementing sky and atmosphere rendering. References `wave1.SH` and `ocean*06.PIC`. Imports (not exports) `_T_HorizonProc` from `main.dll` — the engine provides the horizon renderer; the LAY file supplies lookup table data only. See [formats/LAY.md](formats/LAY.md).

---

## Briefing & Reference System

| Extension | Count | Location | Description |
|-----------|-------|----------|-------------|
| `.INF`   | 269   | FA_3.LIB | Aircraft tech sheets. See [formats/INF.md](formats/INF.md) |
| `.VDO`   | 355   | FA_7.LIB | Briefing video sequences. See [formats/VDO.md](formats/VDO.md) |
| `.FBC`   | 355   | FA_7.LIB | Briefing file companion data. See [formats/FBC.md](formats/FBC.md) |
| `.CB8`   | —     | —        | MPEG/video format for FMV cutscenes. See [formats/CB8.md](formats/CB8.md) |
| `.BIN`   | 6     | —        | Lookup tables and palette data. See [formats/BIN.md](formats/BIN.md) |

`.INF` files use a custom dot-command markup: `.body`, `.title`, `.center`, `.left` directives for display, plus a machine-readable footer with `LENGTH`/`HEIGHT`/`WINGSPAN`/`WEIGHT`/`PERFORMANCE` key-value pairs parsed directly by the engine.

`.BIN` named files: `MIX2L`/`MIX4L` = floor(x/N) lookup tables; `MIX2`/`MIX4` = gamma-corrected variants; `VFONTPAL` = 16 VGA 6-bit RGB palette entries for the video briefing font.

---

## Multiplayer & Configuration

| File | Size | Description |
|------|------|-------------|
| `EA.CFG`  | 347 bytes   | Binary game configuration (graphics, controls, audio, pilot slot). See [formats/CFG.md](formats/CFG.md) |
| `NET.DAT` | 3,552 bytes | Binary multiplayer network settings (IPX/TCP addresses, session config), mostly null-padded. See [formats/NET.md](formats/NET.md) |

---

## RE Resources

**FA.SMS** — Binary symbol map shipping with the game. 3,829 MSVC-mangled C++ symbols with virtual addresses. Load into Ghidra or IDA to auto-name all functions. Structure: `u32` count + N × `[VA u32, strOffset u32]` + null-terminated string table (string table base at byte offset 30,636). See [formats/SMS.md](formats/SMS.md).

**OpenFA project** (GitLab, Rust) — Primary external reference for BRF/OT/NT/PT formats and the EALIB parser.

**fighters-toolkit** (this repo):
- `ft` CLI — LIB archive operations
- `ft-gui` — GUI editor for LIB archives
- `lib/src/blast.cpp` — DCL-Blast decompressor
- `lib/src/brf.cpp` — BRF format parser
