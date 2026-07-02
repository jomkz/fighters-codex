# FA Game Architecture

A comprehensive overview of what is known about Jane's Fighters Anthology's runtime architecture, file formats, and subsystems. Organized by subsystem for developers picking up RE work or modding.

> **Provenance:** Ghidra static analysis of FA.EXE with [FA.SMS](formats/SMS.md) symbols applied. Confidence markers follow [spec-authoring.md](../spec-authoring.md): confirmed · inferred · unknown.

---

## Runtime Environment

FA.EXE is a Win32 application built for Windows 95/98, approximately 1.2 MB in size. It runs on modern Windows through the compatibility layer without significant modification.

A binary symbol map, **FA.SMS**, ships with the game and is a gold mine for RE work. It contains 3,829 MSVC C++ mangled function and variable names with their virtual addresses, spanning the range `0x00401000`–`0x005937E0`. Loading this map into Ghidra or IDA auto-names nearly every significant function in the executable with no manual effort required. See [formats/SMS.md](formats/SMS.md) for the file structure (4-byte count + N × 8-byte `[VA, strOffset]` records + null-terminated string table).

---

## Asset System — EALIB Archives

All game assets are packed into `.LIB` files using the EALIB container format. See [formats/LIB.md](formats/LIB.md) for the full directory entry layout.

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
| `FA_1.LIB` | Install | Fonts, icons (~1,986 PICs, 15 `.FNT` overlay DLLs) |
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
| `~` | **Campaign-specific variant or destruction-state replacement**. Two distinct uses: (1) destruction state — e.g. `~BNK5.OT` is "Damaged Shelter 5", placed by the engine in place of `BNK5.OT` after destruction (different shape, zeroed armor, bit 5 of `ot_flags` cleared); (2) campaign/theater variant — e.g. `~COLTWR.OT` adds a loop sound and sets an extra `ot_flags` bit for a specific theater, while `~MOOSE.JT` is a stand-alone Baltic-campaign weapon with no base counterpart. Mission scripting exposes `do_ifdestroyed` (FA.SMS `0x004D22D4`) and `_MISSIONFortDestroyed` for conditional destruction tracking. |

---

## Overlay System — Win32 PE DLLs

Many game subsystems are implemented as hot-swappable Win32 PE DLLs loaded at runtime. These DLLs import from `main.dll` and export named C++ functions.

**`main.dll` = FA.EXE.** `main.dll` does not exist as a file on disk and is not packed in any LIB archive. It is a logical module name used in the overlay DLLs' PE import tables to refer to FA.EXE's own address space. The functions imported under this name (`_DrawAction`, `_DrawRocker`, `_DrawText`, `_DrawFormattedText`, `_DrawEditBox`, `_DrawCampaignList`, `_okString`, `_cancelString`, `_T_HorizonProc`, and others) are all internal FA.EXE routines confirmed via FA.SMS (e.g. `_DrawAction` at `0x00489B90`, `_DrawText` at `0x00489AC0`, `_T_HorizonProc` exported as `T_HorizonProc`). FA.EXE's PE export table lists only 2 functions (`T_HorizonProc`, `WRFogLayerUpdate`) — the drawing functions are not among them. The overlay loader resolves "main.dll" imports by patching the overlay DLL's IAT directly with FA.EXE address-space pointers.

**Loading mechanism.** Where `LoadLibrary` is used (confirmed for `.LAY` — `ParseLayerFile` `0x004b4370`), it must be called with `DONT_RESOLVE_DLL_REFERENCES` or equivalent so Windows does not attempt to load the non-existent `main.dll`. The engine then patches the IAT manually. For other overlay types the exact call site is unconfirmed — the docs simply say "loaded at runtime."

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

Seven file types share a plain-text assembly-like DSL invented by Brent "Buzzboy" Iverson at Electronic Arts. See [formats/BRF.md](formats/BRF.md) for the full format specification. Implemented in `lib/src/brf.cpp`.

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

