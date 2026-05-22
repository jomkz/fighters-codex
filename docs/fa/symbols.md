# FA.EXE Symbol Map â€” Organized Reference

FA.SMS ships with Jane's Fighters Anthology and contains 3,829 MSVC C++ mangled symbols with virtual addresses spanning `0x00401000`â€“`0x005937E0`. This document organizes them by address range into functional subsystems and highlights format-related entry points.

---

## Summary Table

| Subsystem | Address Range | Approx. Symbol Count |
|-----------|--------------|---------------------|
| Network (NET/MP/SER) | 0x401000â€“0x409000 | ~60 |
| HUD / cockpit display | 0x405E30â€“0x40AE50 | ~40 |
| Core shell / menu | 0x40AE50â€“0x421C70 | ~90 |
| Sound / music | 0x432920â€“0x435F80 | ~70 |
| Memory manager (MM) | 0x435C60â€“0x436320 | ~35 |
| Campaign map (MAP/CAM) | 0x421C70â€“0x42B800 | ~25 |
| Collision (COL) | 0x42B800â€“0x42E690 | ~20 |
| Flight model (FM/HARD) | 0x451480â€“0x454800 | ~80 |
| Video decode (FMV/Cobra) | 0x456300â€“0x45D090 | ~45 |
| Network (UDP/PKT layer) | 0x45D090â€“0x45DBD0 | ~30 |
| Graphics low-level (GG/G_) | 0x45DBD0â€“0x499380 | ~130 |
| Wingman/Group AI (WNG/GRP) | 0x45E460â€“0x460FB0 | ~50 |
| Object system (OBJ/chain) | 0x462600â€“0x464C80 | ~40 |
| AI interpreter (CT) | 0x464C80â€“0x467110 | ~120 |
| Pilot / mission / campaign | 0x467110â€“0x490000 | ~180 |
| Joystick / serial / modem | 0x494270â€“0x4AC510 | ~110 |
| Terrain renderer (T_) | 0x4A6E50â€“0x4C5D70 | ~90 |
| Projectile / weapons (PROJ) | 0x4C0690â€“0x4C5D30 | ~55 |
| 3D renderer (GR/render) | 0x4C5D70â€“0x4D5C00 | ~100 |
| Airport / carrier (AP) | 0x4BA750â€“0x4BEE60 | ~40 |
| World render / palette (WR) | 0x4B3010â€“0x4B4B30 | ~30 |
| Multiplayer protocol (MP) | 0x46ADE0â€“0x473680 | ~95 |
| Dialog / UI shell | 0x487A3Aâ€“0x48D200 | ~70 |
| SAY / voice callout | 0x48D2B0â€“0x491240 | ~20 |
| CRT / Win32 imports | 0x4D6F5Câ€“0x4E8B66 | ~300 |
| Data globals / BSS | 0x4EB5F4â€“0x593800 | ~300 |

---

## Subsystem Sections

### Network â€” Master/Slave, UDP, TCP, SPX, SER (0x401000â€“0x409000 + scattered)

Low addresses are NET slave/master negotiation and high-level wrappers; UDP/TCP/SPX factories and serial modem code are in 0x44xxxâ€“0x4Bxxx.

Key functions:
- `0x4016C0` â€” `NET_SlaveInit(CN_INFO*, NET_ADDRESS*, â€¦)`
- `0x401780` â€” `NET_SlaveShutdown()`
- `0x4017B0` â€” `NET_RequestPlayerList(â€¦)`
- `0x401850` â€” `NET_CancelPlayerList()`
- `0x401880` â€” `PlayerListQueryEvents(â€¦)`
- `0x401B20` â€” `slave_events(â€¦)`
- `0x401EB0` â€” `slave_process_pkt(NET_PKT*, socket_state*)`
- `0x402320` â€” `state_func_slave_connecting()`
- `0x40AE50` â€” `NET_MasterInit(â€¦)`
- `0x40AF40` â€” `state_func_master_query()`
- `0x40AFF0` â€” `NET_MasterRejectPlayer(NET_ADDRESS*, char*)`
- `0x40B080` â€” `NET_MasterShutdown()`
- `0x44BAF0` â€” `SER_EnterCriticalCodeForeground()`
- `0x44BB70` â€” `SER_ForegroundCheckConnection(â€¦)`
- `0x44C470` â€” `SER_Initialize(CN_INFO*, long)` (+ Init1â€“5)
- `0x44CCA0` â€” `SER_Shutdown()`
- `0x4B0830` â€” `NET_Initialize(CN_INFO*, long)`
- `0x4B0A10` â€” `NET_Shutdown()`
- `0x4B0AC0` â€” `NET_Often()`
- `0x4B0BD0` â€” `NET_Synchronize()`
- `0x4B0CF0` â€” `NET_Write(â€¦)` / `NET_Read(â€¦)` / `NET_Flush(â€¦)`
- `0x4B1350` â€” `NET_SendMessageAll(char*)`
- `0x4B1540` â€” `NETProcessEvent(â€¦)`

