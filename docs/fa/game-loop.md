# FA Game Loop Architecture

Analysis of FA.EXE's main loop, initialization sequence, per-frame object dispatch, frame timing, mission init, and shutdown. Derived from Ghidra decompile output with FA.SMS symbols applied.

---

## Overview

FA.EXE's execution model is a multi-threaded Win32 application. The Win32 message loop lives on the main thread; the simulation runs on a dedicated game thread. A separate timer thread drives `_timerTicks`. The in-flight simulation loop (`?FlyingLoop@@YAXXZ`) is a do-while loop that runs until a scenario-end condition or player exit is detected.

---

## 1. WinMain / CRT Entry Point

### `_WinMainCRTStartup` — `0x004D9D00` (FA.SMS: `_WinMainCRTStartup`)

The true PE entry point. It is the MSVC CRT startup stub:

1. Calls `GetVersion()` and stores the result in `___winmajor`, `___winminor`, `___winver`, and `DAT_0051e754`.
2. Initialises the MSVC heap (`__heap_init`), multithreading (`__mtinit`), I/O (`__ioinit`), and the multibyte character table (`___initmbctable`).
3. Retrieves the command line (`GetCommandLineA`) and environment (`___crtGetEnvironmentStringsA`); exits with code -1 if either is null.
4. Parses the command-line string (handling quoted paths) and calls `__setargv`, `__setenvp`, `__cinit`.
5. Calls `GetStartupInfoA` and `GetModuleHandleA(NULL)`, then invokes `_WinMain_16`.
6. Does not return — calls `_exit(iVar3)` with WinMain's return value.

### `_WinMain@16` — `0x00476120` (FA.SMS: `_WinMain@16`)

The application WinMain. Steps:

1. Calls `?InitApplication@@YAHPAX@Z` (`0x004764B0`), passing the `HINSTANCE`. If this returns zero (failure), WinMain returns 0 immediately.
2. Enters the standard Win32 message loop: `GetMessageA` → `DispatchMessageA` until `GetMessageA` returns 0.
3. Returns `msg.wParam` as the exit code.

The window title is registered as `"Fighters Anthology"` (string at `0x004EB834`), referenced from `?InitApplication@@YAHPAX@Z` at `0x00476568` and from `?DisplayCopyright@@YAXPAX@Z` at `0x0047682E`.

### `?usnfmain@@YAXXZ` — `0x00403700` (FA.SMS: `?usnfmain@@YAXXZ`)

This is the game's own "main" function invoked after the window is set up. It references the `"Fighters Anthology"` window class string at `0x004EB760` (from offset `0x00403D41`). Full decompile of this function was not included in the analysis output — its exact body is unresolved beyond the string reference.

---

## 2. Initialization Sequence

### Thread Launch

Before the simulation loop starts, `?CreateGameThread@@YAHXZ` (`0x00476660`) spawns the game thread:

| Step | Detail |
|------|--------|
| `CreateThread` | Spawns `?StartGameThread@@YAKPAK@Z` (`0x00436320`) as the game thread entry |
| `_hGameThread` / `_dwGameThreadID` | Handles stored as globals |
| Handshake | Main thread polls `GetExitCodeThread` for exit code `0x103` (STILL_ACTIVE) with 30-second timeout; returns 0 on timeout |
| Timer thread | A separate `_hTimeThread` / `_dwTimeThreadID` also exists; managed symmetrically by `?EndGame@@YAXXZ` |

`FUN_00476780` (`0x00476780`) checks `SystemParametersInfoA(0x10, ...)` (SPI_GETSCREENSAVER) before the thread is spawned and saves the result to `DAT_004F7EF0`; `FUN_004767D0` (`0x004767D0`) restores the screensaver setting on exit.

### `?SetupCobra@@YAGPAUGlobalData@@@Z` — `0x0046B4E0`

This is the top-level subsystem initializer. It calls three init functions in fixed order and rolls back on failure:

```
SetupCobra:
    InitCobra  →  if fail: return error code
    InitVideo  →  if fail: CleanCobra, return error code
    InitAudio  →  if fail: CleanVideo, CleanCobra, return error code
    return 0 (success)
```

All three take a `GlobalData*` pointer (referred to as `param_1` throughout).

### `?InitCobra@@YAGPAUGlobalData@@@Z` — `0x0046AE10`