**Loading mechanism:** `FUN_00428412` (0x428412) is the canonical mission/campaign loader — called from the mission-map screen handler `FUN_00422a71`. It shuts down the prior mission, selects the `.mc_M` campaign script by NATO mode, dispatches through `_CallMissionProc_8` (0x481940), runs `_MISSIONInit2_0`, and finalizes terrain dictionaries. `_CallMissionProc_8` is also called directly by the main loop `?usnfmain@@YAXXZ`, `_MISSIONCheckSuccess@0` (0x486860), and `_MISSIONTextProc@16` (0x481c10).

### Mission Definitions (.M, .MT, .MM, .MC, .SEQ)

| Extension | Count | Description |
|-----------|-------|-------------|
| `.M`   | 517 | Mission definition files. See [formats/M.md](formats/M.md) |
| `.MM`  | 75  | Theater/map layout files. See [formats/MM.md](formats/MM.md) |
| `.MC`  | 21  | Mission condition evaluator DLLs. See [formats/MC.md](formats/MC.md) |
| `.MT`  | 363 | Mission briefing text |
| `.SEQ` | 126 | Cutscene sequencer scripts. See [formats/SEQ.md](formats/SEQ.md) |

`.MC` DLLs poll game state each tick via engine imports: `@OBJAlias@8`, `_Dist@8`, `_OnTheGround@0`, `_PopCurObj@0`, `_PushCurObj@4`, `_playerId`. They are only present for missions with non-trivial trigger logic.

**MM runtime loader** — `LibFileExists` is the MM text keyword parser. It processes one keyword line at a time; for lines beginning with a filename token ending in `.LAY` it detects the extension via `_strstr` and dispatches to `FUN_0047a510`. `FUN_0047a510` is the generic keyword dispatch handler for all MM line types: it extracts the next whitespace-delimited token (using `Sprintf` with `"%s\\%s"` to form a full path) and hands it off to the asset loader. Any trailing integer on a `layer` line (the slot index `0`, `1`, or `4` seen in shipped files) is left unconsumed and has no runtime effect.

### Pilot Save Files (.PLT)

Binary, approximately 3.4 KB. Stores pilot name, rank, stats, campaign progress, and loadout. The campaign block contains the CAM filename, aircraft `.PT` reference, ordnance inventory (`.JT` filename + `u8` quantity per slot), and sensor/ECM loadout. See [formats/P.md](formats/P.md). Field layout from offset `0xB0` to the campaign block start is not yet fully mapped.

---

## HUD & UI System

### Cockpit HUD (.HUD)

46 `.HUD` files, one per aircraft — each a Win32 PE DLL binding cockpit assets. References per-aircraft assets by `~<ac>h` (HUD overlay PIC), `~<ac>s` (symbol set PIC), `~<ac>_p` (propulsion panel), `~<ac>_w` (weapons panel), plus the `hudsym` and `winfont` fonts. Gauge positions are stored as signed s16 offset pairs relative to a per-aircraft **anchor point** (e.g. A7: 320,100 / F22: 249,100 / F14: 349,114). All HUD files have a fixed CODE virtual size of `0x2BB`. See [formats/HUD.md](formats/HUD.md).

### Fonts (.FNT)

15 `.FNT` files — Win32 PE DLLs with embedded glyph bitmap data. Filenames encode context (`HUD*`, `WIN*`, `MAP*`) and variant (`00`/`01`/`11`). `4X12.FNT` is 12,800 bytes (larger than the standard 4,608 due to glyph payload). See [formats/FNT.md](formats/FNT.md).

### Menu Screens (.MNU)

12 `.MNU` files — Win32 PE DLLs containing all UI label strings in the PE data section. Import `_DrawAction`, `_DrawRocker`, `_DrawText` from `main.dll` (= FA.EXE). One DLL per major menu screen. See [formats/MNU.md](formats/MNU.md).

