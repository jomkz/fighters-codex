# FA Struct Reference

Master struct reference for Jane's Fighters Anthology, recovered via Ghidra static analysis of FA.EXE. All offsets were derived by `RecoverStructs.java` scanning the executable for field-access patterns against known struct pointer arguments.

**Confidence notation:**

- **High** â€” accessor function name clearly indicates field meaning (e.g., `MPSetFuel`, `HUDDrawAlt`, `DAMAGEDoHit`)
- **Low** â€” only generic `FUN_*` accessors; field name is inferred from access context or is unknown

All sizes are in bytes. Multi-byte integer types are little-endian. Fixed-point values use FA's standard F24.8 format (24-bit integer part, 8-bit fraction) unless noted. The `entity` struct scan covered offsets `0x00`â€“`0x11E`; the raw scan logged 286 distinct offsets. The tables below present the most useful subset; unlisted offsets within a range are unaccessed or aliased to adjacent fields.

---

## 1. `entity` â€” Game Object Base

Every live game object (aircraft, missile, vehicle, static object, NPC, etc.) shares this common header. Allocated and managed by the object system (`_MoveObj@0`, `_explode`, `_T_AddObj@12`). At runtime a pointer to the current object is held in a global (`_currentT` or equivalent); subsystems such as HUD, damage, flight model, and AI all index off this pointer.

The struct begins at the object's base address. Extension structs (`PROJ_TYPE`, `GV_TYPE`, etc.) overlay the upper region of the same allocation.

**Scan range:** `0x400000`â€“`0x540000`, offsets `0x00`â€“`0x11E`