Opens the movie/FMV data file via `_LibOpen_8` and validates the file header magic:

| Magic (u32 LE) | Meaning |
|----------------|---------|
| `0x43425241` (`CBRA`) | Cobra A |
| `0x43425242` (`CBRB`) | Cobra B |
| `0x43425243` (`CBRC`) | Cobra C — all three treated as error code `3` |
| `0x43425244` (`CBRD`) | Valid movie data; proceeds with setup |

On `CBRD`, if the frame count is below `0x67`, it configures the movie buffer layout: line stride (`param_1+0xD0`), scale factor (`param_1+0xD2`), buffer pointers (`_junkBuff__3PAEA`, `_buffMyMovie__3PAEA`), and reads the first two frame headers. Sets error status at `param_1+0xAE`.

### `?InitVideo@@YAGPAUGlobalData@@@Z` — `0x0046B120`

Configures the VESA/display subsystem:

1. Copies mode dimensions from globals (`DAT_0055C06A` width, `DAT_0055C06C` height) into `param_1+0xC142` and `param_1+0xC144`.
2. Points `param_1+0xC14A` and `param_1+0xC14E` at the fake VESA info structures (`_fakeVESAInfo__3UVGA_Info_t__A`, `_fakeVESAModeInfo__3UVGA_Mode_Info_t__A`).
3. Validates available VRAM against the required framebuffer size for the current color depth (8, 15, 16, or 32 bpp). Sets error code `5` if VRAM is insufficient.
4. Selects a video mode code stored at `param_1+0xC140`:

| Code | Meaning |
|------|---------|
| `1` | 8-bpp, non-DirectDraw |
| `2` | 8-bpp, DirectDraw |
| `3` | 16-bpp VESA (linear) |
| `4` | 16-bpp DirectDraw |

5. Calls `?SetVESATop@@YAXGG@Z` to set the display start address.

### `?InitAudio@@YAGPAUGlobalData@@@Z` — `0x0046B4C0`

Returns 0 immediately. Audio subsystem initialization is handled elsewhere — this stub exists to complete the `SetupCobra` chain.

### `_SMInit@0` — `0x0046A370`

Initializes the symbol/script manager. Calls `_GetExecutablePath_8` to locate the executable directory, then calls `_RMChangeType_12` to build a path with a modified extension, and loads a data file via `_LoadFile_16`. Falls back to a `"RELEASE"` default if the file is not found. The full init path is unresolved beyond the file-load sequence.

---

## 3. Main Loop Body

### `?MainLoop@@YAGPAUGlobalData@@@Z` — `0x0046B560`

This function drives the FMV/cutscene playback loop (Cobra video player), not the in-flight simulation. It is named `MainLoop` in FA.SMS but operates on a `GlobalData` struct that describes a movie context. Key behaviors:

- Takes a `GlobalData*` as `param_1`.
- Snapshots `_timerTicks` at entry as `iVar8` (the frame-timing baseline).
- Iterates `local_14` from `0` to `param_1[0xE6]-1` (frame count loop).
- Per frame: reads frame data from a LIB file via `_LibRead`, decodes via `_DecodeFrame__YAXPAUMovieContext__PAUFrameHeader__PAUGlobalData___Z`, and may call `_WritePalette_8` when a palette change frame is detected.
- Timing: computes elapsed cobra-time as `(_timerTicks - iVar8) * _constConv__1__cobraTime__YAJJ_Z_4JA`, result stored at `param_1+0xC194`. Busy-waits (spin loop) if the wall clock is behind the frame timestamp.
- Frame counter: `param_1[0xC1B0]` is incremented each frame; running average frame time accumulated in `local_18` / `local_28`.
- At the end of each frame outer loop iteration, calls `FUN_0046BD90` (key/mouse check) and `_G_Flush_4`.
- Returns `param_1[0xAE]` as a status code (0 = normal end, 1 = user abort, 2 = read error, 0xFF = quit).

Replay controls (`__myReplay__3JA`, `__myFForward__3JA`, `__myRewind__3JA`, `__myPause__3JA`) are zeroed on return.

### `FUN_0046BD90` — `0x0046BD90`

Per-frame user-abort check used inside the cobra loop:

- Clears `__cobraKeyFlag__3JA`.
- Calls `_GetKey()`. If the key is `0x20` (Space) or `0x1B` (Escape), or if `_cobraATQuit__3DA` is non-zero, sets `__cobraKeyFlag__3JA = 1`.
- If `_mouseButtonPresses` or `DAT_00560EF1` are non-zero, also sets the flag.
- Returns the flag value.