### Dialog Boxes (.DLG)

92 `.DLG` files — Win32 PE DLLs for dialog boxes. Import `_DrawAction`, `_DrawRocker`, `_DrawEditBox`, `_DrawText`, `_DrawFormattedText`, `_DrawCampaignList`, `_cancelString`, `_okString` from `main.dll` (= FA.EXE). See [formats/DLG.md](formats/DLG.md).

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

PCM files use filename prefixes: `&` for looping ambient, `^` for one-shot voice callouts. Thousands of audio files are distributed across `FA_2.LIB` and `FA_4B`/`4C`/`4D.LIB`. See [formats/11K.md](formats/11K.md).

`.MUS` CODE sections are **bytecode scripts** (not x86 code). Key opcodes: `FF` = playlist ID string, `FA <sub> <u32>` = setup/config, `FB <mode> <idx>` = play XMI track, `FE <u32>` = conditional branch, `FD <u24>` = loop/jump, `FC` = shuffle marker. All 9 playlists have been decoded; game-state IDs include `"air"`, `"deck"`, `"launch"`, `"valk"`, `"brief"`, `"menu"`, `"eject"`, `"succ"`, `"home"`. See [formats/MUS.md](formats/MUS.md).

---

## Game Loop & Initialization

**Entry point:** `WinMain` at `0x476120`. Registers the window class, creates the DirectDraw surface, calls `_LoadFAResources`, then enters the shell event pump.

**Separate game thread:** `?CreateGameThread@@YAHXZ` (`0x476660`) spawns the simulation thread. The shell runs on the main Win32 thread; all gameplay runs on the game thread.

**Simulation loop:** `?FlyingLoop@@YAXXZ` (`0x404C70`) — NOT `?MainLoop`. Per-frame sequence:

1. `_timerTicks` (hardware interrupt counter) gated by `_frameTicks` to enforce fixed timestep.
2. `_TIMEUpdate` — advances `_currentTime`, `_currentTicks`, and `_timeCompression`.
3. `_OBJUpdate` — iterates `_objPtrs` array, calls each entity's `utilProc` dispatcher.
4. `_MISSIONCheckSuccess@0` (`0x486860`) — polls active `.MC` DLL each tick.
5. `_NetworkFrame` / `?MPReceive@@YGDXZ` — if multiplayer, synchronizes entity state.
6. `render_3d` → `T_DefaultHorizon` → HUD overlay composite.

**Mission init:** `?_MISSIONInit1@@YGXXZ` allocates the 300,000-byte object pool and chains 19 subsystem inits in order: TIME → OBJ → TERRAIN → NETWORK → AUDIO → HUD → ... → MISSION. Called by `FUN_00428412` (the mission/campaign loader) immediately after DLL setup.

**Object pool:** `_objPtrs` (`0x553848`) is the entity list. `_nextObjId` (`0x553838`) is the free-ID counter. `_cg` (`0x50CE80`) holds the current object context byte (set by `_T_AddObj@12`); `_curId` (`0x4F6FBC`) holds the ID of the object currently being updated.

See [game-loop.md](game-loop.md) for the full WinMain init sequence, per-frame call order, and all subsystem init VAs.

---

## Physics & Flight Model

**Core flight tick:** `_FMFlight@0` at `0x47B020`. Called once per frame per PT entity via `_OBJUpdate`. Reads the PT_TYPE struct (aerodynamics constants from the `.PT` BRF file) and the entity runtime struct, then integrates:

- **Lift/drag/thrust**: from PT fields — `one_g_stall_speed` (word at +0x50), thrust/weight ratio, induced drag coefficient.
- **Stall logic**: `_oneGStallSpeed__3JA` symbol; stall onset at computed speed, with 25-knot hysteresis on recovery.
- **Fuel consumption**: `@FMFuelConsumption@4` (`0x451E50`) reads PT fields for fuel flow at current throttle/altitude.