| Offset | Size | Field name (inferred) | Accessor functions | Notes |
|--------|------|-----------------------|--------------------|-------|
| `0x00` | 1 | `obj_type` | `_explode`, `NET_SlaveInit`, `NET_SlaveShutdown` | Object type/class byte. Highest access count (6434Ã—). **High** |
| `0x01` | 1 | `obj_flags` | `_explode`, `FUN_00401180`, `FUN_00401280` | Primary flags byte; 8288Ã— accesses. **Low** |
| `0x02` | 1 | `obj_flags2` | `_explode`, `FUN_00401280`, `FUN_00401430` | Secondary flags. **Low** |
| `0x03` | 1 | `unk_03` | `_explode`, `NET_SlaveInit`, `player_list_process_pkt` | **Low** |
| `0x04` | 4 | `obj_id` or `obj_ptr` | `_explode`, `FUN_00401180`, `FUN_00401280` | Most-accessed dword (10188Ã—); likely object index/handle. **Low** |
| `0x05` | 1 | `unk_05` | `FUN_00401a60`, `handle_slave_connection_failed` | **Low** |
| `0x06` | 1 | `unk_06` | `_explode`, `player_list_process_pkt` | **Low** |
| `0x07` | 1 | `unk_07` | `slave_process_pkt`, `HUDDrawHeading` | **Low** |
| `0x08` | 4 | `unk_08` | `FUN_00401180`, `FUN_00401280`, `FUN_004014b0` | 5673Ã— accesses. **Low** |
| `0x09` | 1 | `squawk_code` | `FlyingLoop`, `HUDDrawHeading`, `HUDSquawk@0` | Read by `@HUDSquawk@0`; IFF/squawk identifier. **High** |
| `0x0A` | 1 | `warning_flags` | `slave_process_pkt`, `HUDSetWarning@8`, `_HUDInit@0` | Written by `@HUDSetWarning@8`. **High** |
| `0x0B` | 1 | `unk_0B` | `usnfmain`, `HUDSetWarning@8`, `_DAMAGEInit@0` | **Low** |
| `0x0C` | 4 | `unk_0C` | `_explode`, `FUN_00401180`, `FUN_00401280` | 2938Ã— accesses. **Low** |
| `0x0D` | 1 | `unk_0D` | `usnfmain`, `_HUDInit@0`, `FUN_0040ebc0` | **Low** |
| `0x0E` | 1 | `unk_0E` | `player_list_process_pkt`, `usnfmain`, `_HUDInit@0` | **Low** |
| `0x0F` | 1 | `unk_0F` | `usnfmain`, `HUDDrawRangeInfo`, `FUN_00409760` | **Low** |
| `0x10` | 4 | `unk_10` | `_explode`, `FUN_00401180`, `NET_SlaveInit` | 4911Ã— accesses. **Low** |
| `0x14` | 4 | `unk_14` | `_explode`, `FUN_00401180`, `FUN_00401280` | 3399Ã— accesses. **Low** |
| `0x16` | 2 | `altitude` | `usnfmain`, `HUDDrawAlt`, `FUN_004089a0` | Read by `?HUDDrawAlt@@YIXXZ`. **High** |
| `0x17` | 1 | `speed_high` | `HUDDrawSpeed`, `_ServicePlayer@0`, `_MSGSend@32` | Read by `?HUDDrawSpeed@@YIXXZ`. Part of speed value. **High** |
| `0x18` | 4 | `unk_18` | `_explode`, `FUN_00401180`, `NET_SlaveInit` | 2555Ã— accesses. **Low** |
| `0x1C` | 4 | `unk_1C` | `_explode`, `FUN_00401180`, `NET_RequestPlayerList` | 1952Ã— accesses. **Low** |
| `0x1F` | 1 | `hvel_component` | `usnfmain`, `HUDDrawHVel`, `HUDDrawRangeInfo` | Read by `?HUDDrawHVel@@YIXXZ`. **High** |
| `0x20` | 4 | `unk_20` | `_explode`, `FUN_004014b0`, `handle_slave_connection_failed` | 2028Ã— accesses. **Low** |
| `0x21` | 1 | `damage_zone` | `_DAMAGEDoHit@12`, `_RotatedOffsetF24@20`, `MessagesToPlayer` | Written by `_DAMAGEDoHit@12`. **High** |
| `0x22` | 2 | `hud_init_flags` | `_HUDInit@0`, `_HUDShutdown@0`, `FUN_004089a0` | Read/written by HUD init/shutdown. **High** |
| `0x24` | 4 | `unk_24` | `_explode`, `FUN_00401180`, `NET_SlaveInit` | 1289Ã— accesses. **Low** |
| `0x27` | 1 | `zone_flags` | `_HUDInit@0`, `@ArmPlane@4`, `_Collision@56` | Read by zone/collision init. **Low** |
| `0x28` | 4 | `unk_28` | `_explode`, `NET_SlaveInit`, `NET_RequestPlayerList` | 1190Ã— accesses. **Low** |
| `0x29` | 1 | `net_slave_id` | `NET_SlaveInit`, `slave_process_pkt`, `NET_MasterInit` | Accessed in all slave/master NET functions. **High** |
| `0x2C` | 4 | `unk_2C` | `_explode`, `FUN_00401180`, `NET_SlaveInit` | 885Ã— accesses. **Low** |
| `0x30` | 4 | `unk_30` | `FUN_00401180`, `handle_slave_connection_failed`, `_HUDInit@0` | 844Ã— accesses. **Low** |
| `0x32` | 2 | `speed` | `HUDDrawSpeed`, `FUN_00409f30`, `FUN_0040b320` | Read repeatedly by `?HUDDrawSpeed@@YIXXZ`. **High** |
| `0x34` | 4 | `heading` | `slave_process_pkt`, `_HUDInit@0`, `HUDDrawHeading` | Read by `?HUDDrawHeading@@YIXXZ`. **High** |
| `0x36` | 2 | `unk_36` | `HUDDrawSpeed`, `FUN_004089a0` | **Low** |
| `0x38` | 4 | `unk_38` | `player_list_process_pkt`, `slave_process_pkt`, `FUN_004024d0` | 538Ã— accesses. **Low** |
| `0x3A` | 2 | `unk_3A` | `HUDDrawSpeed`, `FUN_004089a0` | **Low** |
| `0x3C` | 4 | `unk_3C` | `FUN_004024d0`, `_HUDInit@0`, `HUDDrawSpeed` | 532Ã— accesses. **Low** |
| `0x3F` | 1 | `chat_line` | `_CHATGetEditLine@0`, `_MSGSend@32` | Read by `_CHATGetEditLine@0`. **High** |
| `0x40` | 4 | `unk_40` | `_explode`, `usnfmain`, `_HUDInit@0` | 644Ã— accesses. **Low** |
| `0x44` | 4 | `unk_44` | `FlyingLoop`, `FUN_00406a5e`, `FUN_00409760` | 430Ã— accesses. **Low** |
| `0x48` | 4 | `unk_48` | `FUN_00402640`, `FUN_00406a5e`, `FUN_0040991b` | 233Ã— accesses. **Low** |
| `0x49` | 1 | `explosion_type` | `FUN_0043a5c0`, `_GRAPHICAddExp@28`, `_GRAPHICAddSmoke@28` | Written when creating explosion graphic. **High** |
| `0x4B` | 1 | `tv_key_state` | `FUN_0040f2d0`, `_DAMAGEDoHit@12`, `TVKey@@YIGG@Z` | Read by `?TVKey@@YIGG@Z`. **High** |
| `0x4C` | 4 | `unk_4C` | `FUN_00406a5e`, `FUN_00409760`, `FUN_00409bfb` | 252Ã— accesses. **Low** |
| `0x50` | 4 | `unk_50` | `FUN_00406a5e`, `FUN_00409760`, `FUN_00409bfb` | 336Ã— accesses. **Low** |
| `0x52` | 1 | `net_conn_id` | `NET_SlaveInit`, `FUN_00402640`, `NET_MasterInit` | Accessed in NET_SlaveInit at offset `0x52`. **High** |
| `0x54` | 4 | `unk_54` | `FUN_00406a5e`, `FUN_0040991b`, `HUDFindNearest@8` | 231Ã— accesses. **Low** |
| `0x58` | 4 | `unk_58` | `FUN_00406a5e`, `FUN_00409910`, `FUN_0040991b` | 156Ã— accesses. **Low** |
| `0x5A` | 2 | `damage_state` | `FUN_004089a0`, `_DAMAGEDoHit@12`, `_DAMAGEUpdate@0` | Read/written by damage system. **High** |
| `0x5C` | 4 | `unk_5C` | `usnfmain`, `FUN_00406a5e`, `FUN_00409bfb` | 143Ã— accesses. **Low** |
| `0x60` | 4 | `unk_60` | `FUN_0040991b`, `FUN_00409bfb`, `MenuCreateRemaps` | 166Ã— accesses. **Low** |
| `0x64` | 4 | `altitude_f24` | `slave_process_pkt`, `HUDDrawAlt`, `FUN_00409bfb` | Read by `?HUDDrawAlt@@YIXXZ`; F24.8 format. **High** |
| `0x65` | 1 | `net_state` | `NET_SlaveInit`, `handle_slave_connection_failed`, `slave_process_pkt` | Network connection state byte. **High** |
| `0x68` | 4 | `unk_68` | `FUN_00406a5e`, `FUN_00409bfb`, `FUN_0040e3a0` | 123Ã— accesses. **Low** |
| `0x6A` | 2 | `damage_init_data` | `_DAMAGEInit@0`, `FUN_00444ca0`, `FUN_0044aa20` | Read during damage system init. **High** |
| `0x6B` | 1 | `max_speed_ref` | `FUN_0040e470`, `@ArmPlane@4`, `_MaxSpeed@8` | Read by `_MaxSpeed@8`. **High** |
| `0x6C` | 4 | `unk_6C` | `FUN_00406a5e`, `FUN_00409bfb`, `FUN_0040b320` | 99Ã— accesses. **Low** |
| `0x6E` | 2 | `fm_plane_fields` | `_ScreenDump@0`, `_FMUpdatePlaneFields@0` | Written by `_FMUpdatePlaneFields@0`. **High** |
| `0x70` | 4 | `unk_70` | `slave_process_pkt`, `FUN_00402640`, `FUN_00406a5e` | 121Ã— accesses. **Low** |
| `0x74` | 4 | `unk_74` | `slave_process_pkt`, `FUN_00402640`, `FUN_00406a5e` | 219Ã— accesses. **Low** |
| `0x78` | 4 | `hud_message_ptr` | `slave_process_pkt`, `_HUDMessage@4`, `FUN_00406a5e` | Read by `_HUDMessage@4`. **High** |
| `0x7C` | 4 | `unk_7C` | `FlyingLoop`, `FUN_00406a5e`, `FUN_00409bfb` | 88Ã— accesses. **Low** |
| `0x7D` | 1 | `call_damage_proc` | `@CallDamageProc@4`, `FUN_00463f30`, `GToTurn@8` | Read by `@CallDamageProc@4`. **High** |
| `0x7F` | 1 | `map_side` | `FUN_00406a5e`, `_ServicePlayer@0`, `@MAPSetSide@4` | Written by `@MAPSetSide@4`. **High** |
| `0x80` | 4 | `unk_80` | `FUN_00401580`, `FUN_00402640`, `usnfmain` | 618Ã— accesses. **Low** |
| `0x84` | 4 | `unk_84` | `FUN_00406a5e`, `_DAMAGEInit@0`, `_ServicePlayer@0` | 75Ã— accesses. **Low** |
| `0x86` | 2 | `damage_init2` | `_DAMAGEInit@0`, `FUN_0041769c`, `FUN_0043db40` | Written during damage init. **High** |
| `0x88` | 4 | `unk_88` | `FUN_00406a5e`, `_DAMAGEInit@0`, `@ArmPlane@4` | 66Ã— accesses. **Low** |
| `0x8C` | 4 | `move_obj_ptr` | `FUN_0042d050`, `FUN_00430a90`, `_MoveObj@0` | Read by `_MoveObj@0`; likely next-object link or move data. **High** |
| `0x8D` | 1 | `crc_data` | `FUN_00428340`, `ComputeCRC@@YGJPAUOBJ_TYPE@@` | Read by `?ComputeCRC@@YGJPAUOBJ_TYPE@@@Z`. **High** |
| `0x8E` | 1 | `graphic_flags` | `FUN_0043d69b`, `_GRAPHICAddYourObjs@4` | Read by graphic object add functions. **High** |
| `0x90` | 4 | `unk_90` | `ShellSetup`, `FUN_0042d050`, `_MoveObj@0` | 32Ã— accesses. **Low** |
| `0x94` | 4 | `unk_94` | `FUN_00418a50`, `@GetNames@4`, `FUN_0042d050` | 49Ã— accesses. **Low** |
| `0x96` | 2 | `altitude_display` | `HUDDrawAlt`, `FUN_0043faf0`, `FUN_004449f0` | Read by `?HUDDrawAlt@@YIXXZ`. **High** |
| `0x98` | 4 | `unk_98` | `FUN_0041769c`, `FUN_0042d050`, `_MoveObj@0` | 38Ã— accesses. **Low** |
| `0x9C` | 4 | `unk_9C` | `FUN_0040d810`, `FUN_0042d050`, `FUN_0042e1d0` | 41Ã— accesses. **Low** |
| `0x9E` | 2 | `eject_data` | `@PilotScreen@4`, `@EJECTAdd@4` | Written by `@EJECTAdd@4`. **High** |
| `0xA0` | 4 | `hud_draw_data` | `_HUDDraw@4`, `FUN_00406a5e`, `_DAMAGEInit@0` | Read by `_HUDDraw@4`. **High** |
| `0xA4` | 4 | `unk_A4` | `FUN_0040d390`, `FUN_00425358`, `FUN_0042d050` | 93Ã— accesses. **Low** |
| `0xA6` | 2 | `unk_A6` | `FUN_00406a5e`, `FUN_0040903b`, `FUN_00409f30` | 93Ã— accesses. **Low** |
| `0xA8` | 4 | `unk_A8` | `FUN_00420bb0`, `FUN_0042d050`, `_MoveObj@0` | 90Ã— accesses. **Low** |
| `0xAA` | 2 | `damage_hit_data` | `_DAMAGEInit@0`, `_DAMAGEDoHit@12`, `FUN_00417530` | Written by `_DAMAGEDoHit@12`. **High** |
| `0xAC` | 4 | `unk_AC` | `_HUDInit@0`, `FUN_0040d810`, `FUN_0040e3a0` | 41Ã— accesses. **Low** |
| `0xAD` | 1 | `plane_type_change` | `_DAMAGEInit@0`, `_ChangePlaneType@12`, `_FMGetAcc@12` | Read by `_ChangePlaneType@12`. **High** |
| `0xAE` | 2 | `unk_AE` | `FUN_0040e470`, `FUN_00428a3b`, `_SelectRepairPlane@16` | 45Ã— accesses. **Low** |
| `0xB0` | 4 | `unk_B0` | `FUN_0040e3a0`, `_DAMAGEInit@0`, `FUN_0042d050` | 51Ã— accesses. **Low** |
| `0xB3` | 1 | `reaction_data` | `@GetNames@4`, `FUN_00426696`, `@Reaction@12` | Read by `@Reaction@12`. **High** |
| `0xB4` | 4 | `unk_B4` | `FUN_00406a5e`, `HUDDrawRangeInfo`, `FUN_0040903b` | 84Ã— accesses. **Low** |
| `0xB6` | 2 | `stability` | `HUDDrawHeading`, `HUDDrawRangeInfo`, `HUDSetStability` | Written by `?HUDSetStability@@YIJXZ`. **High** |
| `0xB8` | 4 | `unk_B8` | `FUN_0041c4f0`, `FUN_0042d050`, `_MoveObj@0` | 39Ã— accesses. **Low** |
| `0xBA` | 2 | `flight_key_state` | `FUN_00402640`, `@FlightKey@4`, `FUN_00418a50` | Read by `@FlightKey@4`. **High** |
| `0xBC` | 4 | `unk_BC` | `FUN_00417150`, `@ArmPlane@4`, `FUN_004242a0` | 44Ã— accesses. **Low** |
| `0xC0` | 4 | `ifm_time` | `usnfmain`, `FlyingLoop`, `IFMSetTime@@YAXD@Z` | Written by `?IFMSetTime@@YAXD@Z`. **High** |
| `0xC8` | 4 | `slave_conn_fail` | `handle_slave_connection_failed`, `FUN_00406a5e` | Read on slave connection failure. **High** |
| `0xCC` | 4 | `msg_chatter_ptr` | `FUN_00409bfb`, `_MSGSendChatter@24`, `FUN_004267e4` | Read by `_MSGSendChatter@24`. **High** |
| `0xD0` | 4 | `unk_D0` | `FUN_00401e30`, `FUN_00415e30`, `_MoveObj@0` | 33Ã— accesses. **Low** |
| `0xD5` | 1 | `msg_state` | `_MSGInit@0`, `_MSGSend@32`, `_MSGReceive@8` | Read/written by message system. **High** |
| `0xD8` | 4 | `unk_D8` | `handle_slave_connection_failed`, `FUN_004225d4` | 20Ã— accesses. **Low** |
| `0xDC` | 4 | `unk_DC` | `FUN_00406a5e`, `FUN_0040e470`, `FUN_0042d050` | 34Ã— accesses. **Low** |
| `0xDE` | 2 | `cpanel_draw` | `FUN_0040d810`, `FUN_0042c9b0`, `_CPDraw@8` | Read by `_CPDraw@8`. **High** |
| `0xE2` | 4 | `cp_skill` | `FUN_004242a0`, `FUN_0043a5c0`, `CPSetSkill@@YAXF@Z` | Written by `?CPSetSkill@@YAXF@Z`. **High** |
| `0xE3` | 4 | `nearest_obj` | `_NearestObj@16`, `FUN_00424de0`, `FUN_0043a5c0` | Read by `_NearestObj@16`. **High** |
| `0xE4` | 4 | `waypoint_ptr` | `FUN_0040d810`, `_MSGSendChatter@24`, `MAPUpdateWPPtrs@8` | Read by `@MAPUpdateWPPtrs@8`. **High** |
| `0xE6` | 2 | `service_player` | `_ServicePlayer@0`, `FUN_00417530`, `_SelectRepairPlane@16` | Read by `_ServicePlayer@0`. **High** |
| `0xE8` | 4 | `msg_chatter2` | `FUN_0040e470`, `_MSGSendChatter@24`, `FUN_004226cb` | **Low** |
| `0xE9` | 1 | `ap_takeoff_type` | `@FlightKey@4`, `@APTakeoffType@8` | Written by `@APTakeoffType@8`. **High** |
| `0xEA` | 2 | `ap_landing_type` | `InitCobra`, `MainLoop`, `@APLandingType@8` | Written by `@APLandingType@8`. **High** |
| `0xEB` | 1 | `ap_type2` | `@ArmPlane@4`, `WRFogLayerUpdate`, `@APTakeoffType@8` | **High** |
| `0xF0` | 4 | `unk_F0` | `FUN_004242a0`, `FUN_00426696`, `FUN_0042d050` | 62Ã— accesses. **Low** |
| `0xF1` | 1 | `proj_fire_ref` | `FUN_004aacfe`, `_PROJFire@16`, `_PROJServiceWeapon@24` | Read by `_PROJFire@16`. **High** |
| `0xF4` | 4 | `unk_F4` | `FUN_0040e470`, `_MSGSendChatter@24`, `FUN_004242a0` | 34Ã— accesses. **Low** |
| `0xF5` | 1 | `bay_state` | `@FMBay@4`, `FUN_004b298e` | Read by `@FMBay@4`; weapon bay open/close. **High** |
| `0xF8` | 4 | `arm_plane_data` | `@ArmPlane@4`, `@GetNames@4`, `FUN_004242a0` | Read by `@ArmPlane@4`. **High** |
| `0xFF` | 1 | `hud_disrupt_flag` | `FUN_00405cd0`, `FUN_00406a5e` | See `HUDSetDisrupt@4` at `0x01` of HUD state. **Low** |
| `0x100` | 4 | `unk_100` | `_explode`, `FUN_00401180`, `FUN_00401280` | 328Ã— accesses; mirrors low-offset pattern. **Low** |
| `0x104` | 4 | `menu_bar_data` | `usnfmain`, `MenuDrawBar@@YIXD@Z`, `FUN_0040cb80` | Read by `?MenuDrawBar@@YIXD@Z`. **High** |
| `0x10C` | 4 | `music_on_ptr` | `_MSGSend@32`, `FUN_0042d050`, `MusicOn@@YIXPADF@Z` | Read by `?MusicOn@@YIXPADF@Z`. **High** |
| `0x10D` | 1 | `streamer_ptr` | `FUN_00409bfb`, `_DrawStreamer@12` | Read by `_DrawStreamer@12`. **High** |
| `0x110` | 4 | `unk_110` | `FUN_0040d810`, `_DAMAGEDoHit@12`, `FUN_0041c4f0` | 28Ã— accesses. **Low** |
| `0x111` | 1 | `ap_park` | `_SelectRepairPlane@16`, `_APClearParks@0`, `_APGetPark@0` | Read by `_APGetPark@0`. **High** |
| `0x113` | 1 | `plane_crash` | `FUN_0043faf0`, `_PLANECrash@4` | Read by `_PLANECrash@4`. **High** |
| `0x115` | 1 | `proj_speed_ref` | `MenuCreateRemaps`, `_PROJSpeed@8` | Read by `_PROJSpeed@8`. **High** |
| `0x118` | 4 | `unk_118` | `MenuCreateRemaps`, `FUN_0040d810`, `FUN_0042d050` | 38Ã— accesses. **Low** |
| `0x11A` | 2 | `menu_state` | `MenuStartUp@@YGXPADJJDJ@Z`, `FUN_0040cb80`, `_CPDraw@8` | Read by menu startup. **High** |
| `0x11C` | 4 | `unk_11C` | `MenuStartUp`, `FUN_0040c990`, `MenuSteelRect` | 29Ã— accesses. **Low** |

