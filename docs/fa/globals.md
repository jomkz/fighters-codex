# FA.EXE Global Variable Reference

Inventory of all named global variables recovered from FA.EXE via FA.SMS symbol map and
DumpGlobals.java scan. Data sourced from `DumpGlobals.csv` (Ghidra headless run).

---

## Summary

| Metric | Value |
|--------|-------|
| Total data symbols scanned | 58,742 |
| Named globals (from FA.SMS) | 4,212 |
| Named globals with size > 0 | 1,944 |
| Unnamed data items | ~54,530 |

**By size category (named with size > 0):**

| Size | Count | Notes |
|------|-------|-------|
| byte (1) | 1,048 | Flags, boolean state, small counters |
| dword (4) | 659 | Pointers, 32-bit counts, tick counters |
| word (2) | 196 | Screen coords, IDs, 16-bit counters |
| small struct/array (8–32) | 26 | Matrices, fixed-size records |
| large (>32) | 8 | Buffers, large tables |

**By address range:**

| Range | Count | Segment |
|-------|-------|---------|
| `0x400000–0x4FFFFF` | 833 | Code + embedded constants |
| `0x500000–0x53FFFF` | 452 | Initialized data |
| `0x540000–0x55FFFF` | 284 | Initialized data |
| `0x560000–0x59FFFF` | 375 | Initialized/BSS data |

---

## Top 30 Most-Referenced Globals

These are the most-accessed runtime state variables across the entire binary.

| Address | Name | Size | Type | Xrefs | First Writer | Notes |
|---------|------|------|------|-------|--------------|-------|
| `0x553848` | `_objPtrs` | 4 | ptr | 408 | `_OBJShutdown@0` | Object pool pointer array — the entity list |
| `0x50CE80` | `_cg` | 1 | byte | 340 | `_T_AddObj@12` | Current game object context / active object slot |
| `0x4EB604` | `_numComputers` | 4 | dword | 321 | `?MPConnect@@YGXXZ` | Number of players/computers in session |
| `0x4F6FBC` | `_curId` | 2 | word | 309 | `@GetCurObj@4` | Current object ID being processed |
| `0x4EB608` | `_thisComputer` | 4 | dword | 299 | `?MPConnect@@YGXXZ` | This player's computer index in session |
| `0x520A50` | `_curScreen` | 2 | word | 249 | `?usnfmain@@YAXXZ` | Current UI screen ID |
| `0x520A1C` | `_playerId` | 2 | word | 237 | `?_MISSIONInit1@@YGXXZ` | Local player entity ID |
| `0x4EB6F8` | `_gamePrefs` | 4 | ptr | 215 | `?MPMissionShutdown@@YGXXZ` | Pointer to single-player preferences struct |
| `0x4EB6FC` | `_gameMultiPrefs` | 4 | ptr | 209 | `FUN_00429dde` | Pointer to multiplayer preferences struct |
| `0x552ED4` | `?curDialog@@3PAUDIALOG@@A` | 4 | ptr | 175 | `_DialogSetup@12` | Active dialog box pointer |
| `0x501504` | `_cb` | 4 | ptr | 141 | `@G_SetBitmap@4` | Current bitmap / render target pointer |
| `0x5183A0` | `vector_table` | 4 | ptr* | 141 | — | Dispatch vector table (function pointer array) |
| `0x515F90` | `_resbuf` | 4 | ptr | 123 | `_GRTo2d@8` | Rasterizer result buffer pointer |
| `0x553838` | `_nextObjId` | 2 | word | 119 | `_OBJInit@4` | Next available object ID counter |
| `0x5528E0` | `_currentTime` | 2 | word | 111 | `_TIMEInit@12` | Game time (mission elapsed, ticks) |
| `0x552928` | `_currentTicks` | 4 | dword | 108 | `_TIMEInit@12` | Absolute tick counter |
| `0x5528C8` | `_currentT` | 2 | word | 102 | `_TIMEInit@12` | Current time (aliased view of `_currentTime`) |
| `0x583DC0` | `_curPalette` | 4 | ptr | 86 | `FUN_004afb40` | Active 256-color palette pointer |
| `0x4FB1A8` | `_missionName` | 4 | ptr | 84 | `FUN_00428412` | Pointer to current mission name string |
| `0x5528EC` | `_timerTicks` | 4 | dword | 84 | `_InstallTimerInt` | Raw timer interrupt tick count |
| `0x521DE8` | `_shellMousePos` | 2 | word | 75 | `?ShellMousePos@@YGXXZ` | Shell UI mouse position (packed x/y) |
| `0x5528BC` | `_fortMission` | 1 | byte | 66 | `?_MISSIONInit1@@YGXXZ` | Non-zero when mission is a fortress/fort mode |
| `0x58F0E0` | `_overflow_ptr` | 4 | ptr | 62 | `render_3d` | 3D renderer overflow scratch pointer |
| `0x546BA0` | `_serviceTicks` | 2 | word | 62 | `@GetCurObj@4` | Per-object service tick timestamp |
| `0x515F90` | `_xv/_yv/_zv` | 2 | word | 62 | `_GRTo2d@8` | Rasterizer projected vertex coordinates |
| `0x510288` | `_lineStats` | 4 | dword | 61 | `FUN_0045dedf` | Line-draw statistics counter |
| `0x5528F8` | `_timeCompression` | 1 | byte | 61 | `_TIMEInit@12` | Time compression multiplier (fast-forward) |
| `0x515F44` | `_scaled_matrix` | 2 | word | 57 | `FUN_004cdeb4` | Scaled rotation matrix element |
| `0x517F34` | `vbuf` | 2 | word | 56 | `FUN_004d5356` | Vertex buffer pointer |
| `0x556868` | `?appIO@@3P6AJJPAD@ZA` | 4 | ptr | 54 | `?NET_Initialize@@YAJPAUCN_INFO@@J@Z` | Network I/O function pointer |