---

### HUD / Cockpit Display (0x405E30â€“0x40AE50)

- `0x405E30` â€” `HUDInitMessages()`
- `0x405E50` â€” `HUDDrawMessages(char)`
- `0x405F50` â€” `HUDMessage(â€¦)`
- `0x406040` â€” `HUDInit()`
- `0x406950` â€” `HUDShutdown()`
- `0x406A50` â€” `HUDDraw(char)`
- `0x4077B0` â€” `HUDSetWarning(â€¦)`
- `0x407B60` â€” `HUDDrawHeading()`
- `0x407EE0` â€” `HUDDrawSpeed()`
- `0x408420` â€” `HUDDrawAlt()`
- `0x408E20` â€” `HUDDrawHVel()`
- `0x409030` â€” `HUDDrawWeaponInfo()`
- `0x4092D0` â€” `HUDDrawRangeInfo()`
- `0x40A450` â€” `HUDSquawk()`
- `0x40A530` â€” `HUDFindNearest(â€¦)`
- `0x40ABB0` â€” `HUDDrawDisrupt()`
- `0x40AC80` â€” `HUDSetStability(long)`
- `0x40ACE0` â€” `HUDDrawStability()`
- `0x40AE40` â€” `HUDHasFlaps(char)`

---

### Core Shell / Menu (0x40B080â€“0x421C70)

Shell setup, menu creation, mouse handling, view slew, damage system.

- `0x40B8A0` â€” `MouseLoadPtr()`
- `0x40BA10` â€” `ShellSetup()`
- `0x40BD30` â€” `MenuStartUp(â€¦)`
- `0x40C1F0` â€” `MenuCreateRemaps()`
- `0x40C290` â€” `ShellOff()`
- `0x40C310` â€” `MenuShutDown(char)`
- `0x40C4F0` â€” `MenuUpdate()`
- `0x40CFE0` â€” `ShadowBox(long,long,long,long)`
- `0x40D7A0` â€” `VIEWSlew(â€¦)`
- `0x40F6B0` â€” `DAMAGEInit2()` / `0x40F760` â€” `DAMAGEInit()`
- `0x40F970` â€” `DAMAGEDoHit(â€¦)`
- `0x4113C0` â€” `DAMAGEAutopilotAvail()`
- `0x411A40` â€” `Angles(â€¦)` / `AngleOffNose(â€¦)` / `AnglesOffNose(â€¦)`
- `0x411BD0` â€” `Clock(â€¦)`
- `0x4120C0` â€” `Move3d(â€¦)`
- `0x412C10` â€” `PlaySeq(char*, long, long)`
- `0x413120` â€” `CHATInit()` / `CHATKey(â€¦)` / `CHATEndMission()`
- `0x4140A0` â€” `SetPlayerTarget(â€¦)` / `TargetNearestTo(â€¦)`
- `0x414690` â€” `FlightKey(â€¦)`
- `0x416380` â€” `SetAutopilot(â€¦)` / `ForceAutopilot(â€¦)`
- `0x4164B0` â€” `ServicePlayer()`
- `0x417760` â€” `InitPlayerControl()`
- `0x418070` â€” `MSGInit()` / `MSGSend(â€¦)` / `MSGReceive(â€¦)`
- `0x419800` â€” `ArmPlane(â€¦)`
- `0x41D740` â€” `CDirDraw::CreateSingleton()` / `CDirDraw::Create(â€¦)`
- `0x41E8F0` â€” `IsBrentDLL(void*)` â€” tests Phar Lap PE signature
- `0x41E910` â€” `IsDLL(â€¦)`
- `0x41EB60` â€” `LoadDLL(â€¦)` â€” generic overlay DLL loader
- `0x41F240` â€” `LoadBrentDLL(â€¦)` â€” loads Phar Lap PE overlay

---

### Sound / Music (0x432920â€“0x435F80)