---

## 2. `PROJ_TYPE` â€” Projectile / Missile Extension

Overlays the upper region of an entity allocation for missile and projectile objects. Scanned in the range `0x4c0000`â€“`0x4c3000` (the PROJ subsystem code block). Only offsets `0x50`â€“`0x7F` are documented; the lower range is shared with `entity`.

**Scan range:** `0x4c0000`â€“`0x4c3000`, offsets `0x50`â€“`0x7F`

| Offset | Size | Field name (inferred) | Accessor functions | Notes |
|--------|------|-----------------------|--------------------|-------|
| `0x50` | 4 | `proj_target_ptr` | `_PROJFire@16`, `_PROJInFOV@40`, `FUN_004c2b5a` | Read at fire and seeker FOV check. **High** |
| `0x54` | 4 | `unk_54` | (1Ã— only) | **Low** |
| `0x55` | 1 | `proj_hit_flag` | `_PROJHit@8` | Written on impact. **High** |
| `0x57` | 1 | `unk_57` | `FUN_004c0a9d` | **Low** |
| `0x58` | 4 | `proj_fov_data` | `_PROJInFOV@40`, `FUN_004c2b5a` | Read by seeker FOV test. **High** |
| `0x5A` | 2 | `unk_5A` | `FUN_004c0a9d` | **Low** |
| `0x5C` | 4 | `proj_guidance` | `FUN_004c24b0`, `_PROJInFOV@40` | Read by guidance update. **Low** |
| `0x60` | 4 | `proj_guidance2` | `FUN_004c24b0`, `_PROJInFOV@40` | Read by guidance update (5Ã—). **Low** |
| `0x64` | 4 | `proj_speed` | `FUN_004c0a9d`, `_PROJSpeed@8`, `_PROJHit@8`, `_PROJFire@16` | Read by `_PROJSpeed@8`; 25Ã— accesses. **High** |
| `0x67` | 1 | `proj_speed_byte` | `_PROJSpeed@8` | Byte component of speed. **High** |
| `0x68` | 1 | `proj_fov_byte` | `_PROJInFOV@40` | Seeker cone angle. **High** |
| `0x6B` | 1 | `unk_6B` | `_PROJSpeed@8` | **High** (same accessor) |
| `0x6C` | 1 | `proj_fov2` | `_PROJInFOV@40` | Second FOV parameter. **High** |
| `0x6E` | 1 | `unk_6E` | `FUN_004c2b5a` | **Low** |
| `0x70` | 4 | `proj_guidance3` | `FUN_004c24b0`, `_PROJInFOV@40` | **Low** |
| `0x72` | 2 | `unk_72` | (1Ã— only) | **Low** |
| `0x74` | 4 | `proj_fire_data` | `FUN_004c0a9d`, `_PROJInFOV@40` | **Low** |
| `0x7F` | 1 | `proj_fire_sound` | `FUN_004c1c10`, `_PROJFireSound@4` | Written when firing sound is triggered. **High** |