**Collision:** `_Collision@56` — 14-parameter function covering missile–aircraft, missile–terrain, and aircraft–terrain cases. Terrain height query is `_GetGround@0` (`0x47AF70`), which reads T2 tile elevation bands. See [formats/T2.md](formats/T2.md) for the tile record layout.

**`_PROJProc` dispatch:** `_PROJProc` at `0x4C1F50` is confirmed (**2026-05-19** via `dumpAtForced`). It is a vtable-style dispatcher: `case 1` = init/startup; `case 2` = `_PROJMoveProc` (movement/physics, `0x4C11B0`); `case 3` = `_PROJEventProc`; `case 4` = `_PROJDamageProc`. Both `_PROJProc` and `_PROJMoveProc` have no auto-created Ghidra function (vtable-only call pattern) — `dumpAtForced` is required to decompile them. The physics gap PROJ_TYPE+0x50–+0x6E is accessed through `_PROJMoveProc`.

**Dark zone `0x4D0000–0x4EFFFF`** contains the software 3D rasterizer (matrix multiply, world-to-screen projection, polygon fill), not aerodynamics. The rasterizer entry is `_GRTo2d@8`; the matrix globals (`m2`–`m9`, `_scaled_matrix`) are 16-bit fixed-point elements at `0x515F44`–`0x515F54`.

See [physics.md](physics.md) for the full flight model equations, stall parameters, and collision function signatures.

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

24 `.LAY` files — Win32 PE DLLs implementing sky and atmosphere rendering. References `wave1.SH` and `ocean*06.PIC`. Imports `_T_HorizonProc` from `main.dll` (= FA.EXE) — the engine provides the horizon renderer; the LAY file supplies lookup table data only. See [formats/LAY.md](formats/LAY.md).

**FA.EXE atmosphere subsystem** — the engine maintains the following globals for the current atmosphere state:

| Global | Role |
|--------|------|
| `currentTintTable` | Primary active-layer pointer (LAYER struct in loaded DLL) |
| `currentShadeTable` | Secondary active-layer pointer — overwritten each frame by `SetActiveLayerByAngle` based on camera elevation angle |
| `DAT_005843c4`, `DAT_005843c8` | Additional active-layer slots (transition blending) |
| `hdr`–`DAT_00580e24` | DLL data header block — 30 dwords copied verbatim from the loaded LAY DLL's CODE section at offset 0x1000 |

**Per-frame update pipeline:**

1. `ParseLayerFile` (`0x004b4370`) — loads LAY DLL (via `LoadLibrary` with import resolution disabled — see Overlay System note above), copies header block to `hdr`, initialises active-layer pointers to the DLL's default LAYER entry, calls `FindNearestColorEntry` (`0x004b3ad0`) for each LAYER entry to populate `colour_entry_ptr`, then loads cloud/sky PIC wildcards.
2. `UpdateSkyState` (`0x004b3d90`) — per-frame: smooth-transitions all atmosphere parameters and applies the result to the working palette.
3. `WRFogLayerUpdate` (`0x004b4320`) — per-frame: adds random jitter (±25, clamped to [217, 235]) to each LAYER's `fog_density` field.
4. `SetActiveLayerByAngle` (`0x004cc4b4`) — per-frame: reads camera elevation angle from AX, multiplies by `sky_angle_scale` or `below_angle_scale` (from the header block), and writes the indexed LAYER pointer into `currentShadeTable`.
5. `GetFogColour` (`0x004b3410`) — linearly interpolates the fog visibility ramp (`vis_lo`/`vis_hi` over `fog_alt_low`/`fog_alt_high`) and returns a palette colour from the LAYER's colour array.

The colour entry table (pointed to by header offset +0x6C) uses stride-0x30 entries: `[terminator_byte (0 = valid)][R][G][B][count:u32][colour_array:u32[count]]`. `FindNearestColorEntry` walks entries computing Manhattan RGB distance to find the best palette match for each LAYER's `base_rgb`.