- `0x432920` â€” `ShutDownMidi()`
- `0x4329A0` â€” `DMusicOn(char*, float)` / `MusicOn(char*, float)`
- `0x432C30` â€” `ScoreOn(void*, char)` / `ScoreOff()` / `ScoreUpdate()`
- `0x432F80` â€” `ShellMusicUpdate(long)` / `ShellMusic(char)`
- `0x433180` â€” `InitSound()` / `ShutDownSndDriver()` / `InitMixer()`
- `0x433480` â€” `SoundPoints()`
- `0x433580` â€” `SingleSound(char*, float, â€¦)`
- `0x433680` â€” `SoundOn(â€¦)` â€” full parameter sound trigger
- `0x433CE0` â€” `SoundOff(short)`
- `0x433D40` â€” `SoundAllOff()`
- `0x434800` â€” `MaybeLoopSound(â€¦)` / `UpdateLoopSounds()`
- `0x4349D0` â€” `ServiceSounds()`
- `0x435480` â€” `StopGameSounds()`
- `0x435980` â€” `StartVoice(MODSPEC*, short)`
- `0x435B80` â€” `SoundStatus()` / `SndLostFocus()` / `SndGotFocus()`

---

### Memory Manager (MM) (0x435C60â€“0x436320)

- `0x435C60` â€” `MMInit(â€¦)` / `MMShutdown()`
- `0x435D80` â€” `MMAllocHandle(â€¦)` / `MMMapFile(â€¦)` / `MMFreeHandle(â€¦)`
- `0x435F80` â€” `MMFreePtr(â€¦)` / `MMReallocHandle(â€¦)` / `MMReallocPtr(â€¦)`
- `0x436170` â€” `MMPushAllocId(â€¦)` / `MMPopAllocId()` / `MMFreeAllId(â€¦)`
- `0x436210` â€” `MMLock(â€¦)` / `MMUnlock(â€¦)` / `MMAccessR(â€¦)` / `MMAccessW(â€¦)`
- `0x4362C0` â€” `MMAreaFree()`

---

### Campaign Map (MAP/ZONE) (0x421C70â€“0x42B800)

- `0x421C70` â€” `ZONEInit()` / `ZONEAdd(â€¦)` / `ZONEForGV()` / `ZONEUpdate()`
- `0x4221D0` â€” `MAPObjAlts(â€¦)` / `MAPSetSide(â€¦)` / `MAPMaybeSetControl(â€¦)`
- `0x422380` â€” `MAPWorldToScreen(F24_POINT3*, WORD_POINT*)`
- `0x4223BE` â€” `MAPDrawGrid()`
- `0x4224EE` â€” `MAPDrawBG()`
- `0x42267F` â€” `MAPUpdateWPPtrs(â€¦)` / `MAPSetNewWP(â€¦)`
- `0x422851` â€” `MAPDrawSpecials()` / `MAPOnSpecial(â€¦)`

---

### Collision (COL) (0x42B800â€“0x42E690)

- `0x42B800` â€” `Collision(â€¦)` â€” main collision check
- `0x42BD30` â€” `COLSetAngle(â€¦)`
- `0x42DDA0` â€” `COLFlatGround(â€¦)`
- `0x42DF80` â€” `COLPitchToAvoidTerrain()`
- `0x42E0C0` â€” `COLGetInfo(â€¦)` / `COLGetBox(â€¦)`
- `0x42E4E0` â€” `COLTerrainBlocking(â€¦)`
- `0x42E530` â€” `COLInit()` / `COLAddObj()` / `COLRemoveCurObj()`

---

### Flight Model / Hardpoints (FM/HARD) (0x451480â€“0x454800)

- `0x4514C0` â€” `FMUpdateGearPitch()` / `FMUpdateGear()` / `FMUpdateWingSweep()`
- `0x4516B0` â€” `FMGetWeight()`
- `0x4518A0` â€” `FMInitPlane(â€¦)`
- `0x451B00` â€” `SetThrottle(â€¦)` / `FMFlaps(â€¦)` / `FMGear(â€¦)` / `FMBrakes(â€¦)`
- `0x451E50` â€” `FMFuelConsumption(â€¦)` / `BurnFuel()`
- `0x452140` â€” `FMUpdatePlaneFields()`
- `0x452770` â€” `HARDPtrs(â€¦)` â€” hardpoint pointer resolver
- `0x4527F0` â€” `HARDUnload(â€¦)` / `HARDLoad(â€¦)` / `HARDLoadAll()` / `HARDUnloadAll()`
- `0x452D90` â€” `HARDBestSeekers(â€¦)` / `HARDBestSeeker(â€¦)` / `HARDFindJammer(â€¦)`
- `0x452F80` â€” `HARDFindStore(â€¦)` / `HARDFindProj(â€¦)`
- `0x453440` â€” `HARDGunsOnlyAll()`
- `0x453AC0` â€” `HARDNumLoaded(â€¦)` / `HARDTotalFuel()`
- `0x453B90` â€” `HARDRearmTest()` / `HARDRearmHumanLoad()`
- `0x454140` â€” `ChangePlaneType(â€¦)` / `RepairTime(â€¦)` / `SelectRepairPlane(â€¦)`

---

### AI Interpreter (CT) (0x464C80â€“0x467110)