---

## 3. `PT_TYPE` â€” Aircraft Performance Type

Performance type data loaded from `.JT` files on disk (see `architecture.md` BRF type byte `5`). At runtime the struct pointer is obtained from `@T_Load@4` / `@T_GetLeaf@12` and passed to the flight model. Key physics globals (`DAT_0050d3xx` range) in `AnalyzePhysics.txt` map to fields in this struct.

**Disk source:** `.JT` files (BRF struct_type `5`)

**Scan range:** `0x400000`â€“`0x540000`, offsets `0x00`â€“`0xC0`

Offsets `0x00`â€“`0x4F` are shared with the `entity` header (same access patterns); the PT-specific physics fields begin around `0x50`. The table below lists the complete distinct set found in the PT_TYPE scan; offsets that exactly match the `entity` scan are marked as shared.

| Offset | Size | Field name (inferred) | Accessor functions | Notes |
|--------|------|-----------------------|--------------------|-------|
| `0x00`â€“`0x4F` | â€” | *(entity header fields)* | â€” | Shared with `entity`; see Â§1 above |
| `0x50` | 4 | `pt_perf_base` | `FUN_00406a5e`, `FUN_00409760`, `FUN_0040991b` | First PT-specific dword after header. **Low** |
| `0x5A` | 2 | `pt_damage` | `FUN_004089a0`, `_DAMAGEDoHit@12`, `_DAMAGEUpdate@0` | Written by damage system. **High** |
| `0x5C` | 4 | `pt_usnf_data` | `usnfmain`, `FUN_00406a5e`, `FUN_0040991b` | **Low** |
| `0x60` | 4 | `pt_unk_60` | `FUN_0040991b`, `FUN_00409bfb`, `MenuCreateRemaps` | **Low** |
| `0x64` | 4 | `pt_alt_f24` | `slave_process_pkt`, `HUDDrawAlt`, `FUN_0040991b` | Altitude in F24.8. **High** |
| `0x68` | 4 | `pt_unk_68` | `FUN_00406a5e`, `FUN_00409bfb`, `FUN_0040e3a0` | **Low** |
| `0x6B` | 1 | `pt_max_speed` | `FUN_0040e470`, `_MaxSpeed@8` | Read by `_MaxSpeed@8`. **High** |
| `0x6E` | 2 | `pt_fm_fields` | `_FMUpdatePlaneFields@0` | Written by flight model update. **High** |
| `0x70` | 4 | `pt_unk_70` | `slave_process_pkt`, `FUN_00402640` | **Low** |
| `0x74` | 4 | `pt_unk_74` | `slave_process_pkt`, `FUN_00402640` | **Low** |
| `0x78` | 4 | `pt_hud_msg` | `_HUDMessage@4`, `FUN_00406a5e` | **High** |
| `0x7C` | 4 | `pt_unk_7C` | `FlyingLoop`, `FUN_00406a5e` | **Low** |
| `0x80` | 4 | `pt_unk_80` | `FUN_00401580`, `FUN_00402640`, `usnfmain` | 618Ã— accesses. **Low** |
| `0x84` | 4 | `pt_damage_init` | `_DAMAGEInit@0`, `_ServicePlayer@0` | **High** |
| `0x88` | 4 | `pt_arm_plane` | `@ArmPlane@4`, `FUN_004267e4` | Read by `@ArmPlane@4`. **High** |
| `0x8C` | 4 | `pt_move_ptr` | `FUN_0042d050`, `_MoveObj@0` | **High** |
| `0x90` | 4 | `pt_unk_90` | `ShellSetup`, `FUN_0042d050`, `_MoveObj@0` | **Low** |
| `0x94` | 4 | `pt_names` | `@GetNames@4`, `FUN_0042d050` | Read by `@GetNames@4`; aircraft name string ptr. **High** |
| `0x96` | 2 | `pt_alt_display` | `HUDDrawAlt`, `FUN_0043faf0` | **High** |
| `0xA0` | 4 | `pt_hud_draw` | `_HUDDraw@4`, `FUN_0040d810` | **High** |
| `0xA4` | 4 | `pt_unk_A4` | `FUN_0040d390`, `FUN_0042d050` | **Low** |
| `0xA8` | 4 | `pt_unk_A8` | `_MoveObj@0`, `FUN_0043faf0` | **Low** |
| `0xAD` | 1 | `pt_change_type` | `_ChangePlaneType@12`, `_FMGetAcc@12` | Read by `_ChangePlaneType@12`. **High** |
| `0xB0` | 4 | `pt_unk_B0` | `_DAMAGEInit@0`, `FUN_0044a910` | **Low** |
| `0xB3` | 1 | `pt_reaction` | `@Reaction@12`, `@GetNames@4` | **High** |
| `0xB6` | 2 | `pt_stability` | `HUDDrawHeading`, `HUDSetStability` | **High** |
| `0xBC` | 4 | `pt_unk_BC` | `FUN_00417150`, `@ArmPlane@4` | **Low** |
| `0xC0` | 4 | `pt_time` | `usnfmain`, `FlyingLoop`, `IFMSetTime@@YAXD@Z` | Written by `?IFMSetTime@@YAXD@Z`. **High** |