---

## 4. In-Flight Simulation Loop

### `?FlyingLoop@@YAXXZ` — `0x00404C70`

The core per-frame simulation loop for the in-flight screen. This is a do-while that runs until a scenario end or explicit exit condition. Per-frame sequence:

| Step | Call | Notes |
|------|------|-------|
| 1 | `_TIMEUpdate_0()` | Advances the simulation clock; updates `_frameTicks`, `_currentTime`, `_currentT` |
| 2 | `_MISSIONEndScenario_0()` | Returns non-zero → exits loop |
| 3 | Time-limit check | `_currentTime > 0x7F9A` or `_currentT > 0x7F9A` → exits loop |
| 4 | `_MPMaybePausedMsg__YGXXZ()` | Multiplayer: sends/receives pause state |
| 5 | `_MISSIONCheckSuccess_0()` | Evaluates win/lose conditions |
| 6 | `_GetPlayerControl_0()` | Reads joystick/keyboard input |
| 7 | `_WRUpdate_4(DAT_004eb654)` | World renderer update (skipped if IFM active) |
| 8 | Gated on `_frameTicks > 0` | Object sim, graphics, zone, streamers, projectile lock updates |
| 8a | `_ServiceObjects()` | Per-frame object dispatch (see §5) |
| 8b | `_GRAPHICUpdate_0()` | Particle / explosion graphics update |
| 8c | `_ZONEUpdate_0()` | Zone trigger evaluation |
| 8d | `_StreamersUpdate_0()` | Smoke/streamer trail update |
| 8e | `_PROJLockUpdate_0()` | Missile seeker lock update |
| 9 | `_MPSend__YGXXZ()` | Multiplayer: send state packets |
| 10 | `_MPReceive__YGDXZ()` | Multiplayer: receive and apply remote state |
| 11 | `_MPCheckDisconnect__YGDXZ()` | Returns zero → exits loop (disconnect) |
| 12 | `FUN_0040F5D0(&_mainV)` | Unresolved — injects a fake key 0x3B00 if non-zero |
| 13 | `FUN_0040D7F0(&_mainV)` | Unresolved |
| 14 | `FUN_0040D810(&_mainV)` | Unresolved |
| 15 | `FUN_0040E960(&_mainV)` | Unresolved |
| 16 | `_ServiceSounds__YIXXZ()` | Audio: update positional sounds |
| 17 | `_T_Make_12(&_mainV, 0)` | Build render scene (skipped if IFM active) |
| 18 | `_G_SetClipBox_16(...)` | Set render clip rect to cockpit area |
| 19 | `_T_Render_4(&_mainV)` | Render 3D scene to backbuffer (target list built around this call) |
| 20 | `_WRLightUpdate_0()` | Update world light state |
| 21 | `_WRLensFlare_0()` | Lens flare post-pass |
| 22 | `_HUDDraw_4(0)` | Draw cockpit HUD overlay |
| 23 | `_G_SetFullClipBox_0()` | Restore full-screen clip |
| 24 | `FUN_0040EBA0(&_mainV)` | Unresolved — post-render step |
| 25 | `_FPSUpdate_0()` | Framerate counter update |
| 26 | Key read loop | `_GetKey()` / `_GetFakeKey_0()` — classifies Space (weapon) and Tab (gun) |
| 27 | `_CPDraw_8(key, ...)` | Draw control panel (cockpit or IFM variant) |
| 28 | `_G_Flush_4()` | Blit backbuffer to screen |
| 29 | `_SoundPoints__YIXXZ()` | Update 3D sound listener position |
| 30 | `_ChooseScore()` | Score evaluation tick |
| 31 | `_ScoreUpdate__YIXXZ()` | Score display update |
| 32 | `_SlewKey(key)` | Slew mode key handling |
| 33 | `_MPKey__YIGG_Z(key)` | Multiplayer key handling |
| 34 | `_FlightKey_4(key)` | Flight key dispatch (see below) |

**IFM mode** (`_ifmOn__3DA != 0`): the 3D render pipeline is replaced by a solid-color fill (`_G_URect_16`), and `_HUDDraw_4(1)` is called instead.

**Key dispatch from `_FlightKey_4`** (notable cases):