Full condition evaluator and action dispatcher for `.AI` scripts. All `CTEval_*` and `CTDo_*` are exported by `.BI` DLLs and resolved by name.

Selected `CTEval_*` (condition evaluators):
- `0x464E20` â€” `CTEval_time` / `CTEval_do_nothing` / `CTEval_do_evade` / `CTEval_do_attack`
- `0x464E60` â€” `CTEval_do_radar_launch` / `CTEval_do_ir_launch` / `CTEval_do_hit`
- `0x464F10` â€” `CTEval_tgt` / `CTEval_tgtclass` / `CTEval_tgtisfighter` / `CTEval_tgtisbomber`
- `0x464FF0` â€” `CTEval_tgtisship` / `CTEval_tgtissam` / `CTEval_tgtisaaa`
- `0x465040` â€” `CTEval_maxrange` / `CTEval_bestrange` / `CTEval_radar` / `CTEval_ir`
- `0x465150` â€” `CTEval_tgtahead` / `CTEval_tgtfacing` / `CTEval_disttotgt`
- `0x4653A0` â€” `CTEval_speed` / `CTEval_minspeed` / `CTEval_cornerspeed` / `CTEval_maxspeed`
- `0x465480` â€” `CTEval_twr` / `CTEval_turnrate` / `CTEval_turnradius`
- `0x465510` â€” `CTEval_alt` / `CTEval_altdiff` / `CTEval_maxalt` / `CTEval_minalt`
- `0x465640` â€” `CTEval_disttowaypoint` / `CTEval_skill` / `CTEval_engagep`

Selected `CTDo_*` (action executors):
- `0x465A30` â€” `CTDo_exit` / `CTDo_restart` / `CTDo_maneuver` / `CTDo_play`
- `0x465CC0` â€” `CTDo_move` / `CTDo_movetoalt` / `CTDo_turn`
- `0x466052` â€” `CTDo_yoyo` / `CTDo_circle` / `CTDo_homeangle` / `CTDo_homepos`
- `0x4663F0` â€” `CTDo_jink` / `CTDo_invert` / `CTDo_btoh` / `CTDo_immelman`
- `0x4665E0` â€” `CTDo_wm_break` / `CTDo_wm_approach` / `CTDo_wm_formation`
- `0x466970` â€” `CTExecProgram(â€¦)` â€” `.AI` interpreter loop

---

### Pilot / Mission / Campaign (0x467110â€“0x490000)

- `0x467180` â€” `PilotSave(PILOT*, short)` / `PilotPhoto(PILOT*)`
- `0x467310` â€” `CallsignChoose(PILOT*, long)` / `EditPilot(â€¦)`
- `0x468020` â€” `PilotScreen(â€¦)`
- `0x4692D0` â€” `EJECTProc` / `EJECTAdd(â€¦)` / `EJECTRemove()`
- `0x4754B0` â€” `PilotSave(â€¦)` â€” save pilot to .PLT file
- `0x480750` â€” `_MISSIONInit1()` / `_MISSIONInit2()`
- `0x480B40` â€” `MISSIONInit1()` / `MISSIONInit2()` / `MISSIONInit3()`
- `0x480C20` â€” `LoadCampaignProc`
- `0x480C40` â€” `InitCampaignPilot`
- `0x480C90` â€” `AddCampaignPlane`
- `0x480D70` â€” `CampaignPlanesLeft()`
- `0x480DF0` â€” `UkraineAddA7` â€” per-theater campaign hook
- `0x481150` â€” `AtFriendlyAP()`
- `0x481320` â€” `CampaignSave` / `CampaignOff`
- `0x481440` â€” `CallCampaignProc(â€¦)` / `CallMissionProc(â€¦)`
- `0x4819F0` â€” `MISSIONShutdown()` / `MISSIONSuccess()`
- `0x4851C0` â€” `MISSIONFortDestroyed(â€¦)` â€” fort destruction logic
- `0x485260` â€” `MISSIONFortDestroyedByFort(â€¦)`
- `0x486010` â€” `MISSIONLoadCommonResources()`
- `0x486160` â€” `MISSIONEndScenario()`
- `0x486860` â€” `MISSIONCheckSuccess()`
- `0x4869A0` â€” `TIMESystemTime()` / `TIMEInit(â€¦)` / `TIMEUpdate()`

---

### Object System / Entity Chain (0x462600â€“0x464C80)