---

## 4. `CN_INFO` â€” Network Configuration

Persisted to and loaded from `EA.CFG` / `NET.DAT` on disk. `CN_ReadConfig` reads `0xDDC` bytes after a 4-byte checksum header; `CN_WriteConfig` writes the same. The struct is version-stamped at `[0x00]` (version byte: `1`, `2`, or `3`). Version 3 is the current live format.

**Disk source:** `EA.CFG` or `NET.DAT`  
**Total size:** `0xDDC` bytes (body only; 4-byte checksum prepended on disk)

| Offset | Size | Field name (inferred) | Accessor functions | Notes |
|--------|------|-----------------------|--------------------|-------|
| `0x00` | 4 | `cn_version` | `CN_ReadConfig`, `NET_SlaveShutdown` | Config version. `3` = current format. **High** |
| `0x01` | 1 | `cn_flags` | `NET_SlaveInit`, `NET_MasterStartGame` | General net flags byte. **High** |
| `0x04` | 4 | `cn_session_id` | `NET_CancelPlayerList`, `netDialogAppIO`, `ip2long` | Session or IP identifier. **High** |
| `0x06` | 2 | `cn_hud_msg_flags` | `_HUDMessage@4`, `HUDReprintMessages` | HUD message display flags. **High** |
| `0x07` | 1 | `cn_master_reject` | `NET_MasterRejectPlayer` | Set when master rejects a player. **High** |
| `0x08` | 4 | `cn_pkt_state` | `player_list_process_pkt`, `slave_process_pkt` | Packet processing state. **High** |
| `0x0A` | 1 | `cn_warning` | `HUDSetWarning@8` | HUD warning flags from net. **High** |
| `0x0B` | 1 | `cn_unk_0B` | `usnfmain`, `HUDSetWarning@8` | **Low** |
| `0x0C` | 4 | `cn_unk_0C` | `NET_SlaveInit`, `NET_RequestPlayerList` | **Low** |
| `0x10` | 4 | `cn_master_ptr` | `NET_MasterInit`, `NET_MasterShutdown` | Master session handle. **High** |
| `0x14` | 4 | `cn_player_list` | `NET_RequestPlayerList`, `NET_MasterRejectPlayer` | Player list pointer or state. **High** |
| `0x20` | 4 | `cn_hud_range` | `HUDDrawRangeInfo` | Range info for HUD display. **High** |
| `0x24` | 4 | `cn_hud_heading` | `HUDDrawHeading` | Heading value for HUD. **High** |
| `0x28` | 4 | `cn_hud_speed` | `HUDDrawSpeed` | Airspeed for HUD. **High** |
| `0x30` | 4 | `cn_master_shutdown` | `NET_MasterShutdown` | Shutdown state flag. **High** |
| `0x34` | 4 | `cn_hud_alt` | `HUDDrawAlt` | Altitude for HUD. **High** |
| `0x38` | 4 | `cn_hud_data` | `HUDDrawWeaponInfo` | Weapon info HUD data. **High** |
| `0x40` | 4 | `cn_hud_draw_data` | `_HUDDraw@4` | HUD draw buffer reference. **High** |
| `0x44` | 4 | `cn_hud_init_ext` | `_HUDInit@0`, `FUN_004089a0` | **Low** |
| `0x52` | 1 | `cn_slave_id` | `NET_SlaveInit` | Slave player index. **High** |
| `0x54` | 4 | `cn_hud_nearest` | `HUDFindNearest@8` | Nearest contact for HUD lock. **High** |
| `0x60` | 4 | `cn_hud_data2` | `HUDDrawRangeInfo`, `FUN_00409bfb` | **Low** |
| `0x64` | 4 | `cn_net_slave_state` | `NET_SlaveInit`, `slave_process_pkt` | Full slave network state. **High** |
| `0x68` | 4 | `cn_flight_key` | `FlightKey@4` | Flight key binding data. **High** |
| `0x7C` | 4 | `cn_flying_loop` | `FlyingLoop` | Game loop counter or tick. **High** |
| `0x80` | 4 | `cn_unk_80` | `usnfmain`, `FlyingLoop` | **Low** |
| `0xA0` | 4 | `cn_hud_disrupt` | `_HUDDraw@4` | HUD disruption state. **High** |
| `0xB4` | 1 | `cn_hud_extra` | `HUDSetStability` | HUD stability display. **High** |
| `0xB6` | 2 | `cn_stability` | `HUDSetStability` | Stability value sent over net. **High** |
| `0xC0` | 4 | `cn_ifm_time` | `IFMSetTime@@YAXD@Z` | IFM timer. **High** |
| `0xC8` | 4 | `cn_damage` | `_DAMAGEDoHit@12` | Damage data for net sync. **High** |
| `0xD8` | 4 | `cn_slave_fail` | `handle_slave_connection_failed` | Failure state for disconnection. **High** |
| `0xDB0` | â€” | `cn_tcp_block` | `_NetSetFactoryTCP` | TCP factory defaults block; set by `_NetSetFactoryTCP__YAXPAUCN_INFO_TCP___Z`. **High** |
| `0x8E4` | 1 | `cn_has_mac_str` | `CN_ReadConfig` | Non-zero if MAC address string present at this offset. **High** |
| `0x8E4` | 8 | `cn_mac_str[8]` | `CN_ReadConfig`, `_atohb` | ASCII MAC address string, converted to binary at `0xDD0`. **High** |
| `0x8EC` | 12 | `cn_ip_str[12]` | `CN_ReadConfig`, `_atohb` | ASCII IP address string, converted to binary at `0xDD4`. **High** |
| `0xDD0` | 8 | `cn_mac_bin[8]` | `CN_ReadConfig` | Binary MAC address bytes (decoded from `0x8E4`). **High** |
| `0xDD4` | 12 | `cn_ip_bin[12]` | `CN_ReadConfig` | Binary IP address bytes (decoded from `0x8EC`). **High** |
| `0xDDA` | 1 | `cn_addr_valid` | `CN_ReadConfig` | Set to `1` when both MAC and IP decoded successfully. **High** |
| `0xDDC` | â€” | *(end of struct)* | `CN_WriteConfig` | `fwrite(param_1, 0xDDC, 1, _File)` confirms total body size. |

