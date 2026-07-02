# FA.EXE Symbol Map — Organized Reference

FA.SMS ships with Jane's Fighters Anthology and contains 3,829 MSVC C++ mangled symbols with virtual addresses spanning `0x00401000`–`0x005937E0`. This document organizes them by address range into functional subsystems and highlights format-related entry points.

> **Provenance:** Ghidra static analysis of FA.EXE with [FA.SMS](formats/SMS.md) symbols applied; the file itself is specified in [formats/SMS.md](formats/SMS.md). Confidence markers follow [spec-authoring.md](../spec-authoring.md): confirmed · inferred · unknown.

---

## Summary Table

| Subsystem | Address Range | Approx. Symbol Count |
|-----------|--------------|---------------------|
| Network (NET/MP/SER) | 0x401000–0x409000 | ~60 |
| HUD / cockpit display | 0x405E30–0x40AE50 | ~40 |
| Core shell / menu | 0x40AE50–0x421C70 | ~90 |
| Sound / music | 0x432920–0x435F80 | ~70 |
| Memory manager (MM) | 0x435C60–0x436320 | ~35 |
| Campaign map (MAP/CAM) | 0x421C70–0x42B800 | ~25 |
| Collision (COL) | 0x42B800–0x42E690 | ~20 |
| Flight model (FM/HARD) | 0x451480–0x454800 | ~80 |
| Video decode (FMV/Cobra) | 0x456300–0x45D090 | ~45 |
| Network (UDP/PKT layer) | 0x45D090–0x45DBD0 | ~30 |
| Graphics low-level (GG/G_) | 0x45DBD0–0x499380 | ~130 |
| Wingman/Group AI (WNG/GRP) | 0x45E460–0x460FB0 | ~50 |
| Object system (OBJ/chain) | 0x462600–0x464C80 | ~40 |
| AI interpreter (CT) | 0x464C80–0x467110 | ~120 |
| Pilot / mission / campaign | 0x467110–0x490000 | ~180 |
| Joystick / serial / modem | 0x494270–0x4AC510 | ~110 |
| Terrain renderer (T_) | 0x4A6E50–0x4C5D70 | ~90 |
| Projectile / weapons (PROJ) | 0x4C0690–0x4C5D30 | ~55 |
| 3D renderer (GR/render) | 0x4C5D70–0x4D5C00 | ~100 |
| Airport / carrier (AP) | 0x4BA750–0x4BEE60 | ~40 |
| World render / palette (WR) | 0x4B3010–0x4B4B30 | ~30 |
| Multiplayer protocol (MP) | 0x46ADE0–0x473680 | ~95 |
| Dialog / UI shell | 0x487A3A–0x48D200 | ~70 |
| SAY / voice callout | 0x48D2B0–0x491240 | ~20 |
| CRT / Win32 imports | 0x4D6F5C–0x4E8B66 | ~300 |
| Data globals / BSS | 0x4EB5F4–0x593800 | ~300 |

---

## Subsystem Sections

### Network — Master/Slave, UDP, TCP, SPX, SER (0x401000–0x409000 + scattered)

Low addresses are NET slave/master negotiation and high-level wrappers; UDP/TCP/SPX factories and serial modem code are in 0x44xxx–0x4Bxxx.

Key functions:
- `0x4016C0` — `NET_SlaveInit(CN_INFO*, NET_ADDRESS*, …)`
- `0x401780` — `NET_SlaveShutdown()`
- `0x4017B0` — `NET_RequestPlayerList(…)`
- `0x401850` — `NET_CancelPlayerList()`
- `0x401880` — `PlayerListQueryEvents(…)`
- `0x401B20` — `slave_events(…)`
- `0x401EB0` — `slave_process_pkt(NET_PKT*, socket_state*)`
- `0x402320` — `state_func_slave_connecting()`
- `0x40AE50` — `NET_MasterInit(…)`
- `0x40AF40` — `state_func_master_query()`
- `0x40AFF0` — `NET_MasterRejectPlayer(NET_ADDRESS*, char*)`
- `0x40B080` — `NET_MasterShutdown()`
- `0x44BAF0` — `SER_EnterCriticalCodeForeground()`
- `0x44BB70` — `SER_ForegroundCheckConnection(…)`
- `0x44C470` — `SER_Initialize(CN_INFO*, long)` (+ Init1–5)
- `0x44CCA0` — `SER_Shutdown()`
- `0x4B0830` — `NET_Initialize(CN_INFO*, long)`
- `0x4B0A10` — `NET_Shutdown()`
- `0x4B0AC0` — `NET_Often()`
- `0x4B0BD0` — `NET_Synchronize()`
- `0x4B0CF0` — `NET_Write(…)` / `NET_Read(…)` / `NET_Flush(…)`
- `0x4B1350` — `NET_SendMessageAll(char*)`
- `0x4B1540` — `NETProcessEvent(…)`