- `0x462600` â€” `InitChain()` / `RemoveFromChains()` / `ImmediateService()`
- `0x4627B0` â€” `RemoveCurObj()` / `GetCurObj(â€¦)` / `PutCurObj()`
- `0x4629E0` â€” `PushCurObj(â€¦)` / `PopCurObj()`
- `0x462A50` â€” `ServiceObjects`
- `0x462E70` â€” `Service()` â€” main per-frame service
- `0x463980` â€” `MaybeCallEventProc(â€¦)` / `CallEventProc(â€¦)`
- `0x463A20` â€” `CreateMove(â€¦)` / `CreateMoveGoal(â€¦)`
- `0x463F60` â€” `CallUtilProc` â€” dispatches to OBJ/GV/PROJ proc
- `0x464040` â€” `Reaction(â€¦)` / `EnterState(â€¦)`
- `0x473A40` â€” `OBJEventProc` / `OBJDamageProc(HIT_OBJ_DATA*)`
- `0x473BE0` â€” `OBJProc` â€” static object update
- `0x473C10` â€” `Kill()`
- `0x473DB0` â€” `GVProc` â€” ground vehicle update
- `0x491240` â€” `OBJGet(â€¦)` / `OBJInit(â€¦)` / `OBJShutdown()`
- `0x491300` â€” `OBJAlloc(â€¦)` / `OBJAdd(â€¦)` / `OBJSubtract()`
- `0x4914C0` â€” `OBJAlias(â€¦)` â€” alias lookup (used by .MC DLLs)

---

### Wingman / Group AI (WNG/GRP) (0x45E460â€“0x460FB0)

- `0x45E460` â€” `WNGInit()` / `WNGAdd(â€¦)` / `WNGWingmen(â€¦)` / `WNGPart(â€¦)`
- `0x45E8F0` â€” `WNGLeaderLanding()` / `WNGFormationMove(â€¦)` / `WNGSendWM(â€¦)`
- `0x45F190` â€” `GRPInit()` / `GRPAdd(â€¦)` / `GRPRemove()`
- `0x45F360` â€” `GRPLeader(â€¦)` / `GRPWingman(â€¦)` / `GRPWingmenNearby(â€¦)`
- `0x45F580` â€” `GRPSetWaypoints(â€¦)` / `GRPControl(â€¦)` / `GRPLeaderLanding()`
- `0x45F7F0` â€” `GRPSetControl(â€¦)` / `GRPSetType(â€¦)` / `GRPSetSpacingH/V(â€¦)`
- `0x45FE30` â€” `GRPName(â€¦)`
- `0x45FEC0` â€” `INFO2Draw()`
- `0x46A370` â€” `SMInit()` / `SMShutdown()` / `SMAddress(â€¦)` / `SMCallByName`

---

### Airport / Carrier (AP) (0x4BA750â€“0x4BEE60)

- `0x4BA750` â€” `APInit()` / `APAdd(â€¦)` / `APDelete(â€¦)` / `APNearest(â€¦)`
- `0x4BAA10` â€” `APTakeoffType(â€¦)` / `APLandingType(â€¦)`
- `0x4BADB0` â€” `APTakeoff()`
- `0x4BC210` â€” `APStartFinalApproach()` / `APEndArrestorCatch()` / `APLanding()`
- `0x4BD2D0` â€” `APFind(â€¦)` / `APClearParks()` / `APGetPark()` / `APAssignPark()`
- `0x4BD5B0` â€” `CARRIERProc`
- `0x4BE640` â€” `STRIPProc` / `APApproachPath(â€¦)` / `APTeleport`
- `0x4BEB00` â€” `APAddToCarrier(â€¦)` / `APRemoveFromCarrier()` / `APCheckCarrier()`
- `0x4BED70` â€” `APHomeAirport()` / `APObjOnShip(â€¦)`

---

### World Render / Palette / Layer (WR) (0x4B3010â€“0x4B4B30)

See architecture.md for the full per-frame update pipeline. These functions implement the atmosphere/sky system that consumes loaded `.LAY` DLL data.

- `0x4B3190` â€” `WRGetLayer(â€¦)` / `WRSetRemaps(â€¦)`
- `0x4B3480` â€” `WRUpdate(â€¦)` â€” transition atmosphere parameters
- `0x4B3D90` â€” `WRUpdatePalette()` â€” per-frame palette smooth-transition (= `UpdateSkyState`)
- `0x4B4170` â€” `WRLightUpdate()`
- `0x4B4320` â€” `WRFogLayerUpdate` â€” per-frame fog density jitter
- `0x4B4370` â€” `WRInit(â€¦)` â€” loads LAY DLL, sets up atmosphere (= `ParseLayerFile`)
- `0x4B46D0` â€” `WRShutdown()`
- `0x4B4720` â€” `WRWeatherEffects`
- `0x4B47B0` â€” `SetTmapRemaps()`
- `0x4B4990` â€” `WRLensFlare()` / `WRCanSee(â€¦)`
- `0x4C8E20` â€” `WRBlackenPalette(â€¦)` / `WRWhitenPalette(â€¦)` / `WRReddenPalette(â€¦)` / `WRColorPalette(â€¦)`