| Key code | Action |
|----------|--------|
| `0x1B` (Escape) | `_MPSetPaused__YIXD_Z(1)` |
| `0x43` | Toggle IFM if active, else `uVar3 = 0xFFFF` |
| `0x44` | `_HUDReprintMessages_0()` |
| `0x4D` | Toggle IFM on/off (`_ifmOn__3DA`) |
| `0x63` (99) | `_TIMESetCompression_4(...)` — cycles time compression: ½×, 1×, 2×, 4×, 8×, PAUSED |
| `0x1002` | End mission (single/campaign calls `_CallCampaignProc_4(6)`; multiplayer broadcasts and returns) |

**Time compression display strings** (from `?FlyingLoop@@YAXXZ`):

| `__timeCompression` | HUD message |
|---------------------|-------------|
| `0` | `"Time compression: 1X"` |
| `1` | `"Time compression: 2X"` |
| `2` | `"Time compression: 4X"` |
| `3` | `"Time compression: 8X"` |
| `-1` | `"Time compression: 1/2"` |
| `0x7FFF` | `"GAME PAUSED"` |

---

## 5. Per-Frame Object Dispatcher

### Object List

The object list is a flat array managed by `_OBJInit@4` / `_OBJAdd@8`. Key globals:

| Global | Role |
|--------|------|
| `DAT_004FFE34` / `DAT_00553828` | Base pointer / bump-allocator current pointer into the object pool |
| `DAT_00553840` | Pool capacity (bytes) — set to `300,000` by `_MISSIONInit1` |
| `_nextObjId` | Next slot index (starts at 1; 0 is unused) |
| `_objPtrs` | Array of pointers, indexed by object ID |
| `DAT_00553120` | Parallel array of object sizes (u16 per slot) |

**`_OBJInit@4` — `0x00491250`**: allocates the object pool with `_MMAllocPtr_8(param_1, 0x8000)`. Also initialises `_nextObjId = 1` and computes a network ID range based on `_thisComputer`.

**`_OBJAdd@8` — `0x004913E0`**: copies `param_2` bytes from `param_1` into the bump-allocated pool. Returns a u16 object ID. Hard caps: 799 objects when `_curScreen == 3`; 899 otherwise. Returns 0 on overflow.

**`_T_ObjList@8` — `0x004A7DF0`**: the object iterator used for spatial queries. Iterates IDs `1..nextObjId-1`. For each entry where `objPtr[+1] & 1` (alive flag) and `_InBounds_8` (position within a query rect) pass, invokes a callback function pointer. Returns early if the callback returns `'\0'`.

### `_ServiceObjects` (unresolved VA)

Called from `?FlyingLoop@@YAXXZ` when `_frameTicks > 0`. Full decompile was not included in the analysis output; its virtual address is unresolved from the output. It is the function that iterates all live objects and dispatches per-frame procs including `_GVProc` and the projectile proc.

### `_GVProc` — `0x00473DB0` (FA.SMS: `_GVProc`)

The per-frame dispatch function for ground vehicles and naval vessels (`.NT` objects). It is a proc-table accessor, not a loop itself — it is called once per object per frame by the object iterator.

```c
undefined * _GVProc(char param_1) {
    if (param_1 == 0x03) return _GVEventProc;      // event proc
    if (param_1 == 0x05) return &_NPCWeaponsProc;  // weapons proc
    return _OBJProc(param_1);                       // delegate to base
}
```

Dispatch codes:

| Code | Returned proc |
|------|--------------|
| `0x03` | `_GVEventProc` — event handler (see below) |
| `0x04` | `_OBJDamageProc__YAXPAUHIT_OBJ_DATA___Z` (via `_OBJProc`) |
| `0x05` | `_NPCWeaponsProc` — NPC weapon firing |
| `0x06` | `_OBJSayProc` (via `_OBJProc`) — speech/callout |
| other | `NULL` (via `_OBJProc`) |

Callers of `_GVProc` are listed in the analysis as "unresolved" — the cross-reference scan returned no direct callers within the analyzed address range. The call presumably originates inside `_ServiceObjects`.

### `_GVEventProc` — `0x00473F50` (FA.SMS: `_GVEventProc`)