---

### HUD / Cockpit Display (0x405E30–0x40AE50)

- `0x405E30` — `HUDInitMessages()`
- `0x405E50` — `HUDDrawMessages(char)`
- `0x405F50` — `HUDMessage(…)`
- `0x406040` — `HUDInit()`
- `0x406950` — `HUDShutdown()`
- `0x406A50` — `HUDDraw(char)`
- `0x4077B0` — `HUDSetWarning(…)`
- `0x407B60` — `HUDDrawHeading()`
- `0x407EE0` — `HUDDrawSpeed()`
- `0x408420` — `HUDDrawAlt()`
- `0x408E20` — `HUDDrawHVel()`
- `0x409030` — `HUDDrawWeaponInfo()`
- `0x4092D0` — `HUDDrawRangeInfo()`
- `0x40A450` — `HUDSquawk()`
- `0x40A530` — `HUDFindNearest(…)`
- `0x40ABB0` — `HUDDrawDisrupt()`
- `0x40AC80` — `HUDSetStability(long)`
- `0x40ACE0` — `HUDDrawStability()`
- `0x40AE40` — `HUDHasFlaps(char)`

---

### Core Shell / Menu (0x40B080–0x421C70)

Shell setup, menu creation, mouse handling, view slew, damage system.

- `0x40B8A0` — `MouseLoadPtr()`
- `0x40BA10` — `ShellSetup()`
- `0x40BD30` — `MenuStartUp(…)`
- `0x40C1F0` — `MenuCreateRemaps()`
- `0x40C290` — `ShellOff()`
- `0x40C310` — `MenuShutDown(char)`
- `0x40C4F0` — `MenuUpdate()`
- `0x40CFE0` — `ShadowBox(long,long,long,long)`
- `0x40D7A0` — `VIEWSlew(…)`
- `0x40F6B0` — `DAMAGEInit2()` / `0x40F760` — `DAMAGEInit()`
- `0x40F970` — `DAMAGEDoHit(…)`
- `0x4113C0` — `DAMAGEAutopilotAvail()`
- `0x411A40` — `Angles(…)` / `AngleOffNose(…)` / `AnglesOffNose(…)`
- `0x411BD0` — `Clock(…)`
- `0x4120C0` — `Move3d(…)`
- `0x412C10` — `PlaySeq(char*, long, long)`
- `0x413120` — `CHATInit()` / `CHATKey(…)` / `CHATEndMission()`
- `0x4140A0` — `SetPlayerTarget(…)` / `TargetNearestTo(…)`
- `0x414690` — `FlightKey(…)`
- `0x416380` — `SetAutopilot(…)` / `ForceAutopilot(…)`
- `0x4164B0` — `ServicePlayer()`
- `0x417760` — `InitPlayerControl()`
- `0x418070` — `MSGInit()` / `MSGSend(…)` / `MSGReceive(…)`
- `0x419800` — `ArmPlane(…)`
- `0x41D740` — `CDirDraw::CreateSingleton()` / `CDirDraw::Create(…)`
- `0x41E8F0` — `IsBrentDLL(void*)` — tests Phar Lap PE signature
- `0x41E910` — `IsDLL(…)`
- `0x41EB60` — `LoadDLL(…)` — generic overlay DLL loader
- `0x41F240` — `LoadBrentDLL(…)` — loads Phar Lap PE overlay

---

### Sound / Music (0x432920–0x435F80)

- `0x432920` — `ShutDownMidi()`
- `0x4329A0` — `DMusicOn(char*, float)` / `MusicOn(char*, float)`
- `0x432C30` — `ScoreOn(void*, char)` / `ScoreOff()` / `ScoreUpdate()`
- `0x432F80` — `ShellMusicUpdate(long)` / `ShellMusic(char)`
- `0x433180` — `InitSound()` / `ShutDownSndDriver()` / `InitMixer()`
- `0x433480` — `SoundPoints()`
- `0x433580` — `SingleSound(char*, float, …)`
- `0x433680` — `SoundOn(…)` — full parameter sound trigger
- `0x433CE0` — `SoundOff(short)`
- `0x433D40` — `SoundAllOff()`
- `0x434800` — `MaybeLoopSound(…)` / `UpdateLoopSounds()`
- `0x4349D0` — `ServiceSounds()`
- `0x435480` — `StopGameSounds()`
- `0x435980` — `StartVoice(MODSPEC*, short)`
- `0x435B80` — `SoundStatus()` / `SndLostFocus()` / `SndGotFocus()`

---

### Memory Manager (MM) (0x435C60–0x436320)