---

### Projectile / Weapons (PROJ) (0x4C0690â€“0x4C5D30)

- `0x4C06A0` â€” `PROJInit()` / `PROJGetTargetPos(â€¦)` / `PROJAccurateHardPos(â€¦)`
- `0x4C0870` â€” `PROJSetTarget(â€¦)` / `PROJLockUpdate()`
- `0x4C0A90` â€” `PROJAdd(â€¦)` â€” spawn projectile
- `0x4C1170` â€” `PROJEngineState()`
- `0x4C11B0` â€” `PROJMoveProc(char)` / `PROJDamageProc(HIT_OBJ_DATA*)`
- `0x4C1F50` â€” `PROJProc`
- `0x4C20C0` â€” `PROJHit(â€¦)` / `PROJFire(â€¦)` / `PROJFireSound(â€¦)`
- `0x4C2860` â€” `PROJInFOV(â€¦)` / `PROJRadarIsOn(â€¦)` / `PROJLock(â€¦)`
- `0x4C3380` â€” `PROJHitChance(â€¦)` / `PROJLaunchDevice(â€¦)`
- `0x4C3CA0` â€” `PROJRemove()` / `PROJRetargetMissiles(â€¦)`
- `0x4C3EB0` â€” `PROJMakeBombEq(â€¦)` / `PROJChangeBombEq(â€¦)` / `PROJBombPos(â€¦)`
- `0x4C4100` â€” `PROJSelectTarget()` / `PROJServiceWeapon(â€¦)`
- `0x4C5670` â€” `PROJSendCollateralDamages(â€¦)`

---

### Terrain Renderer (T_) (0x4A7310â€“0x4C5D70)

- `0x4A6E50` â€” `LoadPIC` â€” bitmap load dispatcher
- `0x4A6EB0` â€” `SetupOT` / `SetupNT` / `SetupPT` / `SetupJT` â€” BRF object type setup
- `0x4A7310` â€” `T_InitPlane(â€¦)` / `T_AddObj(â€¦)` / `T_AddYourObjs()`
- `0x4A7D70` â€” `T_ImmediateVisibility(â€¦)` / `T_ObjList(â€¦)` / `T_Render(â€¦)`
- `0x4A7F20` â€” `T_InitForestProc` / `T_ForestProc(long)`
- `0x4A8660` â€” `T_InitFarmProc` / `T_FarmProc(long)` / `T_InitMooseProc` / `T_MooseProc(long)`
- `0x4A8870` â€” `T_InitVietRicePaddy1â€“3Proc` / `T_VietPalms1â€“3Proc` / `T_VietTrees1â€“3Proc`
- `0x4A8A70` â€” `T_InitWaterProc` / `T_WaterProc(long)` / `T_InitCloudProc` / `T_CloudProc(long)`
- `0x4A8D30` â€” `T_Normal(â€¦)` / `T_LeafOp(â€¦)` / `T_Make(â€¦)`
- `0x4AA620` â€” `T_InitDictionary()` / `T_InitDictionaryEntry(â€¦)` / `T_NamedTmaps()`
- `0x4AACF0` â€” `T_DefaultHorizon` / `T_HorizonProc` â€” exported as `T_HorizonProc` from FA.EXE
- `0x4C5D60` â€” `T_Init()` / `T_Load(â€¦)` / `T_Init2()` / `T_Shutdown()` / `T_StopAdding()`
- `0x4C6040` â€” `T_GetLeaf(â€¦)`

---

### 3D Renderer (GR/render) (0x4C5D70â€“0x4D5C00)

- `0x4D5B64` â€” `GRInit3d(â€¦)` / `GRRender(â€¦)` / `GRSinCos(â€¦)` / `GRTo2d(â€¦)`
- `0x4D5E58` â€” `MakeObjRotationMatrix(â€¦)` / `MakeViewRotationMatrix(â€¦)` / `MultPointByMatrix(â€¦)`
- `0x4D6348` â€” `GRSaveContext()` / `GRRestoreContext()` / `GRExec(â€¦)`
- `0x4D64D8` â€” `MultF24PointByMatrix(â€¦)` / `Sqrt(â€¦)`
- `0x4D057C` â€” `GRAddBrentObj(â€¦)` â€” add BRF object to render queue
- `0x4CD834` â€” `GRSetLightSource(â€¦)` / `SetShading`
- `0x4CD8B0` â€” `Sun` â€” sun direction update
- `0x4CDCB8` â€” `render_3d` â€” main 3D render entry
- `0x4CE980` â€” `dmxmul` / `dmxmul2` â€” matrix multiply helpers
- `0x4CC4B4` â€” `SetShadingTable` (= `SetActiveLayerByAngle`)
- `0x4CCB88` â€” `ArcTan(â€¦)`