---

## 5. `GV_TYPE` â€” Ground Vehicle Extension

Extension struct overlaid on entity allocations for ground vehicle objects. Scanned exclusively in the MP subsystem code block (`0x470000`â€“`0x480000`), which handles multiplayer synchronization of ground vehicles. The MP message functions (`MPMsgSend`, `MPGraphicAdd*`, `MPKey`) drive nearly all accesses.

**Scan range:** `0x470000`â€“`0x480000`, offsets `0x00`â€“`0x80`

| Offset | Size | Field name (inferred) | Accessor functions | Notes |
|--------|------|-----------------------|--------------------|-------|
| `0x00` | 1 | `gv_type_id` | `FUN_0047001c`, `MPMsgSend`, `MPGraphicAddExp` | Object type identifier. 443Ã— accesses. **Low** |
| `0x01` | 1 | `gv_flags` | `MPProjAdd`, `FUN_0047001c`, `MPEjectAdd` | Primary flags. 687Ã— accesses. **Low** |
| `0x04` | 4 | `gv_id` | `MPSetPaused`, `FUN_0047001c`, `MPEjectAdd` | Vehicle index or ID. 610Ã— accesses. **Low** |
| `0x08` | 4 | `gv_pos` | `FUN_0047001c`, `MPEjectAdd`, `FUN_004701ac` | Position data. 563Ã— accesses. **Low** |
| `0x0C` | 4 | `gv_state` | `FUN_0047001c`, `MPPrepareForInterp`, `FUN_0047025c` | Interpolation/movement state. 168Ã— accesses. **Low** |
| `0x10` | 4 | `gv_graphic` | `FUN_0047025c`, `MPGraphicAddCrater`, `MPGraphicAddSpecialDebris` | Graphic data reference. 262Ã— accesses. **Low** |
| `0x14` | 4 | `gv_unk_14` | `FUN_004701ac`, `FUN_0047025c`, `MPMsgSend` | 203Ã— accesses. **Low** |
| `0x18` | 4 | `gv_net_data` | `MPEjectAdd`, `FUN_004701ac`, `FUN_0047025c` | Network sync data. 160Ã— accesses. **Low** |
| `0x1C` | 4 | `gv_unk_1C` | `FUN_0047001c`, `FUN_004701ac`, `MPMsgSend` | 179Ã— accesses. **Low** |
| `0x20` | 4 | `gv_unk_20` | `FUN_0047025c`, `MPMsgSend`, `MPGraphicAddExp` | 149Ã— accesses. **Low** |
| `0x21` | 1 | `gv_hulk_flag` | `MPGraphicAddHulk`, `_GVEventProc`, `FUN_0047759b` | Set on vehicle destruction. **High** |
| `0x22` | 1 | `gv_unk_22` | `MPMsgSend`, `MPLaunchDevice`, `MPGraphicAddFire` | **Low** |
| `0x24` | 4 | `gv_unk_24` | `FUN_00470560`, `MPMsgSend`, `MPGraphicAddSmoke` | 99Ã— accesses. **Low** |
| `0x28` | 4 | `gv_unk_28` | `FUN_00470560`, `MPMsgSend`, `MPGraphicAddExp` | 95Ã— accesses. **Low** |
| `0x2C` | 4 | `gv_proj_add` | `MPProjAdd`, `FUN_0047001c`, `MPManAdd` | Read by `MPProjAdd`; projectile launch data. **High** |
| `0x2D` | 1 | `gv_chatter_msg` | `MPMsgSend`, `MPGraphicAddSmokeAdder`, `MPKillStats` | Kill stats message data. **High** |
| `0x30` | 4 | `gv_unk_30` | `FUN_00470560`, `MPGraphicAddExp`, `MPGraphicAddSmoke` | 73Ã— accesses. **Low** |
| `0x32` | 2 | `gv_event_proc` | `FUN_0047267c`, `_GVEventProc`, `_CheckLandingParms@0` | Read by `_GVEventProc`. **High** |
| `0x34` | 4 | `gv_unk_34` | `FUN_00470560`, `MPGraphicAddSmoke`, `MPGraphicAddDebris` | 60Ã— accesses. **Low** |
| `0x38` | 4 | `gv_unk_38` | `FUN_00470560`, `MPGraphicAddSmoke`, `MPPlayerChoseSide` | 52Ã— accesses. **Low** |
| `0x3C` | 4 | `gv_unk_3C` | `FUN_004701ac`, `FUN_00470560`, `MPGraphicAddFire` | 47Ã— accesses. **Low** |
| `0x3F` | 1 | `gv_fort_btn2` | `MPFortButtonText2`, `FUN_0047759b`, `@COSig@20` | **Low** |
| `0x40` | 4 | `gv_unk_40` | `FUN_004701ac`, `MPGraphicAddFire`, `MPSendAntiCheat` | 70Ã— accesses. **Low** |
| `0x44` | 4 | `gv_unk_44` | `FUN_004701ac`, `FUN_00470560`, `MPGraphicAddFire` | 36Ã— accesses. **Low** |
| `0x48` | 4 | `gv_unk_48` | `FUN_004701ac`, `FUN_00470560`, `MPGraphicAddSmokeAdder` | 24Ã— accesses. **Low** |
| `0x4B` | 1 | `gv_fuel` | `MPSetFuel@@YIXG@Z`, `FUN_0047759b`, `_COBrv@0` | Written by `?MPSetFuel@@YIXG@Z`. **High** |
| `0x4C` | 4 | `gv_hardpoints` | `FUN_004701ac`, `FUN_00470560`, `MPSetHardpoints` | Read by `?MPSetHardpoints@@YIXG@Z`. **High** |
| `0x4D` | 1 | `gv_waypoints` | `MPSetWaypoints@@YIXGPAUWAYPOINT@@@Z` | Written by `?MPSetWaypoints@@YIXGPAUWAYPOINT@@@Z`. **High** |
| `0x50` | 4 | `gv_unk_50` | `MPGraphicAddSmokeAdder`, `FUN_00472130`, `_FMFlight@0` | 16Ã— accesses. **Low** |
| `0x52` | 1 | `gv_cn_print` | `CN_Print@@YAXPAE@Z` | Read by `?CN_Print@@YAXPAE@Z`. **High** |
| `0x54` | 4 | `gv_unk_54` | `MPGraphicAddSmokeAdder`, `FUN_004715bc`, `FUN_0047267c` | **Low** |
| `0x5C` | 4 | `gv_clear_surface` | `CDirDrawSurface::Clear`, `_LibStartUp` | DirectDraw surface clear data. **Low** |
| `0x60` | 4 | `gv_cn_factory` | `CN_SetFactoryDefaults` | Network config factory defaults. **High** |
| `0x64` | 4 | `gv_flight_menu` | `MPKey@@YIGG@Z`, `_FlightMenu` | Read by `_FlightMenu`. **High** |
| `0x78` | 4 | `gv_ddraw_surface` | `FUN_004735d0`, `CDirDrawSurface::Create`, `CDirDrawSurface::SetEntries` | DirectDraw surface handle. **Low** |
| `0x7C` | 4 | `gv_ddraw_surface2` | `MPHUDMessage`, `CDirDrawSurface::Create`, `CDirDrawSurface::SetEntries` | **Low** |
| `0x7F` | 1 | `gv_disconnect` | `MPMsgSend`, `FUN_0047267c`, `MPCheckDisconnect` | Read by `?MPCheckDisconnect@@YGDXZ`. **High** |
| `0x80` | 4 | `gv_mp_key` | `FUN_0047025c`, `FUN_00471bdf`, `MPKey@@YIGG@Z` | Read by `?MPKey@@YIGG@Z`. **High** |