- `0x435C60` — `MMInit(…)` / `MMShutdown()`
- `0x435D80` — `MMAllocHandle(…)` / `MMMapFile(…)` / `MMFreeHandle(…)`
- `0x435F80` — `MMFreePtr(…)` / `MMReallocHandle(…)` / `MMReallocPtr(…)`
- `0x436170` — `MMPushAllocId(…)` / `MMPopAllocId()` / `MMFreeAllId(…)`
- `0x436210` — `MMLock(…)` / `MMUnlock(…)` / `MMAccessR(…)` / `MMAccessW(…)`
- `0x4362C0` — `MMAreaFree()`

---

### Campaign Map (MAP/ZONE) (0x421C70–0x42B800)

- `0x421C70` — `ZONEInit()` / `ZONEAdd(…)` / `ZONEForGV()` / `ZONEUpdate()`
- `0x4221D0` — `MAPObjAlts(…)` / `MAPSetSide(…)` / `MAPMaybeSetControl(…)`
- `0x422380` — `MAPWorldToScreen(F24_POINT3*, WORD_POINT*)`
- `0x4223BE` — `MAPDrawGrid()`
- `0x4224EE` — `MAPDrawBG()`
- `0x42267F` — `MAPUpdateWPPtrs(…)` / `MAPSetNewWP(…)`
- `0x422851` — `MAPDrawSpecials()` / `MAPOnSpecial(…)`

---

### Collision (COL) (0x42B800–0x42E690)

- `0x42B800` — `Collision(…)` — main collision check
- `0x42BD30` — `COLSetAngle(…)`
- `0x42DDA0` — `COLFlatGround(…)`
- `0x42DF80` — `COLPitchToAvoidTerrain()`
- `0x42E0C0` — `COLGetInfo(…)` / `COLGetBox(…)`
- `0x42E4E0` — `COLTerrainBlocking(…)`
- `0x42E530` — `COLInit()` / `COLAddObj()` / `COLRemoveCurObj()`

---

### Flight Model / Hardpoints (FM/HARD) (0x451480–0x454800)

- `0x4514C0` — `FMUpdateGearPitch()` / `FMUpdateGear()` / `FMUpdateWingSweep()`
- `0x4516B0` — `FMGetWeight()`
- `0x4518A0` — `FMInitPlane(…)`
- `0x451B00` — `SetThrottle(…)` / `FMFlaps(…)` / `FMGear(…)` / `FMBrakes(…)`
- `0x451E50` — `FMFuelConsumption(…)` / `BurnFuel()`
- `0x452140` — `FMUpdatePlaneFields()`
- `0x452770` — `HARDPtrs(…)` — hardpoint pointer resolver
- `0x4527F0` — `HARDUnload(…)` / `HARDLoad(…)` / `HARDLoadAll()` / `HARDUnloadAll()`
- `0x452D90` — `HARDBestSeekers(…)` / `HARDBestSeeker(…)` / `HARDFindJammer(…)`
- `0x452F80` — `HARDFindStore(…)` / `HARDFindProj(…)`
- `0x453440` — `HARDGunsOnlyAll()`
- `0x453AC0` — `HARDNumLoaded(…)` / `HARDTotalFuel()`
- `0x453B90` — `HARDRearmTest()` / `HARDRearmHumanLoad()`
- `0x454140` — `ChangePlaneType(…)` / `RepairTime(…)` / `SelectRepairPlane(…)`

---

### AI Interpreter (CT) (0x464C80–0x467110)

Full condition evaluator and action dispatcher for `.AI` scripts. All `CTEval_*` and `CTDo_*` are exported by `.BI` DLLs and resolved by name.

Selected `CTEval_*` (condition evaluators):
- `0x464E20` — `CTEval_time` / `CTEval_do_nothing` / `CTEval_do_evade` / `CTEval_do_attack`
- `0x464E60` — `CTEval_do_radar_launch` / `CTEval_do_ir_launch` / `CTEval_do_hit`
- `0x464F10` — `CTEval_tgt` / `CTEval_tgtclass` / `CTEval_tgtisfighter` / `CTEval_tgtisbomber`
- `0x464FF0` — `CTEval_tgtisship` / `CTEval_tgtissam` / `CTEval_tgtisaaa`
- `0x465040` — `CTEval_maxrange` / `CTEval_bestrange` / `CTEval_radar` / `CTEval_ir`
- `0x465150` — `CTEval_tgtahead` / `CTEval_tgtfacing` / `CTEval_disttotgt`
- `0x4653A0` — `CTEval_speed` / `CTEval_minspeed` / `CTEval_cornerspeed` / `CTEval_maxspeed`
- `0x465480` — `CTEval_twr` / `CTEval_turnrate` / `CTEval_turnradius`
- `0x465510` — `CTEval_alt` / `CTEval_altdiff` / `CTEval_maxalt` / `CTEval_minalt`
- `0x465640` — `CTEval_disttowaypoint` / `CTEval_skill` / `CTEval_engagep`

