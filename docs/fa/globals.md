# Global Variable Reference

Inventory of all named global variables recovered from the game executable.

> **Provenance:** Ghidra static analysis of the game executable with [FA.SMS](formats/SMS.md) symbols applied; data sourced from `DumpGlobals.csv` (`DumpGlobals.java` headless run). Confidence markers follow [spec-authoring.md](../spec-authoring.md): confirmed · inferred · unknown.

> **Referenced-globals rule:** the [game-executable reconstruction program](reconstruction.md)
> treats a data symbol as in-scope only when it is **referenced by code** (≥1 xref):
> the 10,313 such globals in `db/inventory/FA.EXE/globals.csv` (the local-only Ghidra
> inventory export — see [db/README.md](https://github.com/jomkz/fighters-codex/blob/main/db/README.md))
> are the mechanical universe a completed subsystem must name or explicitly waive. The
> ~48k zero-xref entries are mostly struct/array interiors named at their base and are
> not individually tracked.

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

## Globals by subsystem

**Generated from the [symbol database](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/)** — the named referenced globals of each completed subsystem (waived struct/array interiors live in the CSVs, not here). Detail per subsystem is on its own page.

<!-- BEGIN GENERATED: globals-registry -->

<!-- Generated by tools/check_status.py --write-matrix. Do not edit. -->

_Generated from [`db/symbols/`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/); each subsystem's detailed prose lives on its own page._

**Binary: `FA.EXE`**

### Network / multiplayer (NET/SER/UDP/MP)

[`network.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/network.csv) · [page](network.md) — 3 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x004EB608` | `thisComputer` | sms | imported by 1 shipped .MC overlay (#491); named at this VA by FA.SMS |
| `0x00520A1C` | `playerId` | sms | imported by 1 shipped .MC overlay (#491); named at this VA by FA.SMS |
| `0x00572568` | `querySocket` | re | master/query socket handle — set from the connect result in ?NET_StartQuery@@…, used for the broadcast host query, closed and reset to -1 by ?NET_MasterShutdown@@YAXXZ |

### HUD / cockpit

[`hud.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/hud.csv) · [page](hud.md) — 16 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x004EBD08` | `_hudTargetViewEnable` | re | enable flag guarding HUDDrawTargetView |
| `0x004EBD30` | `_hudPitchBarTable` | re | pitch-ladder bar table, 37 records x 3 int16 {pitch,dY,dX}; extent proven in the #455 close-out |
| `0x0052107C` | `_hudWarnExpireTick` | re | warning-message expiry tick vs _currentTicks |
| `0x005213AD` | `_hudLineHeight` | re | text row pitch between HUD print lines |
| `0x005213D2` | `_hudColor` | re | current HUD draw colour (G_SetColor) |
| `0x005213D4` | `_hudBitmap` | re | HUD offscreen bitmap handle (G_SetBitmap target / G_Blit source) |
| `0x005213D8` | `_hudShape` | re | 3D shape record rendered into the HUD by HUDDrawTargetView; extent proven in the #455 close-out |
| `0x00521498` | `_hudTargetViewCount` | re | element count for the HUDDrawTargetView blit loop |
| `0x00521614` | `_hudBlink1` | re | ~1Hz blink flag from _timerTicks |
| `0x00521620` | `_hudWarnText2` | re | secondary warning line buffer |
| `0x00521694` | `_hudMasterMode` | re | HUD weapon/master submode; ==2 selects landing (HUDDrawApproach) |
| `0x005216A0` | `_hudBlink2` | re | second blink-phase flag |
| `0x005216A4` | `_hudWarnText` | re | warning-message string pointer set by HUDSetWarning |
| `0x00521980` | `_hudFpmXCached` | re | cached _hudFpmX restored when not in a view transition |
| `0x00521D94` | `_hudFpmX` | re | flight-path-marker screen X; anchor for nearly all HUD symbology; extent proven in the #455 close-out |
| `0x00521D96` | `_hudFpmY` | re | flight-path-marker screen Y |

### Core shell / menu / dialog UI

[`shell-ui.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/shell-ui.csv) · [page](shell-ui.md) — 43 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x004EB620` | `gameMode` | sms | imported by 3 shipped .CAM overlays (#491); named at this VA by FA.SMS |
| `0x004EEA34` | `namesItems` | re | GetNames: the NAMES record array (alias of _items) (#492) |
| `0x004F6DB8` | `info2SavedIndex` | re | reference screen: remembered selection (category 1 only) (#492) |
| `0x004F6DBC` | `info2ForceClear` | re | clear-pane flag (ar_nopic case) (#492) |
| `0x004F6DC0` | `info2PicX` | re | photo blit x (#492) |
| `0x004F6DC2` | `info2PicY` | re | photo blit y (#492) |
| `0x004F6DC8` | `info2DragX` | re | 3-D drag anchor x (#492) |
| `0x004F6DCC` | `info2DragY` | re | 3-D drag anchor y (#492) |
| `0x00502170` | `yesString` | sms | localized dialog-button label; the table _yesString/_noString/_okString/_cancelString/_exitString is contiguous |
| `0x00502174` | `noString` | sms | localized dialog-button label |
| `0x00502178` | `okString` | sms | localized dialog-button label; IMPORTED BY 69 of the 92 shipped .DLG overlays (#491) |
| `0x0050217C` | `cancelString` | sms | localized dialog-button label; imported by 72 of the 92 shipped .DLG overlays (#491) |
| `0x00502184` | `exitString` | sms | localized dialog-button label; imported by 1 shipped .DLG overlay (#491) |
| `0x0052912C` | `namesCapacity` | re | GetNames: allocated NAMES record capacity (grows by 400) (#492) |
| `0x00529160` | `namesCount` | re | GetNames: NAMES records in _items (#492) |
| `0x00546B08` | `info2LastYaw` | re | INFO2 dirty-check: last drawn yaw (#492) |
| `0x00546B0A` | `info2LastPitch` | re | INFO2 dirty-check: last drawn pitch (#492) |
| `0x00546B0C` | `info2LastRoll` | re | INFO2 dirty-check: last drawn roll (#492) |
| `0x00546B10` | `info2Palette` | re | palette.PAL contents for the photo pages (#492) |
| `0x00546B14` | `info2LastPhoto` | re | INFO2 dirty-check: last drawn photo index (#492) |
| `0x00546B18` | `info2PhotoIndex` | re | current photo page index (#492) |
| `0x00546B1C` | `info2LightYaw` | re | 3-D view light heading (#492) |
| `0x00546B20` | `info2BaseDist` | re | ObjRadius*15 base view distance (#492) |
| `0x00546B24` | `info2PhotoPages` | re | number of <type>_N photo pages found (#492) |
| `0x00546B28` | `info2LastType` | re | INFO2 dirty-check: last drawn type record (#492) |
| `0x00546B2C` | `info2TypeRec` | re | selected type record (RMAccess of _items[n]) (#492) |
| `0x00546B30` | `info2TextPage` | re | current .INF text page (#492) |
| `0x00546B34` | `info2ViewMode` | re | view mode: 0 text, 1 3-D, 2 photos, 3-6 art, 7-9 video (#492) |
| `0x00546B38` | `info2FadePending` | re | first-draw flag: fade in instead of flush (#492) |
| `0x00546B3C` | `info2Zoom` | re | 3-D view distance (0x800..0x6400) (#492) |
| `0x00546B40` | `info2TypeName` | re | selected type filename (#492) |
| `0x00546B50` | `info2LastTextPage` | re | INFO2 dirty-check: last drawn text page (#492) |
| `0x00546B58` | `info2PaletteCopy` | re | screen palette copy for change detection (Info2SyncPalette) (#492) |
| `0x00546B60` | `info2Yaw` | re | 3-D view yaw (lo16) / pitch (hi16) (#492) |
| `0x00546B64` | `info2Roll` | re | 3-D view roll (#492) |
| `0x00546B68` | `info2Background` | re | the ar background PIC handle (#492) |
| `0x00546B6C` | `info2LastMode` | re | INFO2 dirty-check: last drawn view mode (#492) |
| `0x00546B70` | `info2Category` | re | active GetNames category mask (#492) |
| `0x00546B74` | `info2Index` | re | selected _items index (#492) |
| `0x00546B78` | `info2Dirty` | re | needs-redraw flag (#492) |
| `0x00546B7C` | `info2GlassesOn` | re | 3-D glasses active for the 3-D view (#492) |
| `0x00546B80` | `info2LastZoom` | re | INFO2 dirty-check: last drawn zoom (#492) |
| `0x00572008` | `singleMissionName` | re | the picked single-mission filename (SingleFilename) (#492) |

### View / camera & replay (VIEW)

[`view.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/view.csv) · [page](view.md) — 5 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x004EC420` | `viewModeTable` | re | view-mode dispatch table scanned by VIEWModeLookup; extent proven in the #455 close-out |
| `0x005223F0` | `replayWindowStart` | re | replay capture window start tick |
| `0x005223F4` | `replayWindowEnd` | re | replay capture window end tick |
| `0x00522400` | `replaySaveBuf` | re | saved-view replay buffer base (0x30 dwords copied in/out of the view); extent proven by the save/restore loops, which move 0x30 dwords |
| `0x005224C0` | `replayActive` | re | replay-active flag (set by VIEWReplayRecordGate, read by VIEWReplayPlayback) |

### Campaign / mission / pilot (MAP/CAM/MC/MM/PLT)

[`campaign.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/campaign.csv) · [page](campaign.md) — 14 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x004F8C7A` | `pilotCampaignsWon` | re | _campaignPilot+0xC2: ", "-joined completed-campaigns list (P.md) (#492) |
| `0x004F9937` | `campaignFileCopy` | re | _campaignPilot+0xD7F: active campaign .CAM filename (P.md) (#492) |
| `0x004F9944` | `campaignDisplayName` | re | _campaignPilot+0xD8C: campaign display name (P.md) (#492) |
| `0x004FA818` | `campaignStores` | re | _campaignPilot+0x1C60: 50 x 16-byte ordnance pool (P.md) (#492) |
| `0x004FAB38` | `pilotMissionsFlown` | re | _campaignPilot+0x1F80 (P.md stats block) (#492) |
| `0x004FB1A8` | `missionName` | sms | imported by 6 shipped .CAM overlays (#491); named at this VA by FA.SMS |
| `0x0050CFD1` | `ejectNextTime` | re | ejection seat: next state deadline, in _currentT ticks — EJECTAdd sets _currentT+0xC, EJECTMoveProc re-arms at +1 and parks it at 0x7FFF when the sequence ends |
| `0x0050CFD3` | `ejectAngle` | re | ejection seat: seat attitude — passed as the ANGLE* first argument of ?MPPrepareForInterp@@YGXPAUANGLE@@J@Z, which types it, and slewed toward 0x7FF8 by _Slew@16 |
| `0x0050CFD9` | `ejectSpeed` | re | ejection seat: seat velocity — EJECTAdd seeds it from the ejecting aircraft, EJECTMoveProc drains it by _LMultDiv256(speed, _serviceTicks) each tick and drives _Move3d with the remainder, clamping at 0 |
| `0x0054E418` | `campaignFailures` | sms | imported by 6 shipped .CAM overlays (#491); named at this VA by FA.SMS |
| `0x0054E468` | `playerDead` | sms | imported by 6 shipped .CAM overlays (#491); named at this VA by FA.SMS |
| `0x00551660` | `campaignSucceeded` | sms | imported by 6 shipped .CAM overlays (#491); named at this VA by FA.SMS |
| `0x00552804` | `campaignFailed` | sms | imported by 6 shipped .CAM overlays (#491); named at this VA by FA.SMS |
| `0x005528E0` | `currentTime` | sms | imported by 12 shipped .MC overlays (#491); named at this VA by FA.SMS |

### Collision (COL)

[`collision.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/collision.csv) · [page](collision.md) — 28 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x00536728` | `_colSelfObj` | re | querying entity ptr (predicted-pos cache follows) |
| `0x00536730` | `_colRayEndX` | re | query ray end X; extent proven in the #455 close-out |
| `0x0053673C` | `_colObjPart` | re | hit child-box/part id |
| `0x00536740` | `_colObjCount` | re | registered collidable count this frame |
| `0x00536748` | `_colBoundMaxX` | re | swept AABB max X |
| `0x0053674C` | `_colBoundMaxY` | re | swept AABB max Y |
| `0x00536750` | `_colBoundMaxZ` | re | swept AABB max Z |
| `0x00536758` | `_colBoundMinX` | re | swept AABB min X |
| `0x0053675C` | `_colBoundMinY` | re | swept AABB min Y |
| `0x00536760` | `_colBoundMinZ` | re | swept AABB min Z |
| `0x00536764` | `_colBlockType` | re | blocking-hit terrain-leaf/structure type |
| `0x00536768` | `_colStructCount` | re | registered structure count |
| `0x0053676C` | `_colBlockDist` | re | nearest blocking-hit distance (sentinel 0x7FFFFFFF) |
| `0x00536770` | `_colStructList` | re | structure id list (word[0x1c2]); extent proven in the #455 close-out |
| `0x00536AF8` | `_colObjAngleH` | re | object-hit angle H; extent proven in the #455 close-out |
| `0x00536B00` | `_colClosureRadius` | re | closure radius (query param) |
| `0x00536B04` | `_colGearHeight` | re | gear/ground clearance (COLInfo+8 <<8) |
| `0x00536B0C` | `_colSelfSide` | re | IFF side bit of the querying object |
| `0x00536B10` | `_colObjList` | re | collidable id list (word[900]); extent proven in the #455 close-out |
| `0x00537218` | `_colBlockAngleH` | re | blocking-hit angle H; extent proven in the #455 close-out |
| `0x00537220` | `_colRayStartX` | re | query ray start X; extent proven in the #455 close-out |
| `0x0053722C` | `_colObjId` | re | nearest hit object id |
| `0x00537230` | `_colBlockPosX` | re | blocking-hit pos X; extent proven in the #455 close-out |
| `0x0053723C` | `_colSelfId` | re | querying entity id |
| `0x00537240` | `_colObjPosX` | re | object-hit pos X; extent proven in the #455 close-out |
| `0x0053724C` | `_colTargetId` | re | single-target id (excluded from broad-phase) |
| `0x00537250` | `_colObjDist` | re | nearest object-hit distance (sentinel 0x7FFFFFFF) |
| `0x00537254` | `_colObjBox` | re | hit box pointer |

### Sound / music (incl. WAIL32)

[`sound.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/sound.csv) · [page](sound.md) — 5 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x004EB5F0` | `_musicOn` | re | music/MIDI subsystem-available master flag |
| `0x004F3C10` | `_warnSndVol` | re | initial-volume table for _warnSnd; extent proven in the #455 close-out |
| `0x004F3C20` | `_warnSndName` | re | filename table for _warnSnd (IR1.11K, ...); extent proven in the #455 close-out |
| `0x004F3CCC` | `_curShellMusic` | re | current shell-music category; extent proven in the #455 close-out |
| `0x005380B8` | `_warnSnd` | re | RWR/IR threat-warning tone channel table (8 entries); extent proven in the #455 close-out |

### Memory & resource managers (MM/RM)

[`memory-resource.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/memory-resource.csv) · [page](memory-resource.md) — 12 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x004F3DBC` | `mm_initialized` | sms | MM one-time init guard (bool); set by MMInit cleared by MMShutdown |
| `0x004F3DC0` | `mm_handles` | sms | base of T_HANDLE[count] pool (operator_new'd in MMInit) |
| `0x004F3DC4` | `mm_used_list` | sms | head of allocated-handle intrusive list (next@+0 prev@+4) |
| `0x004F3DC8` | `mm_unused_list` | sms | head of free-handle intrusive list |
| `0x004F3DCC` | `mmAllocIdSp` | re | depth/index of the alloc-id save-stack (@0x538250) for MMPushAllocId/MMPopAllocId [was DAT_004f3dcc] |
| `0x00503E30` | `resList` | sms | RES_LIST[1400] resource registry spanning 0x503E30..0x50A618 (1400*0x13=0x67E8) |
| `0x0050A618` | `rmInitialized` | re | RM init flag; its ADDRESS doubles as the resList end-marker (scans stop at 0x50A617) [was DAT_0050a618] |
| `0x0050A61C` | `rmNotifyEnabled` | re | guards RMNotify; cleared during RMFree/RMFreeAllId bulk operations to prevent reentrancy [was DAT_0050a61c] |
| `0x00538248` | `mm_page_size` | sms | GetSystemInfo dwPageSize; VirtualAlloc-vs-GlobalAlloc threshold (shared reader: sound) |
| `0x0053824C` | `mmAllocId` | sms | current alloc-id tag stamped on new handles/RES_LIST rows; widely read (campaign/network/shell-ui) but MM-owned |
| `0x00573208` | `brsColorProcess` | sms | bitmap color-process mode set by SetupBitmapAccess |
| `0x00573248` | `resCache` | re | 20-slot x 8-byte LRU find-cache {RES_LIST* ; timerTicks} spanning 0x573248..0x5732E8 [was DAT_00573248]; extent proven by the init loop, which clears 0x28 dwords = 160 bytes = the 20 x 8-byte slots |

### Cockpit sensors (radar / IR / RWR)

[`cockpit-sensors.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/cockpit-sensors.csv) · [page](cockpit-sensors.md) — 9 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x005387D8` | `_radarMode` | re | cockpit sensors: current radar mode (0=off, 2/3 air scan variants, 4 air-to-ground/wide) — the value the detection predicates branch on (#486) |
| `0x005387F0` | `_rwrLaunchWarn` | re | cockpit sensors: RWR missile-launch warning flag (#486) |
| `0x00538800` | `_scopeYScale` | re | cockpit sensors: MFD vertical scale shift (from screen resolution, WPCInit) (#486) |
| `0x00538808` | `_scopeXScale` | re | cockpit sensors: MFD horizontal scale shift (WPCInit) (#486) |
| `0x00539E4C` | `_rwrNextRefresh` | re | cockpit sensors: next tick the RWR display re-renders (CPDrawRWR, +0x40) (#486) |
| `0x00539E58` | `_rwrScopeBuf` | re | cockpit sensors: the RWR contact buffer filed by CPScopeInsert and drawn by CPDrawRWR (#486) |
| `0x0053BEA8` | `_radarScopeBuf` | re | cockpit sensors: the radar+IR contact buffer (0x23-dword records) filed by CPScopeInsert and walked by CPDrawRadarScope (#486) |
| `0x0053DA08` | `_radarEmitter` | re | cockpit sensors: the active radar/seeker emitter object driving the scope this frame (#486) |
| `0x0053DA10` | `_rwrNextScan` | re | cockpit sensors: next tick the RWR re-scans its contact buffer (+0x300) (#486) |

### .SEQ scripted-cutscene / sequence player (SEQ)

[`seq.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/seq.csv) · [page](seq.md) — 25 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x00540318` | `seqFontArray` | sms | SEQFNT font pool base - init by SeqInit |
| `0x0054039C` | `seqFonts` | sms | active font list head |
| `0x005403A0` | `seqFadeLen` | sms | palette fade duration in fixed ticks |
| `0x005403A8` | `seqGrArray` | sms | SEQGR graphic-node pool base |
| `0x00540768` | `seqSynch` | sms | synch flag - current command is a wait/synch point |
| `0x00540770` | `curSeq` | sms | index of the sequence slot being ticked |
| `0x00540778` | `seqTextArray` | sms | SEQTXT text-node pool base |
| `0x005407D8` | `videoState` | sms | video/AVI playback state gating SEQvideo (shared with video path) |
| `0x005407DC` | `seqIgnoreTicks` | sms | accumulated load time subtracted from the sequence clock |
| `0x005407E0` | `seqGrList` | sms | SEQGR free-list head |
| `0x005407E4` | `seqFading` | sms | fade direction 0 none 1 in -1 out |
| `0x005407E8` | `seqFlipState` | sms | page-flip/dirty state for the render pass |
| `0x005407EC` | `seqSndPriority` | sms | current sound priority |
| `0x005407F0` | `seqLoadPtr` | sms | write cursor into the compiled command buffer |
| `0x005407F4` | `seqFadeStart` | sms | tick at which the current fade started |
| `0x00541278` | `seqSndLink` | sms | linked sound handle |
| `0x0054127C` | `seqGraphics` | sms | active SEQGR display-list ring head |
| `0x00541280` | `seqFontList` | sms | SEQFNT free-list head |
| `0x00541284` | `seqLabelList` | sms | SEQLBL free-list head |
| `0x00541288` | `lineChar` | sms | interpreter cursor into the current command line |
| `0x00541290` | `seqLine` | sms | expanded current-line scratch buffer |
| `0x00541394` | `seqLabelPtr` | sms | label node just defined by SeqParseLabel |
| `0x00541398` | `seqTextList` | sms | SEQTXT free-list head |
| `0x005413A0` | `seqList` | sms | SEQUENCE slot array base - stride 0x38 - up to 3-4 slots |
| `0x00541450` | `seqLabels` | sms | SEQLBL node pool base |

### Renderer & rasterizer (GG/G_)

[`renderer.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/renderer.csv) · [page](renderer.md) — 14 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x004F6CA4` | `_blitDestX` | re | destination X of the present blit rect (GG_FlushShaken/FlushDirtyLines) |
| `0x004F6CA8` | `_blitDestY` | re | destination Y of the present blit rect; offset by screen shake |
| `0x004F6CAC` | `_shakeParity` | re | current screen-shake parity toggle (GG_FlushShaken) |
| `0x0050C5C4` | `glassesBitmap` | re | stereo-glasses overlay bitmap handle — allocated by _G_AllocBitmap@12 in ?GLASSESInit@@YGXXZ, freed in ?GLASSESShutDown@@YGXXZ |
| `0x0050C8D8` | `_lensFlareTable` | re | WR: lens-flare disc table {offset%, radius, colour} (WRLensFlare) (#493) |
| `0x0050C8DC` | `_lensFlareCount` | re | WR: number of lens-flare discs to draw (#493) |
| `0x00580D90` | `_curEyeLayer` | re | WR: the LAYER struct covering the eye altitude this frame (WRUpdate) (#493) |
| `0x00580DA4` | `_eyeAltitude` | re | WR: current eye altitude >> 8 (WRUpdate / WRSetRemaps interpolation) (#493) |
| `0x00583DBE` | `_sunScreenY` | re | WR: projected sun screen Y for the lens flare (WRLensFlare) (#493) |
| `0x005843C0` | `_wrHazeActive` | re | WR: nonzero when the eye is under a haze/cloud deck (drives WRSetRemaps blending) (#493) |
| `0x005843CC` | `_wrLayerAbove` | re | WR: LAYER just above the eye (interpolation bound) (#493) |
| `0x005843D4` | `_wrLayerBelow` | re | WR: LAYER just below the eye (interpolation bound) (#493) |
| `0x0058F0E8` | `overflowQuotient` | re | divide-overflow handler scratch: the saturated quotient magnitude, computed as (mask ^ 0x70000000) - mask. Reached through the _overflow_ptr slot the raster inner loops install (renderer.md) |
| `0x0058F0F4` | `overflowSignMask` | re | divide-overflow handler scratch: the sign mask (arithmetic >> 0x1F of the dividend XOR divisor) that gives _overflowQuotient its sign |

### Wingman / group AI (WNG/GRP)

[`wingman.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/wingman.csv) · [page](wingman.md) — 1 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x00546AB8` | `_wmNameBuf` | re | scratch buffer for WNGName/GRPName output |

### Object / entity system & shape selection

[`objects.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/objects.csv) · [page](objects.md) — 31 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x004F6FBC` | `curId` | sms | imported by 1 shipped .MC overlay (#491); named at this VA by FA.SMS |
| `0x004F6FDC` | `_curObjStackTop` | re | PushCurObj/PopCurObj nesting stack for GetCurObj |
| `0x004F6FE0` | `_cmdBufWritePtr` | re | command-buffer write cursor (WriteCmdBuf*/AllocCmdBuf) |
| `0x004FFE34` | `_objArena` | re | entity arena base (MMAllocPtr in OBJInit; shrunk by OBJStopAdding) |
| `0x0050CCC8` | `takeoffPattern` | re | autopilot takeoff-pattern record — field 0 holds &?APTakeoff@@YGDXZ, registered with the autopilot dispatcher by _APAdd@4 from ?STRIPAddProc@@YAJXZ |
| `0x0050CE80` | `cg` | sms | the current-entity mirror. FA.SMS names this ONE address SIX ways -- _cg _cj _cn _co _cp _curThing -- the class-typed views of the same block, matching the OBJ/NPC/PLANE/PROJ hierarchy the .OT/.NT/.PT/.JT records declare (#454). The .MC overlays import it as _co, _cp and _cn; db keys symbols by VA, so the aliases live here rather than in rows of their own (#491) |
| `0x0050D0B7` | `cmDispenser` | re | countermeasure dispenser state — low 7 bits are the remaining count (decremented per launch), bit 0x80 selects chaff vs flare; read by _NPCWeaponsProc and _PLANEEventProc |
| `0x0050D0E5` | `sayLastComment` | re | wingman chatter: id of the last line spoken, compared against the new candidate so a line never repeats back-to-back (_PLANECommentProc) |
| `0x0050D0E6` | `sayLastStress` | re | wingman chatter: snapshot of the pilot g-stress byte, compared against the current value to gate the grunt lines (_PLANECommentProc) |
| `0x0050D0E7` | `sayHighGTicks` | re | wingman chatter: high-g tick count this minute — 0x12 triggers "Ease up on the stick", 0x14 a grunt; reset on the minute rollover (_PLANECommentProc) |
| `0x0050D0E8` | `sayCommentMinute` | re | wingman chatter: current minute bucket (_currentTime / 0x3C); a change resets _sayHighGTicks (_PLANECommentProc) |
| `0x0050D0EA` | `sayLastAngleOff` | re | wingman chatter: last angle-off-nose snapshot — the angle must change before "Approaching target" is repeated (_PLANECommentProc) |
| `0x0050D0EC` | `sayCommentUntil` | re | wingman chatter: per-line cooldown deadline in _currentTime ticks; chatter is suppressed while _currentTime < this (_PLANECommentProc) |
| `0x0052253D` | `damageQuietUntil` | re | damage: cooldown deadline in _currentTime ticks — _DAMAGEInit@0 parks it at 0x7FFF (disabled) and _DAMAGEUpdate@0 gates on it; also read as the end-of-mission grace check by _PLANECommentProc |
| `0x00546B94` | `_curObjSize` | re | byte size of the current entity mirror; = type +0x03 + 0xDE for classes 0/2/4/6 |
| `0x00546B9C` | `_curTypeSize` | re | byte size of the current type-record mirror (from type +0x01) |
| `0x00546BA4` | `_lastPadlockId` | re | previous padlock/tracked id, compared by CheckForEvents1/2 |
| `0x00546BA8` | `_requeueChain` | re | objects serviced this frame; ChainMergeSorted folds it back into chainStart |
| `0x00546BB8` | `_lastCurZ` | re | previous frame Z of current object (CheckForEvents1); extent proven in the #455 close-out |
| `0x00552FC8` | `sayFailedSent` | re | wingman chatter: mission-failed line already spoken (set once, gates the failure callout); cleared by _SAYInit2@0 |
| `0x00552FDC` | `sayChatterUntil` | re | wingman chatter: global chatter cooldown deadline gating the top of _PLANECommentProc; re-armed by @SAYRearmMessage@8 |
| `0x00552FE0` | `saySucceededSent` | re | wingman chatter: mission-succeeded line already spoken; suppresses further home-approach callouts. Cleared by _SAYInit2@0 |
| `0x00552FE8` | `sayAlmostHomeUntil` | re | wingman chatter: "Almost home" cooldown deadline — re-armed to _currentTime + 4 each time it fires (_PLANECommentProc) |
| `0x00553120` | `_objSizes` | re | word[900] per-id entity size table (OBJAdd/OBJSubtract); ends at _objArenaNext; element width from OBJAdd (*(short*)(&_objSizes + id*2)); extent from OBJInit, which clears 0x1C2 dwords = 1800 bytes = 900 u16 |
| `0x00553828` | `_objArenaNext` | re | bump cursor into the entity arena (OBJAdd memcpy target) |
| `0x0055382C` | `_tempAliasBase` | re | lowest temp-alias id: (-0x14 - thisComputer)*1000 - 999 |
| `0x00553830` | `_tempAliasMax` | re | highest temp-alias id: (-0x14 - thisComputer)*1000; OBJTempAlias wraps to base past it |
| `0x00553834` | `_multiAliasCursor` | re | OBJAliasForMulti/OBJNextAliasForMulti iteration cursor |
| `0x0055383C` | `_tempAliasNext` | re | next temp alias returned by OBJTempAlias (negative per-computer id band) |
| `0x00553840` | `_objArenaSize` | re | arena byte capacity (OBJInit parameter), bounds OBJAdd |
| `0x00571420` | `wasOnGround` | re | previous-frame ground contact, fed to _PLANEUpdateJustLanded@8 and then refreshed from _OnTheGround@0 — the edge that makes a landing "just landed" exactly once |

### AI interpreter (CT)

[`ai.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/ai.csv) · [page](ai-interpreter.md) — 8 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x00546BC0` | `_ctCompareActor` | re | actor slot used by CTVarDiff to evaluate attributes under the other object |
| `0x00546BC8` | `_ctState` | re | 0x80-byte CT execution state block copied by CTSaveState/CTRestoreState (vars+stack+IP+base+name+line+priority); extent proven by CTInit, which clears 0x20 dwords = 0x80 bytes; interior not mapped, so bytes not dwords |
| `0x00546C48` | `_ctPrintBuf` | re | HUD message text staged by CTDo_print/printnum, flushed on loop exit |
| `0x00546C8C` | `_ctCheckPass` | re | validate/dry-run flag: skips branches and side effects, enables the stack-imbalance check |
| `0x00546C90` | `_ctActionTaken` | re | set 1 when a CTDo action fires; returned by CTExecProgram |
| `0x00546C94` | `_ctActorObj` | re | pointer to the current actor entity record |
| `0x00546C98` | `_ctHalt` | re | halt flag; the CTExecProgram loop runs while *IP!='%' and !_ctHalt |
| `0x00546CA4` | `_ctExecuting` | re | re-entry guard set across CTExecProgram |

### Input — joystick / serial / modem

[`input.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/input.csv) · [page](input.md) — 6 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x004ECE40` | `fakeKeyCount` | re | depth of the 8-entry synthetic-key stack (PutFakeKey/GetFakeKey) (#492) |
| `0x004EE348` | `visConfig` | re | the 57-byte view/visibility block loaded from vis240.SEE by InitPlayerControl; handed to PROJInFOV (#492) |
| `0x004EE384` | `slewSpeed` | re | slew step per keypress; keypad-0 doubles, keypad-. halves (min 0x100) (#492) |
| `0x00522C18` | `lastThrottle` | re | analog-throttle hysteresis state: the previous smoothed reading — ?PotThrottle@@YAGFG@Z keeps the old value when the new one moves less than 3 counts, and otherwise latches the new one |
| `0x00522C1C` | `slewListEnd` | re | end pointer for the SlewObjListCollect id list (#492) |
| `0x00522C24` | `slewListCursor` | re | write cursor for the SlewObjListCollect id list (#492) |

### Weapons — projectiles / seekers / ECM (PROJ)

[`weapons.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/weapons.csv) · [page](weapons.md) — 6 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x0058F0F8` | `_projBestTargetCost` | re | seeker-search best-cost accumulator (init 0x7FFFFFFF) |
| `0x0058F10C` | `_projBestTargetId` | re | seeker-search result id |
| `0x0058F110` | `_projPkPenalty` | re | running hit-chance (Pk) multiplier |
| `0x0058F118` | `_projNameBuf` | re | weapon display-name buffer (PROJBuildName) |
| `0x0058F180` | `_projSeekerList` | re | seeker-parameter array from HARDBestSeekers; extent proven in the #455 close-out |
| `0x0058F1D4` | `_projInboundWarnT` | re | inbound-missile warning throttle |

### Startup / Phar Lap DOS extender / config

[`startup.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/startup.csv) · [page](startup.md) — 40 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x0051E750` | `__umaskval` | sms | MSVC CRT global (xrefs=1) |
| `0x0051E758` | `__winver` | sms | MSVC CRT global (xrefs=1) |
| `0x0051E75C` | `__winmajor` | sms | MSVC CRT global (xrefs=1) |
| `0x0051E760` | `__winminor` | sms | MSVC CRT global (xrefs=2) |
| `0x0051E770` | `__environ` | sms | MSVC CRT global (xrefs=15) |
| `0x0051E778` | `__wenviron` | sms | MSVC CRT global (xrefs=6) |
| `0x0051E788` | `__exitflag` | sms | MSVC CRT global (xrefs=1) |
| `0x0051E78C` | `__C_Termination_Done` | sms | MSVC CRT global (xrefs=1) |
| `0x0051E790` | `__C_Exit_Done` | sms | MSVC CRT global (xrefs=2) |
| `0x0051E7A0` | `__NLG_Destination` | sms | MSVC CRT global (xrefs=2); extent proven in the #455 close-out |
| `0x0051E7BC` | `__adjust_fdiv` | sms | MSVC CRT global (xrefs=1) |
| `0x0051E7C0` | `__FPinit` | sms | MSVC CRT global (xrefs=1) |
| `0x0051E7D0` | `__aenvptr` | sms | MSVC CRT global (xrefs=5) |
| `0x0051E7D8` | `__aexit_rtn` | sms | MSVC CRT global (xrefs=1) |
| `0x0051E7F0` | `__locktable` | sms | MSVC CRT global (xrefs=9); extent proven in the #455 close-out |
| `0x0051E8B0` | `__pctype` | sms | MSVC CRT global (xrefs=38) |
| `0x0051E8B4` | `__pwctype` | sms | MSVC CRT global (xrefs=3) |
| `0x0051EC78` | `__cfltcvt_tab` | sms | MSVC CRT global (xrefs=2) |
| `0x0051EDF8` | `__mbctype` | sms | MSVC CRT global (xrefs=7); extent proven in the #455 close-out |
| `0x0051F010` | `__stdbuf` | sms | MSVC CRT global (xrefs=1); extent proven in the #455 close-out |
| `0x0051F2A8` | `__cflush` | sms | MSVC CRT global (xrefs=2) |
| `0x0051F2B0` | `__XcptActTab` | sms | MSVC CRT global (xrefs=1); extent proven in the #455 close-out |
| `0x0051F328` | `__First_FPE_Indx` | sms | MSVC CRT global (xrefs=4) |
| `0x0051F32C` | `__Num_FPE` | sms | MSVC CRT global (xrefs=4) |
| `0x0051F334` | `__XcptActTabCount` | sms | MSVC CRT global (xrefs=2) |
| `0x0051F3F8` | `__adbgmsg` | sms | MSVC CRT global (xrefs=2) |
| `0x00520470` | `__commode` | sms | MSVC CRT global (xrefs=2) |
| `0x00520474` | `?__pInconsistency@@3P6AXXZA` | sms | MSVC CRT global (xrefs=1) |
| `0x00520478` | `__newmode` | sms | MSVC CRT global (xrefs=3) |
| `0x00520578` | `__alternate_form` | sms | MSVC CRT global (xrefs=17) |
| `0x0052057C` | `__no_lead_zeros` | sms | MSVC CRT global (xrefs=20) |
| `0x005205E0` | `__timezone` | sms | MSVC CRT global (xrefs=7) |
| `0x005205E4` | `__daylight` | sms | MSVC CRT global (xrefs=3) |
| `0x005205E8` | `__dstbias` | sms | MSVC CRT global (xrefs=2) |
| `0x00520670` | `__tzname` | sms | MSVC CRT global (xrefs=4) |
| `0x00591A60` | `?_pnhHeap@@3P6AHI@ZA` | sms | MSVC CRT global (xrefs=1) |
| `0x00591B24` | `__crtheap` | sms | MSVC CRT global (xrefs=11) |
| `0x00591C30` | `__nhandle` | sms | MSVC CRT global (xrefs=18) |
| `0x00592C40` | `__nstream` | sms | MSVC CRT global (xrefs=6) |
| `0x00592C4C` | `__acmdln` | sms | MSVC CRT global (xrefs=5) |

**Binary: `WAIL32.DLL`**

**Binary: `IP.EXE`**

**Binary: `CDRVDL32.DLL`**

**Binary: `CDRVHF32.DLL`**

**Binary: `CDRVXF32.DLL`**

**Binary: `COMMSC32.DLL`**

**Binary: `MSAPI.DLL`**

### Matchmaking / internet-play client (MSAPI)

[`msapi.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/msapi.csv) · [page](matchmaking.md) — 12 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x1001D098` | `ms_running` | re | Running flag: set in initializeMS, cleared by closeMS / ms_disconnect. |
| `0x1001D09C` | `ms_conn_state` | re | Connection-state/idle flag toggled around operations; set when the link is torn down. |
| `0x1001D0A0` | `ms_list_dirty` | re | Game-list-dirty flag: resetMSfilter sets it; requestMSgame refetches when set and clears it when the list is exhausted. |
| `0x10020BD0` | `ms_recv_thread` | re | MFC CWinThread* background receive pump (AfxBeginThread, CREATE_SUSPENDED); m_hThread at +0x28, Resume/Suspended by the login exports. |
| `0x10020BD4` | `ms_game_list_tail` | re | Game-list append cursor (last node); advanced as 'P' records arrive. |
| `0x10020BD8` | `ms_game_list_head` | re | Game-list head sentinel node (0x24 bytes; allocated in initializeMS). |
| `0x10020BDC` | `ms_game_selected` | re | Current game-list selection/iteration node (walked by requestMSgame, reset by resetMSfilter). |
| `0x10020BFC` | `ms_socket` | re | Connected TCP socket handle (AF_INET/SOCK_STREAM); first arg to every send/recv; 0 when not connected. |
| `0x10020C08` | `ms_host_cookie` | re | Host-mode cookie/handle set by loginMShost, cleared by loginMSPlayer. |
| `0x10020C10` | `ms_record_size` | re | Negotiated record size in bytes for game/player records (send/recv payload length). |
| `0x10020C14` | `ms_list_inited` | re | Game-list-initialised flag (set in initializeMS; guards teardown in closeMS). |
| `0x10020C18` | `ms_game_count` | re | Number of games accumulated in the list (incremented per 'P' record, reset by resetMSfilter). |

<!-- END GENERATED: globals-registry -->

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

The complete list of the 1,955 named globals — those carrying an assigned name
(`USER_DEFINED`, applied from `db/`, or `IMPORTED`, from FA.SMS), with Ghidra's
auto-analysis labels (`switchD_`/`caseD_`/`s_`/`DAT_`) excluded — is regenerated
into `$FA_PROJECT/output/DumpGlobals_named.csv` by `DumpGlobals.java`, with columns:
`address, name, size_bytes, data_type, xref_count, first_writer`

The raw full export (all 58,517 data symbols, including switch tables and unnamed
data) is the sibling `$FA_PROJECT/output/DumpGlobals.csv`. Both are local-only
Ghidra output (never committed, [#342](https://github.com/jomkz/fighters-codex/issues/342));
the counts track the current canonical `fa-re` project.