---

## 6. `OT_TYPE` â€” Ordnance Type (Static Object)

Type data loaded from `.OT` files (BRF struct_type `1`). The scan for this struct covers the same broad address range as `entity`; offsets below `0x60` are listed since the scan was bounded there. Offsets `0x00`â€“`0x4F` strongly overlap the `entity` header.

**Disk source:** `.OT` files (BRF struct_type `1`)  
**Scan range:** `0x400000`â€“`0x540000`, offsets `0x00`â€“`0x60`

| Offset | Size | Field name (inferred) | Accessor functions | Notes |
|--------|------|-----------------------|--------------------|-------|
| `0x00`â€“`0x4F` | â€” | *(entity header fields)* | â€” | See Â§1 above |
| `0x50` | 4 | `ot_perf_base` | `FUN_00406a5e`, `FUN_00409760`, `FUN_0040991b` | First OT-specific offset after shared header. **Low** |
| `0x5A` | 2 | `ot_damage` | `FUN_004089a0`, `_DAMAGEDoHit@12`, `_DAMAGEUpdate@0` | Damage hit data. **High** |
| `0x5C` | 4 | `ot_unk_5C` | `usnfmain`, `FUN_00406a5e`, `FUN_0040991b` | **Low** |
| `0x5D` | 1 | `ot_flight_key` | `@FlightKey@4`, `@ArmPlane@4`, `FUN_00422a71` | Read by `@FlightKey@4`. **High** |
| `0x5E` | 2 | `ot_unk_5E` | `FUN_00406a5e`, `@StringIsNumber@4`, `@StringToNumber@4` | **Low** |
| `0x5F` | 1 | `ot_map_draw` | `FUN_0040903b`, `FUN_00409f30`, `MAPDrawGrid` | Read by `?MAPDrawGrid@@YGXXZ`. **High** |
| `0x60` | 4 | `ot_menu_data` | `FUN_0040991b`, `FUN_00409bfb`, `MenuCreateRemaps` | Read during menu remap. **Low** |