Selected `CTDo_*` (action executors):
- `0x465A30` — `CTDo_exit` / `CTDo_restart` / `CTDo_maneuver` / `CTDo_play`
- `0x465CC0` — `CTDo_move` / `CTDo_movetoalt` / `CTDo_turn`
- `0x466052` — `CTDo_yoyo` / `CTDo_circle` / `CTDo_homeangle` / `CTDo_homepos`
- `0x4663F0` — `CTDo_jink` / `CTDo_invert` / `CTDo_btoh` / `CTDo_immelman`
- `0x4665E0` — `CTDo_wm_break` / `CTDo_wm_approach` / `CTDo_wm_formation`
- `0x466970` — `CTExecProgram(…)` — `.AI` interpreter loop

---

### Pilot / Mission / Campaign (0x467110–0x490000)

- `0x467180` — `PilotSave(PILOT*, short)` / `PilotPhoto(PILOT*)`
- `0x467310` — `CallsignChoose(PILOT*, long)` / `EditPilot(…)`
- `0x468020` — `PilotScreen(…)`
- `0x4692D0` — `EJECTProc` / `EJECTAdd(…)` / `EJECTRemove()`
- `0x4754B0` — `PilotSave(…)` — save pilot to .PLT file
- `0x480750` — `_MISSIONInit1()` / `_MISSIONInit2()`
- `0x480B40` — `MISSIONInit1()` / `MISSIONInit2()` / `MISSIONInit3()`
- `0x480C20` — `LoadCampaignProc`
- `0x480C40` — `InitCampaignPilot`
- `0x480C90` — `AddCampaignPlane`
- `0x480D70` — `CampaignPlanesLeft()`
- `0x480DF0` — `UkraineAddA7` — per-theater campaign hook
- `0x481150` — `AtFriendlyAP()`
- `0x481320` — `CampaignSave` / `CampaignOff`
- `0x481440` — `CallCampaignProc(…)` / `CallMissionProc(…)`
- `0x4819F0` — `MISSIONShutdown()` / `MISSIONSuccess()`
- `0x4851C0` — `MISSIONFortDestroyed(…)` — fort destruction logic
- `0x485260` — `MISSIONFortDestroyedByFort(…)`
- `0x486010` — `MISSIONLoadCommonResources()`
- `0x486160` — `MISSIONEndScenario()`
- `0x486860` — `MISSIONCheckSuccess()`
- `0x4869A0` — `TIMESystemTime()` / `TIMEInit(…)` / `TIMEUpdate()`

---

### Object System / Entity Chain (0x462600–0x464C80)

- `0x462600` — `InitChain()` / `RemoveFromChains()` / `ImmediateService()`
- `0x4627B0` — `RemoveCurObj()` / `GetCurObj(…)` / `PutCurObj()`
- `0x4629E0` — `PushCurObj(…)` / `PopCurObj()`
- `0x462A50` — `ServiceObjects`
- `0x462E70` — `Service()` — main per-frame service
- `0x463980` — `MaybeCallEventProc(…)` / `CallEventProc(…)`
- `0x463A20` — `CreateMove(…)` / `CreateMoveGoal(…)`
- `0x463F60` — `CallUtilProc` — dispatches to OBJ/GV/PROJ proc
- `0x464040` — `Reaction(…)` / `EnterState(…)`
- `0x473A40` — `OBJEventProc` / `OBJDamageProc(HIT_OBJ_DATA*)`
- `0x473BE0` — `OBJProc` — static object update
- `0x473C10` — `Kill()`
- `0x473DB0` — `GVProc` — ground vehicle update
- `0x491240` — `OBJGet(…)` / `OBJInit(…)` / `OBJShutdown()`
- `0x491300` — `OBJAlloc(…)` / `OBJAdd(…)` / `OBJSubtract()`
- `0x4914C0` — `OBJAlias(…)` — alias lookup (used by .MC DLLs)

---

### Wingman / Group AI (WNG/GRP) (0x45E460–0x460FB0)

- `0x45E460` — `WNGInit()` / `WNGAdd(…)` / `WNGWingmen(…)` / `WNGPart(…)`
- `0x45E8F0` — `WNGLeaderLanding()` / `WNGFormationMove(…)` / `WNGSendWM(…)`
- `0x45F190` — `GRPInit()` / `GRPAdd(…)` / `GRPRemove()`
- `0x45F360` — `GRPLeader(…)` / `GRPWingman(…)` / `GRPWingmenNearby(…)`
- `0x45F580` — `GRPSetWaypoints(…)` / `GRPControl(…)` / `GRPLeaderLanding()`
- `0x45F7F0` — `GRPSetControl(…)` / `GRPSetType(…)` / `GRPSetSpacingH/V(…)`
- `0x45FE30` — `GRPName(…)`
- `0x45FEC0` — `INFO2Draw()`
- `0x46A370` — `SMInit()` / `SMShutdown()` / `SMAddress(…)` / `SMCallByName`