---

## 3D Rendering Pipeline

**Algorithm:** Painter's sort — no z-buffer. Polygons are depth-sorted by centroid before submission to the rasterizer.

**Scene entry:** `T_DefaultHorizon` (`0x4AACF0`) — the terrain/sky scene renderer, exported from FA.EXE as `T_HorizonProc`. Called from the game loop render step after object updates. Drives the full frame render.

**Pipeline stages (VA range `0x4B4200–0x4BEDFF`):**

1. **Shape loading** — JPEG decoder cluster at `0x4B4BB0`–`0x4B7700` handles `.SH` file decompression. Decoded vertex/polygon data is cached in the shape pool.
2. **World-to-screen projection** — `_GRTo2d@8` reads the rotation matrix globals (`m2`–`m9` at `0x515F44`–`0x515F54`), applies fixed-point transform, writes projected coordinates to `_xv`/`_yv`/`_zv` (`0x51CDAA`–`0x51CDAE`).
3. **Clip testing** — `codes_or` / `codes_and` clip-code accumulators at `0x515F83`/`0x515F82`. Clip rectangle globals: `_clipLeft` (`0x55BE20`), `_clipTop` (`0x556D60`), `_clipRight` (`0x55BEDC`), `_clipBottom` (`0x55C024`).
4. **Polygon fill** — `render_3d` entry (`0x58F0E0` scratch pointer). Overflow buffer at `_overflow_ptr`; line statistics at `_lineStats` (`0x510288`).
5. **HUD overlay** — composited after 3D scene. Advisory icon renderer at `FUN_00407930` handles bits 6–11 of the HUD flags word (`DAT_0050cfef`).

**WR atmosphere subsystem:** `WRFogLayerUpdate` (`0x004B4320`), `UpdateSkyState` (`0x004B3D90`), `SetActiveLayerByAngle` (`0x004CC4B4`). Fog, palette animation, lens flare, and texture remap cache all live in `0x4B4200–0x4BEDFF`.

**Dispatch table:** `vector_table` (`0x5183A0`) is the render dispatch vector table (function pointer array, 141 xrefs).

See [renderer.md](renderer.md) for the full pipeline, shape system, and camera/viewport details.

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
| `NET.DAT` | 3,552 bytes | Binary multiplayer network settings (IPX/TCP addresses, session config), mostly null-padded. See [formats/DAT.md](formats/DAT.md) |

**CN_INFO struct** — total 0xDDC bytes. Written by `CN_WriteConfig`; read by `CN_ReadConfig`. Key fields:

| Offset | Size | Field |
|--------|------|-------|
| +0x00  | u32  | transport type (0=IPX, 1=TCP, 2=serial, 3=modem) |
| +0x04  | u8   | player name (32 bytes) |
| +0x28  | u32  | session host flag |
| +0x2C  | u32  | max players |
| +0xDB0 | —    | TCP config block (IP address string, port) |

Full 50+ field mapping in [network.md](network.md).

**Multiplayer frame sync:** `?MPReceive@@YGDXZ` (`0x46C980`) is the network frame entry. Dispatches on packet type byte (0x10–0x51). Key types: 0x10 = entity position update, 0x20 = weapon fire, 0x30 = kill notification, 0x40 = mission state sync. Serial transport uses `SERIAL_QUEUE` reliability layer with sequence numbers and ACKs.

**Key globals:** `_numComputers` (`0x4EB604`) = player count; `_thisComputer` (`0x4EB608`) = this machine's index; `_gamePrefs` (`0x4EB6F8`) = SP prefs struct pointer; `_gameMultiPrefs` (`0x4EB6FC`) = MP prefs struct pointer.

See [network.md](network.md) for the full protocol, transport layer details, and IP.EXE role.

---

## Runtime Entity Struct — Key Offsets