Event handler for ground vehicle objects. Reads the entity state byte at `+0x100` (the primary per-frame state byte) and the entity class. Delegates waypoint navigation via `_GVDoCurrentWaypoint__YADXZ` and dispatches hit/damage events. References `_Dist_8`, `_COMaxSpeed_0`, `_CreateMove_52`, `_CreateMoveGoal_20`.

### `?GVDoCurrentWaypoint@@YADXZ` — `0x00473DE0`

Called from `_GVEventProc`. Advances the waypoint list via `_WPMaybeAdvance_0`, reads the next waypoint position with `_WPPos_8`, optionally blends toward the prior waypoint position (when waypoint flag bit 2 is set), then calls `_CreateMove_52` and `_CreateMoveGoal_20` to issue a movement command. Sets `DAT_0050CED8 = 0xC700`.

### `_OBJProc` — `0x00473BE0` (FA.SMS: `_OBJProc`)

Base proc-table accessor for static objects (`.OT`). Returns function pointers for event (`0x03`), damage (`0x04`), and speech (`0x06`) dispatch codes; returns `NULL` for all others.

### Projectile Dispatch — `0x004C1F50` (`_PROJProc`)

The analysis identifies the projectile dispatch function at `0x004C1F50`. No callers were found in the analyzed range — cross-reference resolution is unresolved from the output provided. The function symbol `_PROJProc` is confirmed in FA.SMS at this address.

### `_Kill@0` — `0x00473C10` (FA.SMS: `_Kill@0`)

Called when an object is destroyed. Key actions:

- If the killed object belongs to the local player's computer (`DAT_0050CE90 & 0x7F == _thisComputer`) and is not already marked dead: calls `_MISSIONAddScore_12` (-2 penalty), resolves the kill class from `DAT_0050CEF6`, and calls `_KillStats_12`.
- If the killed object is the player (`_playerId == _curId`): in single-player, calls `_SetScenarioEndTime_4(5)`; in multiplayer, broadcasts `"You have died"` / `"<name> has died"` via `_MPLifeNotify`.
- Sets the dead flag in `_DAT_0050CE81` (bit `0x2000`; also bit `0x102000` unless object type byte at `+0x85` is `0x05`).
- If `_cg` is 2 or 4: zeroes the cargo hold (`_HARDFindStore_12` walk over store type 7).

---

## 6. Frame Timing

### Variables

| Symbol | VA | Type | Role |
|--------|----|------|------|
| `_timerTicks` | unresolved (global) | `s32` | Raw timer tick counter — incremented by the timer thread |
| `_frameCounter` | `0x004EB738` | u32 | Frame counter (FA.SMS confirmed) |
| `_frameTicks` | unresolved (global) | `s32` | Ticks elapsed since last frame; gating value for object simulation |
| `_currentTime` | unresolved (global) | u16/u32 | Mission simulation time (in mission ticks); capped at `0x7F9A` |
| `__timeCompression` | unresolved (global) | `s16` | Time compression factor: 0=1×, 1=2×, 2=4×, 3=8×, -1=½×, `0x7FFF`=paused |
| `_constConv__1__cobraTime__YAJJ_Z_4JA` | unresolved (global) | `s32` | Cobra-time conversion constant (multiplied with `_timerTicks` delta to produce cobra-time units) |

### Cobra-Time Computation (movie loop)

In `?MainLoop@@YAGPAUGlobalData@@@Z`, elapsed cobra-time is computed as a 64-bit multiply then shifted:

```c
lVar6 = (longlong)(_timerTicks - iVar8) * (longlong)_constConv__1__cobraTime__YAJJ_Z_4JA;
cobra_time = (int)((ulonglong)lVar6 >> 0x20) << 0x10 | (uint)lVar6 >> 0x10;
```

This is a fixed-point 16.16 scale applied to the raw tick delta.

### Frame-Tick Gating (simulation loop)

`?FlyingLoop@@YAXXZ` gates the entire object simulation block behind `if (0 < _frameTicks)`. When time compression is set to `0x7FFF` (paused), `_frameTicks` evaluates to zero and the simulation is frozen while rendering continues. `_TIMEUpdate_0()` is called unconditionally each iteration to keep the clock current.

### `GetTickCount` Reference

`__imp__GetTickCount@0` is confirmed at import table address `0x00593550`. `_GetTickCount@0` at `0x004D70BE` is an internal wrapper. The Cobra movie player uses `timeGetTime()` (in `?CreateGameThread@@YAHXZ`) for its 30-second thread-startup timeout.