---

### Airport / Carrier (AP) (0x4BA750–0x4BEE60)

- `0x4BA750` — `APInit()` / `APAdd(…)` / `APDelete(…)` / `APNearest(…)`
- `0x4BAA10` — `APTakeoffType(…)` / `APLandingType(…)`
- `0x4BADB0` — `APTakeoff()`
- `0x4BC210` — `APStartFinalApproach()` / `APEndArrestorCatch()` / `APLanding()`
- `0x4BD2D0` — `APFind(…)` / `APClearParks()` / `APGetPark()` / `APAssignPark()`
- `0x4BD5B0` — `CARRIERProc`
- `0x4BE640` — `STRIPProc` / `APApproachPath(…)` / `APTeleport`
- `0x4BEB00` — `APAddToCarrier(…)` / `APRemoveFromCarrier()` / `APCheckCarrier()`
- `0x4BED70` — `APHomeAirport()` / `APObjOnShip(…)`

---

### World Render / Palette / Layer (WR) (0x4B3010–0x4B4B30)

See architecture.md for the full per-frame update pipeline. These functions implement the atmosphere/sky system that consumes loaded `.LAY` DLL data.

- `0x4B3190` — `WRGetLayer(…)` / `WRSetRemaps(…)`
- `0x4B3480` — `WRUpdate(…)` — transition atmosphere parameters
- `0x4B3D90` — `WRUpdatePalette()` — per-frame palette smooth-transition (= `UpdateSkyState`)
- `0x4B4170` — `WRLightUpdate()`
- `0x4B4320` — `WRFogLayerUpdate` — per-frame fog density jitter
- `0x4B4370` — `WRInit(…)` — loads LAY DLL, sets up atmosphere (= `ParseLayerFile`)
- `0x4B46D0` — `WRShutdown()`
- `0x4B4720` — `WRWeatherEffects`
- `0x4B47B0` — `SetTmapRemaps()`
- `0x4B4990` — `WRLensFlare()` / `WRCanSee(…)`
- `0x4C8E20` — `WRBlackenPalette(…)` / `WRWhitenPalette(…)` / `WRReddenPalette(…)` / `WRColorPalette(…)`

---

### Projectile / Weapons (PROJ) (0x4C0690–0x4C5D30)

- `0x4C06A0` — `PROJInit()` / `PROJGetTargetPos(…)` / `PROJAccurateHardPos(…)`
- `0x4C0870` — `PROJSetTarget(…)` / `PROJLockUpdate()`
- `0x4C0A90` — `PROJAdd(…)` — spawn projectile
- `0x4C1170` — `PROJEngineState()`
- `0x4C11B0` — `PROJMoveProc(char)` / `PROJDamageProc(HIT_OBJ_DATA*)`
- `0x4C1F50` — `PROJProc`
- `0x4C20C0` — `PROJHit(…)` / `PROJFire(…)` / `PROJFireSound(…)`
- `0x4C2860` — `PROJInFOV(…)` / `PROJRadarIsOn(…)` / `PROJLock(…)`
- `0x4C3380` — `PROJHitChance(…)` / `PROJLaunchDevice(…)`
- `0x4C3CA0` — `PROJRemove()` / `PROJRetargetMissiles(…)`
- `0x4C3EB0` — `PROJMakeBombEq(…)` / `PROJChangeBombEq(…)` / `PROJBombPos(…)`
- `0x4C4100` — `PROJSelectTarget()` / `PROJServiceWeapon(…)`
- `0x4C5670` — `PROJSendCollateralDamages(…)`

---

### Terrain Renderer (T_) (0x4A7310–0x4C5D70)

- `0x4A6E50` — `LoadPIC` — bitmap load dispatcher
- `0x4A6EB0` — `SetupOT` / `SetupNT` / `SetupPT` / `SetupJT` — BRF object type setup
- `0x4A7310` — `T_InitPlane(…)` / `T_AddObj(…)` / `T_AddYourObjs()`
- `0x4A7D70` — `T_ImmediateVisibility(…)` / `T_ObjList(…)` / `T_Render(…)`
- `0x4A7F20` — `T_InitForestProc` / `T_ForestProc(long)`
- `0x4A8660` — `T_InitFarmProc` / `T_FarmProc(long)` / `T_InitMooseProc` / `T_MooseProc(long)`
- `0x4A8870` — `T_InitVietRicePaddy1–3Proc` / `T_VietPalms1–3Proc` / `T_VietTrees1–3Proc`
- `0x4A8A70` — `T_InitWaterProc` / `T_WaterProc(long)` / `T_InitCloudProc` / `T_CloudProc(long)`
- `0x4A8D30` — `T_Normal(…)` / `T_LeafOp(…)` / `T_Make(…)`
- `0x4AA620` — `T_InitDictionary()` / `T_InitDictionaryEntry(…)` / `T_NamedTmaps()`
- `0x4AACF0` — `T_DefaultHorizon` / `T_HorizonProc` — exported as `T_HorizonProc` from FA.EXE
- `0x4C5D60` — `T_Init()` / `T_Load(…)` / `T_Init2()` / `T_Shutdown()` / `T_StopAdding()`
- `0x4C6040` — `T_GetLeaf(…)`