---

## 7. `NT_TYPE` â€” Nav Target

Type data loaded from `.NT` files (BRF struct_type `3`). Like `OT_TYPE`, the scan covers `0x00`â€“`0x60`. The accessor pattern through the shared range is identical to `entity`; NT-specific behavior appears at `0x5D`â€“`0x60`.

**Disk source:** `.NT` files (BRF struct_type `3`)  
**Scan range:** `0x400000`â€“`0x540000`, offsets `0x00`â€“`0x60`

| Offset | Size | Field name (inferred) | Accessor functions | Notes |
|--------|------|-----------------------|--------------------|-------|
| `0x00`â€“`0x4F` | â€” | *(entity header fields)* | â€” | See Â§1 above |
| `0x50` | 4 | `nt_nav_base` | `FUN_00406a5e`, `FUN_00409760`, `FUN_0040991b` | **Low** |
| `0x5A` | 2 | `nt_damage` | `FUN_004089a0`, `_DAMAGEDoHit@12`, `_DAMAGEUpdate@0` | **High** |
| `0x5C` | 4 | `nt_unk_5C` | `usnfmain`, `FUN_00406a5e` | **Low** |
| `0x5D` | 1 | `nt_flight_ref` | `@FlightKey@4`, `@ArmPlane@4`, `FUN_00422a71` | **High** |
| `0x5E` | 2 | `nt_unk_5E` | `FUN_00406a5e`, `@StringIsNumber@4`, `@StringToNumber@4` | **Low** |
| `0x5F` | 1 | `nt_map_draw` | `FUN_0040903b`, `FUN_00409f30`, `MAPDrawGrid` | Read by `?MAPDrawGrid@@YGXXZ`. **High** |
| `0x60` | 4 | `nt_menu_data` | `FUN_0040991b`, `FUN_00409bfb`, `MenuCreateRemaps` | 166Ã— accesses. **Low** |

---

## Notes on Methodology

The raw data was produced by `RecoverStructs.java`, a Ghidra headless script that enumerates all instructions in the given VA range and records every memory dereference of the form `[reg + constant]` where `reg` holds a known struct pointer argument. The access count column reflects how often each offset was touched across the entire executable scan â€” higher counts indicate more deeply integrated fields.

**Known limitations:**

- The scan cannot distinguish between two different struct types that happen to use the same pointer argument convention. The `entity`, `OT_TYPE`, `NT_TYPE`, and `PT_TYPE` scans share access patterns in the `0x00`â€“`0x4F` range because many engine functions are generic and accept any object pointer.
- `PROJ_TYPE` and `GV_TYPE` were isolated by restricting the scan to subsystem-specific code blocks, which removes the shared-header noise but may miss cross-subsystem accesses.
- Fields accessed only once or twice are often incidental (e.g., CRT helper functions scanning memory). Treat single-access entries as tentative.
- Size column values are inferred from the next observed offset; actual padding may differ.