---

## 7. Shutdown Sequence

### `?EndGame@@YAXXZ` — `0x00476700` (FA.SMS: `?EndGame@@YAXXZ`)

Top-level shutdown called after the flight loop exits:

1. `FUN_004767D0` — restores screensaver setting via `SystemParametersInfoA(0x11, 1, ...)` if it was disabled at startup.
2. `TerminateThread(_hGameThread, 0)` + `CloseHandle` — if `_dwGameThreadID != 0`.
3. `TerminateThread(_hTimeThread, 0)` + `CloseHandle` — if `_dwTimeThreadID != 0`.
4. `_GameCleanup_0()` — full game state teardown (body unresolved from output).

### `?CleanupCobra@@YAGPAUGlobalData@@@Z` — `0x0046B530`

Symmetric inverse of `SetupCobra`:

```
CleanupCobra:
    CleanAudio   (stub — returns immediately)
    CleanVideo   (stub — returns immediately)
    CleanCobra   (closes LIB file handle, frees movie frame table buffer)
```

### `?CleanCobra@@YAGPAUGlobalData@@@Z` — `0x0046B0F0`

Calls `_LibClose(param_1+0xF2)` (close movie LIB file handle) and `_MMFreePtr_4(param_1+0xEA)` (free the frame index table allocated in `InitCobra`).

### `?CleanVideo@@YAGPAUGlobalData@@@Z` — `0x0046B4B0`

Returns 0. No-op in the analyzed build.

### `?CleanAudio@@YAGPAUGlobalData@@@Z` — `0x0046B4D0`

Returns 0. No-op in the analyzed build.

### `?MPMissionShutdown@@YGXXZ` — `0x0046C500`

Called at mission end. If `_thisComputer > 0` (not the host), restores `_gamePrefs` from the value sent by the host at mission init (`DAT_00546E00`).

---

## 8. Campaign / Mission Initialization

Mission initialization is split into two numbered phases (`Init1`, `Init2`) with optional multiplayer wrappers. There are two sets: `?_MISSIONInit1/2@@YGXXZ` (C++ mangled, cdecl) and `_MISSIONInit1/2@0` (C-calling-convention versions at adjacent addresses). The decompile shows them as functionally identical — the two sets are likely inlined duplicates or near-identical callers of the same logic.

### Phase 1: `?_MISSIONInit1@@YGXXZ` / `_MISSIONInit1@0` — `0x00480750` / `0x00480B40`

Resets all per-mission state and initializes subsystems. Actions in order:

| Step | Call / Action |
|------|--------------|
| RNG seed | `_randomSeed = _Rand16_0() ^ (u16)_TIMESystemTime_0() ^ (u16)_waitCounter` |
| Multiplayer | If `_numComputers > 1`: call `_MPMissionInit1__YGXXZ` |
| RNG init | `_InitRand_4(_randomSeed)` |
| Mission flags | Zero `_freeFlightMission`, `_playerCheating`, `_thisMissionGunsOnly`, `_killedMonster__3DA`, `_missionSucceeded`, `_fortMission`, `_fortWin` |
| Scenario limits | `_endScenarioTime = 0x7FFF`, `_endScenarioKills = 0x7FFFFFFF`, `_reviveAllowed = 0x7FFFFFFF`, `_reviveDelay = 0`, `_reviveDist = 10` |
| Score arrays | Zero `_playerKills`, `_playerDeaths`, `_playerDamage`, `_playerRevives`, `_playerKillRatio`, `_playerScores__3PAJA`, `_playerKillsHuman`, `_playerKillsAI`, `_playerDamageHuman`, `_playerDamageAI` (8 entries each) |
| Score state | Zero `_enemyScore__3JA`, `_friendlyScore__3JA`, `_scoreBy`, `_scoreGoal` |
| Entity IDs | Zero `_playerId`, `_doArmPlane`, `_playerWMId`, `_doSelectPlane`, `_slewId` |
| UI flags | Zero `_doBriefMap`, `_doBriefPaper`, `_cloudAlt`, `_missionDLLName__3PADA` |
| Nation sides | `memcpy(&_nationSides, &_defaultNationSides, 0x40)` |
| Wind | `_windH = _Rand_4(0xFFF0)`, `__windSpeed = _Rand_4(0x16) + 7` |
| `FUN_004809D0` | Unresolved — called immediately after wind init |
| MM alloc ID | `_mmAllocId__3EA = 1` |
| Subsystem inits (ordered) | `_T_Init_0`, `_T_InitDatabase_0`, `_FPSInit`, `_TIMEInit_12(0,0,0)`, `_OBJInit_4(300000)`, `_InitChain_0`, `_COLInit_0`, `_CTInit_0`, `_MSGInit_0`, `_SAYInit_0`, `_WNGInit_0`, `_GRPInit_0`, `_APInit_0`, `_PLANEInit_0`, `_ROInit_0`, `_PROJInit_0`, `_ZONEInit_0`, `_GRAPHICInit_0`, `FUN_00422840` |
| Player damage init | If `_playerId != 0`: `_GetCurObj_4(_playerId)` → `_DAMAGEInit_0()` → `_PutCurObj_0()` |
| Stats array | Zero `_stats` (0x978 dwords = ~9,696 bytes) |