---

### 3D Renderer (GR/render) (0x4C5D70–0x4D5C00)

- `0x4D5B64` — `GRInit3d(…)` / `GRRender(…)` / `GRSinCos(…)` / `GRTo2d(…)`
- `0x4D5E58` — `MakeObjRotationMatrix(…)` / `MakeViewRotationMatrix(…)` / `MultPointByMatrix(…)`
- `0x4D6348` — `GRSaveContext()` / `GRRestoreContext()` / `GRExec(…)`
- `0x4D64D8` — `MultF24PointByMatrix(…)` / `Sqrt(…)`
- `0x4D057C` — `GRAddBrentObj(…)` — add BRF object to render queue
- `0x4CD834` — `GRSetLightSource(…)` / `SetShading`
- `0x4CD8B0` — `Sun` — sun direction update
- `0x4CDCB8` — `render_3d` — main 3D render entry
- `0x4CE980` — `dmxmul` / `dmxmul2` — matrix multiply helpers
- `0x4CC4B4` — `SetShadingTable` (= `SetActiveLayerByAngle`)
- `0x4CCB88` — `ArcTan(…)`

Low-level shape dispatch opcodes (interpreter for .SH bytecode):
- `0x4D2180` — `must_clip_3d`
- `0x4D22A8` — `do_sfcal_long`
- `0x4D22D4` — `do_ifdestroyed` — destruction-state conditional
- `0x4D2380` — `do_if_not_effect`
- `0x4D33D8` — `do_icall_long` / `do_jumpfar4`
- `0x4D42EC` — `do_setcoarse` / `do_set_point_color` / `do_set_gouraud`
- `0x4D43DC` — `do_new_poly` / `do_new_smap` / `do_new_rmap` / `do_new_pmap_or_tmap`
- `0x4D47B8` — `do_streamer_def` / `do_streamer_draw`

---

### Dialog / UI Shell (0x487A3A–0x48D200)

- `0x487A63` — `DialogSetup(…)` / `DialogShow()` / `DialogShutDown(…)` / `DialogDone()`
- `0x488470` — `DialogUpdate(…)` / `DialogWhatItem()`
- `0x4892E0` — `DialogGetPtr(…)` / `DialogGetValue(…)` / `DialogSetValue`
- `0x489400` — `DialogSetRocker(…)` / `DialogSetString(…)` / `DialogGetString(…)`
- `0x489AC0` — `DrawText` — imported by .MNU/.DLG overlays as `main.dll::_DrawText`
- `0x489B90` — `DrawAction` — imported by overlays as `main.dll::_DrawAction`
- `0x48A730` — `DrawLight` / `DrawFormattedText` / `DrawMissList` / `DrawCampaignList`
- `0x48B4E0` — `DrawRocker` / `DrawToggle` / `DrawSliderHoriz` / `DrawSliderVert`
- `0x48C710` — `DrawEditBox`

---

### SAY / Voice Callout (0x48D2B0–0x491240)

- `0x48D2B0` — `SAYInit()` / `SAYInit2()` / `SAYShutdown()`
- `0x48D350` — `SAYMsg(…)` / `SAYDefaultSayProc`
- `0x48D780` — `PLANESayProc`
- `0x48E8D0` — `OBJSayProc`
- `0x48E920` — `SAYRearmMessage(…)` / `SAYSuppRadarMessage(…)` / `SAYLowFuelMessage(…)`
- `0x48EC40` — `PLANECommentProc`
- `0x48F6A0` — `APCommentProc`
- `0x490F30` — `SAYTranslate(…)` / `SAYFortAircraft` / `SAYFortStatus`

---

### Graphics Low-Level (G_/GG) (0x45DBD0–0x499380)