Low-level shape dispatch opcodes (interpreter for .SH bytecode):
- `0x4D2180` â€” `must_clip_3d`
- `0x4D22A8` â€” `do_sfcal_long`
- `0x4D22D4` â€” `do_ifdestroyed` â€” destruction-state conditional
- `0x4D2380` â€” `do_if_not_effect`
- `0x4D33D8` â€” `do_icall_long` / `do_jumpfar4`
- `0x4D42EC` â€” `do_setcoarse` / `do_set_point_color` / `do_set_gouraud`
- `0x4D43DC` â€” `do_new_poly` / `do_new_smap` / `do_new_rmap` / `do_new_pmap_or_tmap`
- `0x4D47B8` â€” `do_streamer_def` / `do_streamer_draw`

---

### Dialog / UI Shell (0x487A3Aâ€“0x48D200)

- `0x487A63` â€” `DialogSetup(â€¦)` / `DialogShow()` / `DialogShutDown(â€¦)` / `DialogDone()`
- `0x488470` â€” `DialogUpdate(â€¦)` / `DialogWhatItem()`
- `0x4892E0` â€” `DialogGetPtr(â€¦)` / `DialogGetValue(â€¦)` / `DialogSetValue`
- `0x489400` â€” `DialogSetRocker(â€¦)` / `DialogSetString(â€¦)` / `DialogGetString(â€¦)`
- `0x489AC0` â€” `DrawText` â€” imported by .MNU/.DLG overlays as `main.dll::_DrawText`
- `0x489B90` â€” `DrawAction` â€” imported by overlays as `main.dll::_DrawAction`
- `0x48A730` â€” `DrawLight` / `DrawFormattedText` / `DrawMissList` / `DrawCampaignList`
- `0x48B4E0` â€” `DrawRocker` / `DrawToggle` / `DrawSliderHoriz` / `DrawSliderVert`
- `0x48C710` â€” `DrawEditBox`

---

### SAY / Voice Callout (0x48D2B0â€“0x491240)

- `0x48D2B0` â€” `SAYInit()` / `SAYInit2()` / `SAYShutdown()`
- `0x48D350` â€” `SAYMsg(â€¦)` / `SAYDefaultSayProc`
- `0x48D780` â€” `PLANESayProc`
- `0x48E8D0` â€” `OBJSayProc`
- `0x48E920` â€” `SAYRearmMessage(â€¦)` / `SAYSuppRadarMessage(â€¦)` / `SAYLowFuelMessage(â€¦)`
- `0x48EC40` â€” `PLANECommentProc`
- `0x48F6A0` â€” `APCommentProc`
- `0x490F30` â€” `SAYTranslate(â€¦)` / `SAYFortAircraft` / `SAYFortStatus`

---

### Graphics Low-Level (G_/GG) (0x45DBD0â€“0x499380)

- `0x45DBD0` â€” `GG_InitMode()` / `GG_ShutdownMode()` / `GG_GetMode()`
- `0x45DE70` â€” `GG_SetPalette(â€¦)` / `GG_Shake()` / `GG_Flush(â€¦)`
- `0x497340` â€” `G_Init()` / `G_Shutdown()`
- `0x4974E0` â€” `G_SetBitmap(â€¦)` / `G_SetClipBox(â€¦)` / `G_SetColor(â€¦)`
- `0x497700` â€” `G_UHline(â€¦)` / `G_Hline(â€¦)` / `G_Vline(â€¦)` / `G_Line(â€¦)`
- `0x497D40` â€” `G_UBox(â€¦)` / `G_Box(â€¦)` / `G_Rect(â€¦)`
- `0x4983E0` â€” `G_DrawYLR(â€¦)` / `G_Flush(â€¦)` / `G_Flip(â€¦)`
- `0x4986A0` â€” `G_SetFont(â€¦)` / `G_Print(â€¦)` / `G_Printf`
- `0x498A30` â€” `G_LoadDriver(â€¦)` / `G_UnloadDriver()`
- `0x4B7930` â€” `G_RelocBitmap(â€¦)` / `G_AllocBitmap(â€¦)` / `G_AllocSurfaceBitmap(â€¦)`
- `0x4B7CD0` â€” `G_LoadBitmap(â€¦)` â€” load PIC from LIB
- `0x4B7FA0` â€” `G_BlitToScreen(â€¦)` / `G_Blit(â€¦)`
- `0x4B87C0` â€” `G_Texture(â€¦)` / `G_AcTexture(â€¦)` / `G_PerspectiveFlip(â€¦)`
- `0x4B9430` â€” `NPM_FlatTri(â€¦)` / `NPM_TextureLinearTri(â€¦)` / `NPM_TexturePerspectiveTri(â€¦)`