`_OBJInit_4(300000)` allocates the 300,000-byte object pool, establishing the hard cap on total live-entity data.

### Phase 2: `?_MISSIONInit2@@YGXXZ` / `_MISSIONInit2@0` — `0x00480A30` / `0x00480B50`

Post-spawn finalization. Runs after all mission objects have been loaded and placed:

| Step | Call / Action |
|------|--------------|
| Side assignment | Iterate all objects (`1..nextObjId-1`); for each where `objPtr[+1] & 0x10 == 0`: call `_MAPSetSide_4(objPtr)` |
| Human scan | `_OBJFindHumans_0()` — identifies human-controlled slots |
| Alias setup | `_OBJAliasAll_12(0, 0, 0)`, `_OBJAliasForMulti_0()` |
| Time init | `_TIMEInit_12(0, _missionHours__3JA, _missionMinutes__3JA)` — sets mission start time |
| Scale max | `_G_SetScaleMax_8(0x140, 200)` |
| MC DLL load | If `_missionDLLName__3PADA != 0` and `_thisComputer == 0`: `_eventFilterProc = _RMAccess_8(&_missionDLLName__3PADA, 0x8000)` — loads the `.MC` mission condition evaluator DLL |
| Say init | `_SAYInit2_0()` |
| Score init | `_ChooseScoreInit()` |
| Airport | If `_playerId != 0`: `_APHomeAirport_0()` to identify home airport, call `_NextString_8(...)` for airport name |
| `FUN_004809D0` | Unresolved — called again before success check |
| Success check | `_MISSIONCheckSuccess_0()` |

### `?MPMissionInit1@@YGXXZ` — `0x0046C280`

Multiplayer Phase 1 wrapper. Only runs if `_numComputers > 1`. On host (`_thisComputer == 0`): loads the mission file via `_LoadFile_16(&_missionName, ...)` and broadcasts it to all peers via `FUN_0046C0A0`. On all machines: calls `_MPWaitEveryoneStatus__YGDJDDD_Z(0x17, ...)` to synchronize before proceeding.

### `?MPMissionInit2@@YGXXZ` — `0x0046C470`

Multiplayer Phase 2 wrapper. Only runs if `_numComputers > 1`. Host calls `_MPSendPrefs__YGXXZ` to broadcast `_gamePrefs`; peers copy the received prefs. Resets `_packetTicks__3PAJA` and `_packetTicksAdjust__3PAJA` arrays. Iterates all objects and calls `_MPPrepareForInterp__YGXPAUANGLE__J_Z(0, 0)` on each (interpolation state reset).

### Phase 3 and Medal Info

Two additional init phases follow:

**`_MISSIONInit3@0` — `0x00480B60`**: evaluates `_MISSIONSucceededForThisPlayer_0()`; sets `_home` and `DAT_00552FE0` on success, sets `DAT_00552FC8` on failure.

**`_MISSIONInitMedalInfo@0` — `0x00480B80`**: calls `_WNGPart_12` to check if the player has wingmen; sets `_playerHasWingmen` accordingly. Calls `_MISSIONSetCheating_0()` in both branches.

---

## Function Reference

All addresses are virtual addresses from FA.EXE (base `0x00400000`).