- `0x45DBD0` — `GG_InitMode()` / `GG_ShutdownMode()` / `GG_GetMode()`
- `0x45DE70` — `GG_SetPalette(…)` / `GG_Shake()` / `GG_Flush(…)`
- `0x497340` — `G_Init()` / `G_Shutdown()`
- `0x4974E0` — `G_SetBitmap(…)` / `G_SetClipBox(…)` / `G_SetColor(…)`
- `0x497700` — `G_UHline(…)` / `G_Hline(…)` / `G_Vline(…)` / `G_Line(…)`
- `0x497D40` — `G_UBox(…)` / `G_Box(…)` / `G_Rect(…)`
- `0x4983E0` — `G_DrawYLR(…)` / `G_Flush(…)` / `G_Flip(…)`
- `0x4986A0` — `G_SetFont(…)` / `G_Print(…)` / `G_Printf`
- `0x498A30` — `G_LoadDriver(…)` / `G_UnloadDriver()`
- `0x4B7930` — `G_RelocBitmap(…)` / `G_AllocBitmap(…)` / `G_AllocSurfaceBitmap(…)`
- `0x4B7CD0` — `G_LoadBitmap(…)` — load PIC from LIB
- `0x4B7FA0` — `G_BlitToScreen(…)` / `G_Blit(…)`
- `0x4B87C0` — `G_Texture(…)` / `G_AcTexture(…)` / `G_PerspectiveFlip(…)`
- `0x4B9430` — `NPM_FlatTri(…)` / `NPM_TextureLinearTri(…)` / `NPM_TexturePerspectiveTri(…)`

---

## Format Loaders and Parsers

Cross-reference of symbols that directly load, initialize, or parse named file formats.

### LIB Archive (EALIB)

| Address | Symbol | Role |
|---------|--------|------|
| `0x47A090` | `LibSeek(…)` | Seek within open LIB entry |
| `0x47A130` | `LibFileExists(…)` | Test for named entry |
| `0x479BD0` | `LibOpen(…)` | Open a named LIB entry |
| `0x479C80` | `LibRead` | Read bytes from open entry |
| `0x479D20` | `LibClose` | Close entry handle |
| `0x479D40` | `LibFileSize` | Query entry size |
| `0x479630` | `DoLoadLibFile` | Internal LIB decompression dispatch |
| `0x47A5A0` | `InitGraphicsMode` | Sets up graphics mode post-LIB init |
| `0x47BC40` | `LibStartUp` | Initialize LIB subsystem |
| `0x4792D0` | `LibShutDown` | Shutdown LIB subsystem |
| `0x479350` | `LibUpdate` | Periodic LIB maintenance |

### Overlay DLL (.LAY, .HUD, .FNT, .CAM, .MUS, .BI, .MC)

| Address | Symbol | Role |
|---------|--------|------|
| `0x41E8F0` | `IsBrentDLL(void*)` | Detect Phar Lap `PL\0\0` signature |
| `0x41E910` | `IsDLL(…)` | Generic DLL validity check |
| `0x41EB60` | `LoadDLL(…)` | Load and IAT-patch an overlay DLL |
| `0x41F240` | `LoadBrentDLL(…)` | Load Phar Lap PE overlay (CAM/BI/MC) |
| `0x4B4370` | `WRInit(…)` | Load `.LAY` file via `LoadLibrary` + IAT patch |
| `0x4A6E50` | `LoadPIC` | Load `.PIC` bitmap (also via LIB) |
| `0x4A7220` | `SetupPT` | Init `.PT` (playable aircraft BRF type) |
| `0x4A6EB0` | `SetupOT` | Init `.OT` (static object BRF type) |
| `0x4A7200` | `SetupNT` | Init `.NT` (NPC/vehicle BRF type) |
| `0x4A7230` | `SetupJT` | Init `.JT` (projectile BRF type) |

### Config / Save (.CFG, .PLT, NET.DAT)

| Address | Symbol | Role |
|---------|--------|------|
| `0x47F6D0` | `CN_SetFactoryDefaults(CN_INFO*)` | Initialize config struct to defaults |
| `0x47F7A0` | `CN_ReadConfig(CN_INFO*, unsigned char*)` | Read `EA.CFG` into CN_INFO |
| `0x47F930` | `CN_WriteConfig(CN_INFO*, unsigned char*)` | Write `EA.CFG` from CN_INFO |
| `0x47F740` | `CfigChecksum(CN_INFO*)` | Verify config checksum |
| `0x4B2930` | `UCONFIG_load_EA_CFG()` | High-level `EA.CFG` load |
| `0x4B2980` | `UCONFIG_save_EA_CFG()` | High-level `EA.CFG` save |
| `0x4B2BD0` | `UCONFIG_Initialize()` | Full config system init |
| `0x467180` | `PilotSave(PILOT*, short)` | Write `.PLT` pilot save file |

### BRF / Object Types (.OT, .NT, .PT, .JT, .GAS, .ECM)

| Address | Symbol | Role |
|---------|--------|------|
| `0x41E8F0` | `IsBrentDLL(void*)` | Detect BRF magic header |
| `0x4A6EB0` | `SetupOT` | Load/init `.OT` static object |
| `0x4A7200` | `SetupNT` | Load/init `.NT` NPC/vehicle |
| `0x4A7220` | `SetupPT` | Load/init `.PT` playable aircraft |
| `0x4A7230` | `SetupJT` | Load/init `.JT` projectile |