---

## Globals by Subsystem

### Object / Entity System

| Address | Name | Size | Xrefs | Role |
|---------|------|------|-------|------|
| `0x553848` | `_objPtrs` | 4 | 408 | Entity list — pointer to array of object pointers |
| `0x50CE80` | `_cg` | 1 | 340 | Current game object context byte |
| `0x4F6FBC` | `_curId` | 2 | 309 | ID of object currently being updated |
| `0x553838` | `_nextObjId` | 2 | 119 | Next free object ID |
| `0x546BA0` | `_serviceTicks` | 2 | 62 | Per-object last-service timestamp |
| `0x50D268` | `_cgt` | 1 | 43 | Current object type byte |
| `0x5469A0` | `_wingIds` | 2 | 53 | Wing member IDs array |
| `0x546980` | `_wingSizes` | 1 | 43 | Wing sizes array |
| `0x538270` | `?goalP@@3FA` | 2 | 41 | Goal/waypoint position pointer |

### Timing

| Address | Name | Size | Xrefs | Role |
|---------|------|------|-------|------|
| `0x5528E0` | `_currentTime` | 2 | 111 | Mission elapsed time (game ticks) |
| `0x552928` | `_currentTicks` | 4 | 108 | Absolute tick counter since init |
| `0x5528C8` | `_currentT` | 2 | 102 | Current time (alias used by MC condition logic) |
| `0x5528EC` | `_timerTicks` | 4 | 84 | Raw hardware timer interrupt count |
| `0x5528F8` | `_timeCompression` | 1 | 61 | Time-compression factor (1 = normal) |

### Multiplayer / Network

| Address | Name | Size | Xrefs | Role |
|---------|------|------|-------|------|
| `0x4EB604` | `_numComputers` | 4 | 321 | Number of computers (players) in session |
| `0x4EB608` | `_thisComputer` | 4 | 299 | This machine's player index |
| `0x4EB6F8` | `_gamePrefs` | 4 | 215 | Single-player preferences struct pointer |
| `0x4EB6FC` | `_gameMultiPrefs` | 4 | 209 | Multiplayer preferences struct pointer |
| `0x556868` | `?appIO@@3P6AJJPAD@ZA` | 4 | 54 | Network I/O dispatch function pointer |
| `0x529200` | `_janesOnlineName` | 1 | 40 | Player's online/callsign name string |
| `0x520A28` | `_humanChoseSide` | 4 | 41 | Non-zero when human player chose a side |

### Mission System

| Address | Name | Size | Xrefs | Role |
|---------|------|------|-------|------|
| `0x520A1C` | `_playerId` | 2 | 237 | Local player's entity ID |
| `0x4FB1A8` | `_missionName` | 4 | 84 | Pointer to current mission name |
| `0x5528BC` | `_fortMission` | 1 | 66 | Fortress mission flag |
| `0x5473F0` | `?numHints@@3JA` | 4 | 40 | Number of hint messages |

### UI / Dialog

| Address | Name | Size | Xrefs | Role |
|---------|------|------|-------|------|
| `0x520A50` | `_curScreen` | 2 | 249 | Current menu/screen ID |
| `0x552ED4` | `?curDialog@@3PAUDIALOG@@A` | 4 | 175 | Active DIALOG struct pointer |
| `0x521DE8` | `_shellMousePos` | 2 | 75 | Shell UI mouse X/Y (packed) |
| `0x552FAC` | `?dialogItemPtr@@3PAEA` | 4 | 46 | Active dialog item pointer |
| `0x5222F8` | `_shellButtons` | 1 | 40 | Shell button state bitmask |

### Graphics / Renderer

