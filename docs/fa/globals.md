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

### HUD / cockpit

[`hud.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/hud.csv) · [page](hud.md) — 16 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x004EBD08` | `_hudTargetViewEnable` | re | enable flag guarding HUDDrawTargetView |
| `0x004EBD30` | `_hudPitchBarTable` | re | pitch-ladder bar table, 37 records x 3 int16 {pitch,dY,dX} |
| `0x0052107C` | `_hudWarnExpireTick` | re | warning-message expiry tick vs _currentTicks |
| `0x005213AD` | `_hudLineHeight` | re | text row pitch between HUD print lines |
| `0x005213D2` | `_hudColor` | re | current HUD draw colour (G_SetColor) |
| `0x005213D4` | `_hudBitmap` | re | HUD offscreen bitmap handle (G_SetBitmap target / G_Blit source) |
| `0x005213D8` | `_hudShape` | re | 3D shape record rendered into the HUD by HUDDrawTargetView |
| `0x00521498` | `_hudTargetViewCount` | re | element count for the HUDDrawTargetView blit loop |
| `0x00521614` | `_hudBlink1` | re | ~1Hz blink flag from _timerTicks |
| `0x00521620` | `_hudWarnText2` | re | secondary warning line buffer |
| `0x00521694` | `_hudMasterMode` | re | HUD weapon/master submode; ==2 selects landing (HUDDrawApproach) |
| `0x005216A0` | `_hudBlink2` | re | second blink-phase flag |
| `0x005216A4` | `_hudWarnText` | re | warning-message string pointer set by HUDSetWarning |
| `0x00521980` | `_hudFpmXCached` | re | cached _hudFpmX restored when not in a view transition |
| `0x00521D94` | `_hudFpmX` | re | flight-path-marker screen X; anchor for nearly all HUD symbology |
| `0x00521D96` | `_hudFpmY` | re | flight-path-marker screen Y |

### View / camera & replay (VIEW)

[`view.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/view.csv) · [page](view.md) — 5 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x004EC420` | `viewModeTable` | re | view-mode dispatch table scanned by VIEWModeLookup |
| `0x005223F0` | `replayWindowStart` | re | replay capture window start tick |
| `0x005223F4` | `replayWindowEnd` | re | replay capture window end tick |
| `0x00522400` | `replaySaveBuf` | re | saved-view replay buffer base (0x30 dwords copied in/out of the view) |
| `0x005224C0` | `replayActive` | re | replay-active flag (set by VIEWReplayRecordGate, read by VIEWReplayPlayback) |

### Collision (COL)

[`collision.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/collision.csv) · [page](collision.md) — 38 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x00536728` | `_colSelfObj` | re | querying entity ptr (predicted-pos cache follows) |
| `0x00536730` | `_colRayEndX` | re | query ray end X |
| `0x00536734` | `_colRayEndY` | re | query ray end Y |
| `0x00536738` | `_colRayEndZ` | re | query ray end Z |
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
| `0x00536770` | `_colStructList` | re | structure id list (word[0x1c2]) |
| `0x00536AF8` | `_colObjAngleH` | re | object-hit angle H |
| `0x00536AFC` | `_colObjAngleP` | re | object-hit angle P |
| `0x00536B00` | `_colClosureRadius` | re | closure radius (query param) |
| `0x00536B04` | `_colGearHeight` | re | gear/ground clearance (COLInfo+8 <<8) |
| `0x00536B0C` | `_colSelfSide` | re | IFF side bit of the querying object |
| `0x00536B10` | `_colObjList` | re | collidable id list (word[900]) |
| `0x00537218` | `_colBlockAngleH` | re | blocking-hit angle H |
| `0x0053721C` | `_colBlockAngleP` | re | blocking-hit angle P |
| `0x00537220` | `_colRayStartX` | re | query ray start X |
| `0x00537224` | `_colRayStartY` | re | query ray start Y |
| `0x00537228` | `_colRayStartZ` | re | query ray start Z |
| `0x0053722C` | `_colObjId` | re | nearest hit object id |
| `0x00537230` | `_colBlockPosX` | re | blocking-hit pos X |
| `0x00537234` | `_colBlockPosY` | re | blocking-hit pos Y |
| `0x00537238` | `_colBlockPosZ` | re | blocking-hit pos Z |
| `0x0053723C` | `_colSelfId` | re | querying entity id |
| `0x00537240` | `_colObjPosX` | re | object-hit pos X |
| `0x00537244` | `_colObjPosY` | re | object-hit pos Y |
| `0x00537248` | `_colObjPosZ` | re | object-hit pos Z |
| `0x0053724C` | `_colTargetId` | re | single-target id (excluded from broad-phase) |
| `0x00537250` | `_colObjDist` | re | nearest object-hit distance (sentinel 0x7FFFFFFF) |
| `0x00537254` | `_colObjBox` | re | hit box pointer |

### Sound / music (incl. WAIL32)

[`sound.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/sound.csv) · [page](sound.md) — 5 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x004EB5F0` | `_musicOn` | re | music/MIDI subsystem-available master flag |
| `0x004F3C10` | `_warnSndVol` | re | initial-volume table for _warnSnd |
| `0x004F3C20` | `_warnSndName` | re | filename table for _warnSnd (IR1.11K, ...) |
| `0x004F3CCC` | `_curShellMusic` | re | current shell-music category |
| `0x005380B8` | `_warnSnd` | re | RWR/IR threat-warning tone channel table (8 entries) |

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
| `0x00573248` | `resCache` | re | 20-slot x 8-byte LRU find-cache {RES_LIST* ; timerTicks} spanning 0x573248..0x5732E8 [was DAT_00573248] |

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

[`renderer.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/renderer.csv) · [page](renderer.md) — 3 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x004F6CA4` | `_blitDestX` | re | destination X of the present blit rect (GG_FlushShaken/FlushDirtyLines) |
| `0x004F6CA8` | `_blitDestY` | re | destination Y of the present blit rect; offset by screen shake |
| `0x004F6CAC` | `_shakeParity` | re | current screen-shake parity toggle (GG_FlushShaken) |

### Wingman / group AI (WNG/GRP)

[`wingman.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/wingman.csv) · [page](wingman.md) — 1 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x00546AB8` | `_wmNameBuf` | re | scratch buffer for WNGName/GRPName output |

### Object / entity system & shape selection

[`objects.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/objects.csv) · [page](objects.md) — 15 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x004F6FDC` | `_curObjStackTop` | re | PushCurObj/PopCurObj nesting stack for GetCurObj |
| `0x004F6FE0` | `_cmdBufWritePtr` | re | command-buffer write cursor (WriteCmdBuf*/AllocCmdBuf) |
| `0x004FFE34` | `_objArena` | re | entity arena base (MMAllocPtr in OBJInit; shrunk by OBJStopAdding) |
| `0x00546B94` | `_curObjSize` | re | byte size of the current entity mirror; = type +0x03 + 0xDE for classes 0/2/4/6 |
| `0x00546B9C` | `_curTypeSize` | re | byte size of the current type-record mirror (from type +0x01) |
| `0x00546BA4` | `_lastPadlockId` | re | previous padlock/tracked id, compared by CheckForEvents1/2 |
| `0x00546BA8` | `_requeueChain` | re | objects serviced this frame; ChainMergeSorted folds it back into chainStart |
| `0x00546BB8` | `_lastCurZ` | re | previous frame Z of current object (CheckForEvents1) |
| `0x00553120` | `_objSizes` | re | word[900] per-id entity size table (OBJAdd/OBJSubtract); ends at _objArenaNext |
| `0x00553828` | `_objArenaNext` | re | bump cursor into the entity arena (OBJAdd memcpy target) |
| `0x0055382C` | `_tempAliasBase` | re | lowest temp-alias id: (-0x14 - thisComputer)*1000 - 999 |
| `0x00553830` | `_tempAliasMax` | re | highest temp-alias id: (-0x14 - thisComputer)*1000; OBJTempAlias wraps to base past it |
| `0x00553834` | `_multiAliasCursor` | re | OBJAliasForMulti/OBJNextAliasForMulti iteration cursor |
| `0x0055383C` | `_tempAliasNext` | re | next temp alias returned by OBJTempAlias (negative per-computer id band) |
| `0x00553840` | `_objArenaSize` | re | arena byte capacity (OBJInit parameter), bounds OBJAdd |

### AI interpreter (CT)

[`ai.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/ai.csv) · [page](ai-interpreter.md) — 11 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x0050CF83` | `_ctOverrideName` | re | alternate/override AI script-name buffer; selected by _ctState+0x1d |
| `0x0050CF90` | `_ctStateCheckpoint` | re | heap copy of _ctState for preemptible save/restore |
| `0x0050D312` | `_ctProgramNamePtr` | re | pointer to the current AI script name; NULL gates the interpreter off |
| `0x00546BC0` | `_ctCompareActor` | re | actor slot used by CTVarDiff to evaluate attributes under the other object |
| `0x00546BC8` | `_ctState` | re | 0x80-byte CT execution state block copied by CTSaveState/CTRestoreState (vars+stack+IP+base+name+line+priority) |
| `0x00546C48` | `_ctPrintBuf` | re | HUD message text staged by CTDo_print/printnum, flushed on loop exit |
| `0x00546C8C` | `_ctCheckPass` | re | validate/dry-run flag: skips branches and side effects, enables the stack-imbalance check |
| `0x00546C90` | `_ctActionTaken` | re | set 1 when a CTDo action fires; returned by CTExecProgram |
| `0x00546C94` | `_ctActorObj` | re | pointer to the current actor entity record |
| `0x00546C98` | `_ctHalt` | re | halt flag; the CTExecProgram loop runs while *IP!='%' and !_ctHalt |
| `0x00546CA4` | `_ctExecuting` | re | re-entry guard set across CTExecProgram |

### Terrain (T_)

[`terrain.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/terrain.csv) · [page](terrain.md) — 1 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x004AACF0` | `T_DefaultHorizon` | sms | default horizon descriptor (14 B) embedded after T_HorizonProc; pushed into leaf list by T_Make |

### Weapons — projectiles / seekers / ECM (PROJ)

[`weapons.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/weapons.csv) · [page](weapons.md) — 8 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x0050D0F8` | `_projGunSoundHandle` | re | active looping gun-sound handle |
| `0x0050D0FA` | `_projGunSoundExpiry` | re | gun-sound expiry tick |
| `0x0058F0F8` | `_projBestTargetCost` | re | seeker-search best-cost accumulator (init 0x7FFFFFFF) |
| `0x0058F10C` | `_projBestTargetId` | re | seeker-search result id |
| `0x0058F110` | `_projPkPenalty` | re | running hit-chance (Pk) multiplier |
| `0x0058F118` | `_projNameBuf` | re | weapon display-name buffer (PROJBuildName) |
| `0x0058F180` | `_projSeekerList` | re | seeker-parameter array from HARDBestSeekers |
| `0x0058F1D4` | `_projInboundWarnT` | re | inbound-missile warning throttle |

### Startup / Phar Lap DOS extender / config

[`startup.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/startup.csv) · [page](startup.md) — 47 named referenced globals

| VA | Symbol | Src | Role |
|----|--------|-----|------|
| `0x004D85AA` | `__NLG_Return2` | sms | MSVC CRT global (xrefs=1) |
| `0x004D86F8` | `__except_handler3` | sms | MSVC CRT global (xrefs=7) |
| `0x004DD080` | `__forcdecpt` | sms | MSVC CRT global (xrefs=1) |
| `0x004DD0F0` | `__cropzeros` | sms | MSVC CRT global (xrefs=1) |
| `0x004DD150` | `__positive` | sms | MSVC CRT global (xrefs=1) |
| `0x004DD170` | `__fassign` | sms | MSVC CRT global (xrefs=1) |
| `0x004DD510` | `__cfltcvt` | sms | MSVC CRT global (xrefs=1) |
| `0x0051E750` | `__umaskval` | sms | MSVC CRT global (xrefs=1) |
| `0x0051E758` | `__winver` | sms | MSVC CRT global (xrefs=1) |
| `0x0051E75C` | `__winmajor` | sms | MSVC CRT global (xrefs=1) |
| `0x0051E760` | `__winminor` | sms | MSVC CRT global (xrefs=2) |
| `0x0051E770` | `__environ` | sms | MSVC CRT global (xrefs=15) |
| `0x0051E778` | `__wenviron` | sms | MSVC CRT global (xrefs=6) |
| `0x0051E788` | `__exitflag` | sms | MSVC CRT global (xrefs=1) |
| `0x0051E78C` | `__C_Termination_Done` | sms | MSVC CRT global (xrefs=1) |
| `0x0051E790` | `__C_Exit_Done` | sms | MSVC CRT global (xrefs=2) |
| `0x0051E7A0` | `__NLG_Destination` | sms | MSVC CRT global (xrefs=2) |
| `0x0051E7BC` | `__adjust_fdiv` | sms | MSVC CRT global (xrefs=1) |
| `0x0051E7C0` | `__FPinit` | sms | MSVC CRT global (xrefs=1) |
| `0x0051E7D0` | `__aenvptr` | sms | MSVC CRT global (xrefs=5) |
| `0x0051E7D8` | `__aexit_rtn` | sms | MSVC CRT global (xrefs=1) |
| `0x0051E7F0` | `__locktable` | sms | MSVC CRT global (xrefs=9) |
| `0x0051E8B0` | `__pctype` | sms | MSVC CRT global (xrefs=38) |
| `0x0051E8B4` | `__pwctype` | sms | MSVC CRT global (xrefs=3) |
| `0x0051EC78` | `__cfltcvt_tab` | sms | MSVC CRT global (xrefs=2) |
| `0x0051EDF8` | `__mbctype` | sms | MSVC CRT global (xrefs=7) |
| `0x0051F010` | `__stdbuf` | sms | MSVC CRT global (xrefs=1) |
| `0x0051F2A8` | `__cflush` | sms | MSVC CRT global (xrefs=2) |
| `0x0051F2B0` | `__XcptActTab` | sms | MSVC CRT global (xrefs=1) |
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