---

## Format Loaders and Parsers

Cross-reference of symbols that directly load, initialize, or parse named file formats.

### LIB Archive (EALIB)

| Address | Symbol | Role |
|---------|--------|------|
| `0x47A090` | `LibSeek(â€¦)` | Seek within open LIB entry |
| `0x47A130` | `LibFileExists(â€¦)` | Test for named entry |
| `0x479BD0` | `LibOpen(â€¦)` | Open a named LIB entry |
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
| `0x41E910` | `IsDLL(â€¦)` | Generic DLL validity check |
| `0x41EB60` | `LoadDLL(â€¦)` | Load and IAT-patch an overlay DLL |
| `0x41F240` | `LoadBrentDLL(â€¦)` | Load Phar Lap PE overlay (CAM/BI/MC) |
| `0x4B4370` | `WRInit(â€¦)` | Load `.LAY` file via `LoadLibrary` + IAT patch |
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
| `0x4AE440` | `PlayVDOString(char*, â€¦)` | Play FMV by filename |
| `0x4AE406` | `PlayVDOFile(char*, â€¦)` | Play FMV from open file |
| `0x4AF070` | `StartVDOAudio(char*)` | Start audio stream for VDO |
| `0x4AF1B0` | `OpenVDOFile(char*)` | Open a `.VDO` file |
| `0x4AF200` | `ReadVDOHeader(â€¦)` | Parse `.VDO` file header |
| `0x4AF2D0` | `ReadFrameSizesFile(char*)` | Read `.FBC` companion sizes |
| `0x4AF320` | `ReadVDOPalette(â€¦)` | Extract palette from VDO header |
| `0x4AF3A0` | `AllocVDO(VDO*)` | Allocate VDO playback context |
| `0x4AE4E0` | `BuildVDOList(char*)` | Build linked list of VDO files |
| `0x4AED50` | `VDOSetMode(VDO*)` | Set video decode mode |
| `0x442360` | `InitMovieContext(MovieContext*, â€¦)` | Init Cobra codec context |
| `0x442370` | `DecodeFrame(MovieContext*, â€¦)` | Decode one Cobra video frame |

### Terrain (.T2)

| Address | Symbol | Role |
|---------|--------|------|
| `0x4C5D60` | `T_Init()` | Initialize terrain database |
| `0x4C5D70` | `T_Load(â€¦)` | Load `.T2` terrain file |
| `0x4C5D50` | `T_ShutdownDatabase()` / `T_Init2()` / `T_Shutdown()` | Lifecycle |
| `0x4AA620` | `T_InitDictionary()` | Set up terrain tile dictionary |
| `0x4AA680` | `T_InitDictionaryEntry(â€¦)` | Add `.T2` tile entry |
| `0x4AA7E0` | `T_CompareTlist(â€¦)` / `T_SortTmapList()` | Sort terrain tmap list |
| `0x4C6040` | `T_GetLeaf(â€¦)` | Get terrain leaf node at position |

### Music / Sequencer (.MUS, .XMI)

| Address | Symbol | Role |
|---------|--------|------|
| `0x432920` | `InitMusic()` / `ShutDownMidi()` | Miles Sound System MIDI init/shutdown |
| `0x4329A0` | `DMusicOn(char*, float)` | Load and start `.MUS` playlist |
| `0x432A90` | `MusicOn(char*, float)` | Load and start music by name |
| `0x432B40` â€” `0x432C00` | `MusicVolume(â€¦)` / `DMusicOff()` / `MusicOff()` | Volume / stop |
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
| `0x4454D0` | `SeqSubstitute(â€¦)` | Variable substitution in SEQ text |

### Mission Map (.MM)

| Address | Symbol | Role |
|---------|--------|------|
| `0x47A130` | (Ghidra: `LibFileExists`) | MM text keyword parser |
| `0x4B4370` | `WRInit(â€¦)` | Dispatcher for `.LAY` lines in `.MM` |
| `0x4A7D70` | `T_ImmediateVisibility(â€¦)` | Terrain visibility update from MM |

### Modem DB / Serial config

| Address | Symbol | Role |
|---------|--------|------|
| `0x4B9BA0` | `ReadModemDB()` | Read modem database file |
| `0x4B9BD6` | `WriteModemEntry(â€¦)` | Write modem entry to file |
| `0x4B9DC0` | `SelectModemFromDB(CN_INFO*)` | Select modem from parsed DB |
| `0x4B9BF0` | `WriteModemFile(CN_INFO*)` | Write modem config file |

---

*Generated from FA.SMS (3,829 symbols). Addresses are virtual addresses in the FA.EXE address space (ImageBase 0x00400000).*