The FA entity struct (one instance per live game object, base pointer stored per-object at runtime) has 80+ confirmed field offsets from Ghidra analysis. Selected key fields:

| Offset | Size | Role | Evidence |
|--------|------|------|---------|
| `+0x00` | u16 | Object ID | `_OBJInit@4`, `_nextObjId` |
| `+0x02` | u8  | Object type (`_cgt`) | `_T_AddObj@12` |
| `+0x04` | u16 | Object class (`obj_class`) | BRF `utilProc` dispatch |
| `+0x08` | u32 | `utilProc` function pointer | BRF loader |
| `+0x10` | s16[3] | World position X/Y/Z | `_GRTo2d@8`, flight model |
| `+0x20` | s16[3] | Velocity X/Y/Z | `_FMFlight@0` |
| `+0x30` | s16[3] | Orientation matrix row 0 | matrix multiply |
| `+0x40` | u16 | Health / hitpoints remaining | `_DAMAGEDoHit@12` |
| `+0x44` | u16 | Armor value | BRF OBJ_TYPE load |
| `+0x60` | u8  | AI state | `_CTEval_*` condition fns |
| `+0xF0` | u32 | Target entity ID (NPC nav) | GAS init cases 0x03/0x05 |
| `+0xF4` | u16 | Reaction parameter 1 | GAS init |
| `+0xF6` | u16 | Reaction parameter 2 | GAS init; `_Reaction_12` (0x464040), `_MaskEvents_4` |
| `+0xF8` | u16 | Reaction parameter 3 | GAS init |
| `+0xFA` | u8  | Mode byte | GAS init; `_CTEval_tgt` / `_CTEval_p` |
| `+0xFF` | — | Confirmed read by: `_Kill@0` (0x473c10), `@AmmoForClass@4` (0x474740) | `0xFF` scan |
| `+0x100` | u8 | Primary per-frame state byte — most-polled field in flight loop | `_FMFlight@0`, `_MovePlane@0`, `_GVEventProc`, `CheckForEvents2`, and ~15 others |
| `+0x101` | u16 | Timeout timer | GAS init |
| `+0x10A` | — | Speed/energy field | `_MaxSpeed@8` (0x477d50) |
| `+0x10C` | — | Campaign/init context — read by `_CampaignSave`, `_CallCampaignProc@4`, `_MISSIONLoadCommonResources@0` | `0x10C` scan |
| `+0x114` | — | Init handle / capability field — read by `?InitCobra@@YAGPAUGlobalData@@@Z`, `?InitVideo@@YAGPAUGlobalData@@@Z` | `0x114` scan |
| `+0x16F` | u32 | HUD state flags word (`DAT_0050cfef`) — advisory bits, damage state, ejection triggers | `ChangePlaneType` |

Full 80+ field table with all confirmed offsets is in [structs.md](structs.md).

Weapon / projectile entity offsets are documented separately in [formats/JT.md](formats/JT.md) (PROJ_TYPE runtime mapping at `missile+0xA6` onward).

---

## RE Resources

**FA.SMS** — Binary symbol map shipping with the game. 3,829 MSVC-mangled C++ symbols with virtual addresses. Load into Ghidra or IDA to auto-name all functions. Structure: `u32` count + N × `[VA u32, strOffset u32]` + null-terminated string table (string table base at byte offset 30,636). See [formats/SMS.md](formats/SMS.md).

**symbols.md** — Organized reference of all 3,829 FA.SMS symbols, grouped by subsystem with demangled names. See [symbols.md](symbols.md).

**OpenFA project** (GitLab, Rust) — Primary external reference for BRF/OT/NT/PT formats and the EALIB parser.

**fighters-codex** (this repo):
- `fx` CLI — LIB archive operations
- `fx-gui` — GUI editor for LIB archives
- `lib/src/blast.cpp` — DCL-Blast decompressor
- `lib/src/brf.cpp` — BRF format parser