### Video (.VDO / Cobra codec)

| Address | Symbol | Role |
|---------|--------|------|
| `0x4AE440` | `PlayVDOString(char*, …)` | Play FMV by filename |
| `0x4AE406` | `PlayVDOFile(char*, …)` | Play FMV from open file |
| `0x4AF070` | `StartVDOAudio(char*)` | Start audio stream for VDO |
| `0x4AF1B0` | `OpenVDOFile(char*)` | Open a `.VDO` file |
| `0x4AF200` | `ReadVDOHeader(…)` | Parse `.VDO` file header |
| `0x4AF2D0` | `ReadFrameSizesFile(char*)` | Read `.FBC` companion sizes |
| `0x4AF320` | `ReadVDOPalette(…)` | Extract palette from VDO header |
| `0x4AF3A0` | `AllocVDO(VDO*)` | Allocate VDO playback context |
| `0x4AE4E0` | `BuildVDOList(char*)` | Build linked list of VDO files |
| `0x4AED50` | `VDOSetMode(VDO*)` | Set video decode mode |
| `0x442360` | `InitMovieContext(MovieContext*, …)` | Init Cobra codec context |
| `0x442370` | `DecodeFrame(MovieContext*, …)` | Decode one Cobra video frame |

### Terrain (.T2)

| Address | Symbol | Role |
|---------|--------|------|
| `0x4C5D60` | `T_Init()` | Initialize terrain database |
| `0x4C5D70` | `T_Load(…)` | Load `.T2` terrain file |
| `0x4C5D50` | `T_ShutdownDatabase()` / `T_Init2()` / `T_Shutdown()` | Lifecycle |
| `0x4AA620` | `T_InitDictionary()` | Set up terrain tile dictionary |
| `0x4AA680` | `T_InitDictionaryEntry(…)` | Add `.T2` tile entry |
| `0x4AA7E0` | `T_CompareTlist(…)` / `T_SortTmapList()` | Sort terrain tmap list |
| `0x4C6040` | `T_GetLeaf(…)` | Get terrain leaf node at position |

### Music / Sequencer (.MUS, .XMI)

| Address | Symbol | Role |
|---------|--------|------|
| `0x432920` | `InitMusic()` / `ShutDownMidi()` | Miles Sound System MIDI init/shutdown |
| `0x4329A0` | `DMusicOn(char*, float)` | Load and start `.MUS` playlist |
| `0x432A90` | `MusicOn(char*, float)` | Load and start music by name |
| `0x432B40` — `0x432C00` | `MusicVolume(…)` / `DMusicOff()` / `MusicOff()` | Volume / stop |
| `0x432C30` | `ScoreOn(void*, char)` | Start `.XMI` sequence via AIL |
| `0x446B70` | `SEQmusic` | SEQ script music command dispatcher |

### Sequence Scripts (.SEQ)

| Address | Symbol | Role |
|---------|--------|------|
| `0x44F70` | `SeqInit` | Initialize sequencer |
| `0x445060` | `SeqStart` | Begin SEQ playback |
| `0x445D30` | `SeqStop` | Stop SEQ |
| `0x445700` | `SeqContinue` | Resume/step SEQ |
| `0x446C70` | `SEQsound` / `SEQsndoff` | SEQ audio commands |
| `0x446A50` | `SEQfont` | SEQ font command |
| `0x446BE0` | `SEQpalette` | SEQ palette command |
| `0x447090` | `SEQvideo` | SEQ video command |
| `0x4454D0` | `SeqSubstitute(…)` | Variable substitution in SEQ text |

### Mission Map (.MM)

| Address | Symbol | Role |
|---------|--------|------|
| `0x47A130` | (Ghidra: `LibFileExists`) | MM text keyword parser |
| `0x4B4370` | `WRInit(…)` | Dispatcher for `.LAY` lines in `.MM` |
| `0x4A7D70` | `T_ImmediateVisibility(…)` | Terrain visibility update from MM |

### Modem DB / Serial config

| Address | Symbol | Role |
|---------|--------|------|
| `0x4B9BA0` | `ReadModemDB()` | Read modem database file |
| `0x4B9BD6` | `WriteModemEntry(…)` | Write modem entry to file |
| `0x4B9DC0` | `SelectModemFromDB(CN_INFO*)` | Select modem from parsed DB |
| `0x4B9BF0` | `WriteModemFile(CN_INFO*)` | Write modem config file |

---

*Generated from FA.SMS (3,829 symbols). Addresses are virtual addresses in the FA.EXE address space (ImageBase 0x00400000).*