| Address | Name | Size | Xrefs | Role |
|---------|------|------|-------|------|
| `0x501504` | `_cb` | 4 | 141 | Current bitmap / render surface |
| `0x515F90` | `_resbuf` | 4 | 123 | Rasterizer result buffer |
| `0x583DC0` | `_curPalette` | 4 | 86 | Active 256-color palette pointer |
| `0x58F0E0` | `_overflow_ptr` | 4 | 62 | 3D renderer scratch overflow |
| `0x51CDAA` | `_xv` | 2 | 62 | Projected vertex X |
| `0x51CDAC` | `_yv` | 2 | 62 | Projected vertex Y |
| `0x51CDAE` | `_zv` | 2 | 62 | Projected vertex Z |
| `0x510288` | `_lineStats` | 4 | 61 | Rasterizer line count |
| `0x515F44` | `_scaled_matrix` | 2 | 57 | Scaled rotation matrix element |
| `0x517F34` | `vbuf` | 2 | 56 | Vertex buffer |
| `0x55BE20` | `_clipLeft` | 4 | 55 | Clip rectangle left edge |
| `0x556D60` | `_clipTop` | 4 | 51 | Clip rectangle top edge |
| `0x55C024` | `_clipBottom` | 4 | 48 | Clip rectangle bottom edge |
| `0x55BEDC` | `_clipRight` | 4 | 46 | Clip rectangle right edge |
| `0x5183A0` | `vector_table` | 4 | 141 | Render dispatch vector table |
| `0x4EB730` | `_xscale` | 2 | 47 | X scale factor (resolution/viewport) |
| `0x4EB734` | `_yscale` | 1 | 52 | Y scale factor |
| `0x4EB630` | `_mainV` | 2 | 66 | Main view/viewport ID |

### Rotation Matrix (render_3d)

The matrix globals (`m2`–`m9`, `_scaled_matrix`) are individual 16-bit fixed-point
elements of the 3D rotation matrix used by the software rasterizer. All accessed by
`FUN_004cdeb4` (matrix multiply) and `_GRTo2d@8` (world-to-screen projection).

| Address | Name | Element |
|---------|------|---------|
| `0x515F46` | `m2` | Row 0, col 2 |
| `0x515F48` | `m3` | Row 1, col 0 |
| `0x515F4C` | `m5` | Row 1, col 2 |
| `0x515F4E` | `m6` | Row 2, col 0 |
| `0x515F50` | `m7` | Row 2, col 1 |
| `0x515F52` | `m8` | Row 2, col 2 |
| `0x515F54` | `m9` | (reserved/w) |
| `0x515F83` | `codes_or` | Clip code OR |
| `0x515F82` | `codes_and` | Clip code AND |

### Audio / Sequencer

Audio globals are named with `_SEQ*` prefixes (sound sequencer) and are concentrated
in the `0x546000–0x548000` range. These are accessed exclusively by `_SEQmusic`,
`_SEQfadein`, `_SEQfadeout`, and related functions. The full list is in
`DumpGlobals_named.csv` (43 entries).

Key entries:

| Address | Name | Size | Xrefs | Role |
|---------|------|------|-------|------|
| (see CSV) | `_SEQ*` | byte/word | 2–15 | Sequencer channel state, volumes, fade params |

### Terrain

| Address | Name | Size | Xrefs | Role |
|---------|------|------|-------|------|
| (see CSV) | `_T_*` / `_terrain*` | varies | 3–20 | Terrain tile cache, height map pointers |

---

## Notable Globals for C Reimplementation

These globals represent the minimum set of extern declarations needed for a C
reimplementation to link correctly:

```c
// Object system
extern void     **_objPtrs;          // 0x553848 — entity list
extern uint8_t    _cg;               // 0x50CE80 — current object context
extern uint16_t   _curId;            // 0x4F6FBC — current object ID
extern uint16_t   _nextObjId;        // 0x553838 — next free ID

// Timing
extern uint16_t   _currentTime;      // 0x5528E0 — mission elapsed ticks
extern uint32_t   _currentTicks;     // 0x552928 — absolute tick counter
extern uint32_t   _timerTicks;       // 0x5528EC — hardware timer count
extern uint8_t    _timeCompression;  // 0x5528F8 — time-compression factor

// Multiplayer
extern uint32_t   _numComputers;     // 0x4EB604 — player count
extern uint32_t   _thisComputer;     // 0x4EB608 — this player index
extern void      *_gamePrefs;        // 0x4EB6F8 — SP prefs struct
extern void      *_gameMultiPrefs;   // 0x4EB6FC — MP prefs struct

// Mission
extern uint16_t   _playerId;         // 0x520A1C — local player entity ID
extern void      *_missionName;      // 0x4FB1A8 — mission name pointer
extern uint8_t    _fortMission;      // 0x5528BC — fortress mission flag

// UI
extern uint16_t   _curScreen;        // 0x520A50 — current screen ID
extern void      *curDialog;         // 0x552ED4 — active dialog pointer

// Graphics
extern void      *_cb;               // 0x501504 — current bitmap
extern void      *_curPalette;       // 0x583DC0 — active palette
extern int16_t    _xv, _yv, _zv;    // 0x51CDAA/AC/AE — projected vertex
extern int32_t    _clipLeft, _clipRight, _clipTop, _clipBottom; // clip rect
```

---

## Full Named Global Listing

The complete list of 1,944 named globals (filtered from FA.SMS) is available in
`%FA_PROJECT%/output/DumpGlobals_named.csv` with columns:
`address, name, size_bytes, data_type, xref_count, first_writer`

The raw full export (all 58,742 symbols including switch tables and unnamed data)
is in `%FA_PROJECT%/output/DumpGlobals.csv`.