| VA | FA.SMS name | Role |
|----|-------------|------|
| `0x004D9D00` | `_WinMainCRTStartup` | PE entry point / CRT startup |
| `0x00476120` | `_WinMain@16` | Win32 WinMain; message loop |
| `0x004764B0` | `?InitApplication@@YAHPAX@Z` | Window class registration and creation |
| `0x00403700` | `?usnfmain@@YAXXZ` | Game's internal main; body partially unresolved |
| `0x00476660` | `?CreateGameThread@@YAHXZ` | Spawns game simulation thread |
| `0x00476700` | `?EndGame@@YAXXZ` | Tears down game and timer threads |
| `0x00436320` | `?StartGameThread@@YAKPAK@Z` | Game thread entry point |
| `0x0046B4E0` | `?SetupCobra@@YAGPAUGlobalData@@@Z` | Top-level subsystem init (Cobra → Video → Audio) |
| `0x0046AE10` | `?InitCobra@@YAGPAUGlobalData@@@Z` | Movie subsystem / GlobalData init |
| `0x0046B120` | `?InitVideo@@YAGPAUGlobalData@@@Z` | VESA/DirectDraw display init |
| `0x0046B4C0` | `?InitAudio@@YAGPAUGlobalData@@@Z` | Audio init stub (no-op) |
| `0x0046B530` | `?CleanupCobra@@YAGPAUGlobalData@@@Z` | Top-level subsystem shutdown |
| `0x0046B0F0` | `?CleanCobra@@YAGPAUGlobalData@@@Z` | Cobra / movie cleanup |
| `0x0046B4B0` | `?CleanVideo@@YAGPAUGlobalData@@@Z` | Video cleanup stub (no-op) |
| `0x0046B4D0` | `?CleanAudio@@YAGPAUGlobalData@@@Z` | Audio cleanup stub (no-op) |
| `0x0046B560` | `?MainLoop@@YAGPAUGlobalData@@@Z` | Cobra FMV playback loop |
| `0x0046BD90` | _(unnamed)_ | Per-frame key/mouse abort check (cobra loop) |
| `0x00404C70` | `?FlyingLoop@@YAXXZ` | In-flight simulation loop |
| `0x0046A370` | `_SMInit@0` | Symbol/script manager init |
| `0x0046A4C0` | `_SMShutdown@0` | Symbol/script manager shutdown |
| `0x00473BE0` | `_OBJProc` | Static object proc-table accessor |
| `0x00491250` | `_OBJInit@4` | Object pool allocation |
| `0x004913E0` | `_OBJAdd@8` | Add object to live list |
| `0x004A7DF0` | `_T_ObjList@8` | Spatial object iterator |
| `0x00473DB0` | `_GVProc` | Ground vehicle proc-table accessor |
| `0x00473DE0` | `?GVDoCurrentWaypoint@@YADXZ` | Ground vehicle waypoint navigation |
| `0x00473F50` | `_GVEventProc` | Ground vehicle event handler |
| `0x004C1F50` | `_PROJProc` | Projectile proc-table accessor |
| `0x00473C10` | `_Kill@0` | Object death handler |
| `0x00476880` | `_LimitFromLowSpeed@12` | Low-speed flight limit utility |
| `0x00476AE0` | `_MovePlane@0` | Player aircraft movement update |
| `0x00480750` | `?_MISSIONInit1@@YGXXZ` | Mission Phase 1 init (C++ form) |
| `0x00480A30` | `?_MISSIONInit2@@YGXXZ` | Mission Phase 2 init (C++ form) |
| `0x00480B40` | `_MISSIONInit1@0` | Mission Phase 1 init (C form) |
| `0x00480B50` | `_MISSIONInit2@0` | Mission Phase 2 init (C form) |
| `0x00480B60` | `_MISSIONInit3@0` | Mission Phase 3 — home/fail flag set |
| `0x00480B80` | `_MISSIONInitMedalInfo@0` | Wingman/cheating flag init |
| `0x0046C280` | `?MPMissionInit1@@YGXXZ` | Multiplayer mission Phase 1 wrapper |
| `0x0046C470` | `?MPMissionInit2@@YGXXZ` | Multiplayer mission Phase 2 wrapper |
| `0x0046C500` | `?MPMissionShutdown@@YGXXZ` | Multiplayer mission shutdown |
| `0x004EB738` | `_frameCounter` | Frame counter global |
| `0x004D70BE` | `_GetTickCount@0` | Internal GetTickCount wrapper |
| `0x00593550` | `__imp__GetTickCount@0` | GetTickCount IAT slot |
