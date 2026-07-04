# FA Multiplayer Networking Internals

FA.EXE's multiplayer networking internals. Companion reference: [formats/DAT.md](formats/DAT.md).

> **Provenance:** Ghidra static analysis of FA.EXE with [FA.SMS](formats/SMS.md) symbols
> applied; source output `AnalyzeNetwork.txt`. Every symbol is recorded in the
> [symbol database](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/network.csv)
> and applied to the Ghidra project (this is the reconstruction-program `network` subsystem,
> #219 — it also owns the serial-cable and modem **link transport**, `SER_`/`MOD_`, per the
> program's ownership split with [input](input.md)). Progress:
> [reconstruction matrix](reconstruction.md). Confidence markers follow
> [spec-authoring.md](../spec-authoring.md): confirmed · inferred · unknown.

![Network: a common transport vtable (UDP/IPX/TCP/SPX/serial/modem) under a master/slave session mesh; MPSend/MPReceive carry the per-frame sync.](diagrams/network.svg)

---

## CN_INFO Struct — EA.CFG / NET.DAT Layout

`CN_INFO` is the central configuration struct passed by pointer to nearly every network and
config function. It is 0xDDC bytes on disk (written verbatim by `CN_WriteConfig`). The version
field at offset 0x00 distinguishes EA.CFG revisions 1, 2, and 3.

The table below is derived from the offset scan in `AnalyzeNetwork.txt`, which records every
function that dereferences a `CN_INFO *` and the field offset used. Field names are inferred
from accessor function names and call context.

### CN_INFO Offset Table

| Offset | Size | Inferred Field | Accessor / Evidence |
|--------|------|----------------|---------------------|
| `0x00` | dword | `version` (1, 2, or 3) | `CN_ReadConfig` — branch on `iVar2 == 3 / 2 / 1` |
| `0x01` | byte | flags byte 1 | `FUN_004024d0`, `?IFMSetTime@@YAXD@Z`, `?MPMissionInit2@@YGXXZ` |
| `0x02` | word | flags word | `FUN_0041c460` |
| `0x04` | dword | `transport_type` or link count | `_explode`, `FUN_00401180` (read), `?NET_CancelPlayerList@@YAXXZ` |
| `0x06` | word | sub-field of transport word | `FUN_004075de` |
| `0x07` | byte | sub-field | `FUN_0040b320` |
| `0x08` | dword | `local_addr` / connection slot | `FUN_00401180`, `FUN_00401430`, `?player_list_process_pkt@@YAXPAUNET_PKT@@PAUsocket_state@@@Z` |
| `0x0C` | dword | `remote_addr` or session name | `FUN_00401280`, `FUN_004014b0`, `FUN_00401540`, `?ZeroFrame@@YGXPAEJJF@Z` |
| `0x10` | dword | mouse/display field | `?MouseLoadPtr@@YGXXZ`, `?MaybeCampaignMenu@@YGXPADJJD@Z` |
| `0x14` | dword | player list context pointer | `?NET_RequestPlayerList@@YADPAUNET_ADDRESS@@PAXP6AX1W4PLAYER_ACTION@@PAD0J@Z1P6AX1W4NET_CONNECTED_STATE@@@Z@Z` |
| `0x18` | dword | HUD/display field | `FUN_0040d4e0` |
| `0x1C` | dword | HUD alt state | `?HUDDrawAlt@@YIXXZ` |
| `0x20` | dword | HUD range / weapons | `?HUDDrawRangeInfo@@YIXXZ`, `FUN_00408c80` |
| `0x24` | dword | HUD heading | `?HUDDrawHeading@@YIXXZ` |
| `0x28` | dword | HUD speed / waypoints | `?HUDDrawSpeed@@YIXXZ`, `FUN_00409f30` |
| `0x2C` | dword | HUD / view | `FUN_00408c8b`, `FUN_0040eba0` |
| `0x30` | dword | net session / map | `?NET_MasterShutdown@@YAXXZ`, `FUN_00422a71` |
| `0x34` | dword | HUD init slot | `_HUDInit@0` |
| `0x38` | dword | weapon panel | `?HUDDrawWeaponInfo@@YIXXZ`, `?ComputeBombPosition@4` |
| `0x40` | dword | HUD draw field | `?HUDHasFlaps@@YIDHXZ`, `FUN_00408c80` |
| `0x44` | dword | screen / view slew | `_VIEWSlew@12` |
| `0x4C` | dword | HUD nearest target | `@HUDFindNearest@8` |
| `0x50` | dword | view / graphics | `FUN_0040f270` |
| `0x52` | byte | NET_SlaveInit flag | `?NET_SlaveInit@@...` (offset 0x52) |
| `0x54` | dword | HUD / font | `@HUDFindNearest@8`, `FUN_0040c990` |
| `0x58` | dword | — | `FUN_00409910`, `FUN_0040e3a0` |
| `0x60` | dword | — | `FUN_0040e450` |
| `0x64` | dword | slave pkt connection | `?slave_process_pkt@@YAXPAUNET_PKT@@PAUsocket_state@@@Z` |
| `0x68` | dword | flight loop slot | `@FlightKey@4` |
| `0x6C` | word | player screen handle | `FUN_004075de` |
| `0x70` | dword | pilot object slot | `FUN_00416050` |
| `0x7C` | dword | flying-loop state | `?FlyingLoop@@YAXXZ` (offset 0x7C) |
| `0x80` | dword | mission / damage | `FUN_004282d0` |
| `0x84` | dword | camera / view parms | `FUN_00424040`, `FUN_004240d0` |
| `0x88` | dword | MP send buffer | `?MPSend@@YGXXZ` |
| `0x8C` | byte | MP slow-comm flag | `?MPSetSlowComm@@YGXXZ` (offset 0x8C implied by +0x148) |
| `0x99` | byte | chat / text | `FUN_00413151` |
| `0xA0` | dword | player-list HUD | `_HUDDraw@4` |
| `0xA8` | dword | MP connect flag | `?MPCheckConnection@@YGXXZ` (offset 0xB0) |
| `0xB4` | dword | message state | `FUN_0040d7f0` |
| `0xB6` | word | HUD stability | `?HUDSetStability@@YIJXZ` |
| `0xC8` | dword | slave connection context | `FUN_00401e30` |
| `0xD0` | dword | MSG init | `_MSGInit@0` |
| `0xD8` | dword | slave disconnect context | `?handle_slave_connection_failed@@YAXPAUsocket_state@@@Z` |
| `0x100` | dword | menu / shell state | `?ShellSetup@@YGXXZ`, `?MenuStartUp@@YGXPADJJDJ@Z` |
| `0x104` | dword | menu item state | `FUN_0040cea0`, `FUN_0040cf40` |
| `0x10C` | dword | net dialog | `?netDialogAppIO@@YAJJPAD@Z` |
| `0x114` | dword | init capability field | `_HUDInit@0`, `?InitVideo@@YAGPAUGlobalData@@@Z` (confirmed entity struct offset) |
| `0x148` | dword | MP slow-comm period | `?MPSetSlowComm@@YGXXZ` |
| `0x154` | dword | MP connect field | `?MPConnect@@YGXXZ` |
| `0x194` | dword | menu / display large | `FUN_0040d390` |
| `0x198` | dword | name list | `@GetNames@4` |
| `0x1B0` | dword | — | `FUN_0043bba0` |
| `0x1D4` | dword | MSGSend buffer | `_MSGSend@32` |
| `0x1E0` | dword | view transition | `FUN_0040d810` |
| `0x1F0` | dword | INFO2 draw | `?INFO2Draw@@YIXXZ` |
| `0xDB0` | dword[16]+ | `CN_INFO_TCP` block | `_NetSetFactoryTCP` call in `CN_ReadConfig` (v1/v2 upgrade path) |
| `0x8E4` | byte | IPX node / address hex string start | `CN_ReadConfig` v1/v2 migration — `atohb(param_1 + 0x8E4, param_1 + 0xDD0, 8)` |
| `0x8EC` | byte | secondary address hex string | `atohb(param_1 + 0x8EC, param_1 + 0xDD4, 0xC)` |
| `0xDD0` | byte[8] | binary network address (node) | decoded from `0x8E4` |
| `0xDD4` | byte[12] | binary network address (full) | decoded from `0x8EC` |
| `0xDDA` | byte | address decode valid flag | set to 1 if both `atohb` calls succeed |
| `0xDDB`–`0xDDC` | byte[2] | padding / end | zeroed in v1/v2 migration |

**Notes:**
- Total struct size: `0xDDC` bytes (3,548 bytes). This matches NET.DAT's 3,552-byte file size
  (4-byte checksum prefix + 0xDDC struct body).
- `CN_ReadConfig` writes a 4-byte checksum before the struct; `CfigChecksum` (`0x47F740`)
  computes it over the raw `CN_INFO` bytes.
- Version 3 is the current format. Versions 1 and 2 trigger an upgrade path via
  `_NetSetFactoryTCP` and binary address migration.

---

## CN_ReadConfig / CN_WriteConfig

### `CN_ReadConfig` — `0x47F7A0`

FA.SMS: `?CN_ReadConfig@@YAXPAUCN_INFO@@PAE@Z`
Parameters: `CN_INFO *info`, `char *path`

```c
void CN_ReadConfig(CN_INFO *info, char *path) {
    FILE *f = _wfopen(path, "rb");
    if (f && fread(&checksum, 4, 1, f) == 1) {
        size_t n = fread(info, 1, 0xDDC, f);
        fclose(f);
        if (CfigChecksum(info) == checksum) {
            if (info->version == 3) {
                if (n == 0xDDC) {
                    /* copy _janesOnlineName over info->name field if non-empty */
                    return;
                }
            } else if (info->version == 1 || info->version == 2) {
                info->version = 3;
                NetSetFactoryTCP(info->tcp_block);
                if (info[0x8E4] != 0) {
                    atohb(&info[0x8E4], &info[0xDD0], 8);
                    if (atohb(&info[0x8EC], &info[0xDD4], 0xC))
                        info[0xDDA] = 1;
                } else {
                    memset(&info[0xDD0], 0, 11);
                }
                /* copy _janesOnlineName if non-empty */
                return;
            }
        }
    }
    CN_SetFactoryDefaults(info);
}
```

Key points:
- Opens the file with `_wfopen` (wide-path wrapper). The path argument is typically `"EA.CFG"`.
- Reads a 4-byte checksum header, then 0xDDC bytes of struct body.
- Validates checksum via `CfigChecksum` (`0x47F740`) before trusting any fields.
- On version mismatch or corrupt data, falls back to `CN_SetFactoryDefaults` (`0x47F6D0`).
- v1/v2 migration: patches the TCP block at `+0xDB0` via `_NetSetFactoryTCP`, then decodes the
  hex IPX addresses stored at `+0x8E4` / `+0x8EC` into binary form at `+0xDD0` / `+0xDD4`.
- `_janesOnlineName` (a global) overwrites the player-name field at the tail of the struct when
  set — this is the Janes.com online service name override.

### `CN_WriteConfig` — `0x47F930`

FA.SMS: `?CN_WriteConfig@@YAXPAUCN_INFO@@PAE@Z`
Parameters: `CN_INFO *info`, `char *path`

Writes checksum then the full 0xDDC-byte struct. No transformation — the struct is written
verbatim.

---

## MPReceive — `0x46C980`

FA.SMS: `?MPReceive@@YGDXZ`
Signature: `char MPReceive(void)` — returns non-zero if any packet was processed.

`MPReceive` is the per-frame inbound packet dispatcher. It is called from the main game loop and
all modal screens that need to stay synchronized during multiplayer.

### What it does

1. **Timeout scan** — iterates all peer slots (`0` to `_numComputers - 1`). For each peer that
   is connected (bit set in `MP_Info->connected_mask` at `+0x158`):
   - If the last packet timestamp (`DAT_00546e30[peer]`) is more than `0x500` ticks old, sends a
     keepalive byte `0x4E` via `FUN_0046c0a0` and flushes.
   - If no packet received for `0x5A00` ticks, calls `_MP_Disconnect` or clears the connected
     bit.

2. **Receive loop** — for each peer, calls `FUN_0046ee00(peer)` to pull a pending packet type
   byte. Dispatches on type byte (0x10–0x51 confirmed, plus default fall-through). Updates the
   last-receive timestamp at `DAT_00546e78[peer]` on success.

3. **Returns** — `uStack_54c` (1 if any packet was handled, 0 if none).

4. **Tail** — calls `(*_MP_Often)()` (the transport layer's per-frame poll).

### Packet type dispatch table (MPReceive switch)

| Type | Payload bytes | Action |
|------|--------------|--------|
| `0x10` | 4 | Sync packet tick timestamp — updates `_packetTicks[peer]` |
| `0x11` | 2 | Frame rate update — writes `DAT_00547328[peer]` |
| `0x12` | 1 | Scenario end time trigger — calls `_SetScenarioEndTime(2)` |
| `0x13` | 3 | Object removal — resolves alias, calls `_RemoveCurObj` |
| `0x14` | 9 | Position update (cases 0x14 and 0x15) — reads `obj+0xA8/AC/B0` (XYZ), `+0xCC/D0` (angles); computes interpolation velocities into `+0xB4/B8/BC/D2/D4/D6` |
| `0x15` | 6 | Partial position update (XYZ only) |
| `0x16` | 0x15 | Full entity state — position + angles + velocity packed |
| `0x17` | 0x2B | Projectile add — calls `_PROJAdd`, writes `+0x74` (owner ID), `+0xE6` (target), fires sound |
| `0x18` | 5 | Ejection — resolves pilot alias, calls `_EJECTAdd` |
| `0x19` | 0x2B | Manual entity add — calls `_MANAdd` |
| `0x1A` | 0x0E + variable | Multi-player message — calls `_MSGMultiGet` or `_SAYMsg` |
| `0x1B` | 0x11 | Crater graphic — calls `_GRAPHICAddCrater` |
| `0x1C` | 0x13 | Explosion graphic — calls `_GRAPHICAddExp` |
| `0x1D` | 0x1C | Smoke graphic — calls `_GRAPHICAddSmoke` |
| `0x1E` | 0x1C | Debris graphic — calls `_GRAPHICAddDebris` |
| `0x1F` | 0x1C | Cluster release debris |
| `0x20` | 0x10 | Special debris — calls `_GRAPHICAddSpecialDebris` |
| `0x21` | 0x11 | Hulk graphic — calls `_GRAPHICAddHulk` |
| `0x22` | 0x0F | Device launch — calls `_GRAPHICAddDevice`, `_PROJRetargetMissilesOnDevice` |
| `0x23` | 0x22 | Fire graphic — calls `_GRAPHICAddFire` |
| `0x24` | 0x2D | Smoke adder graphic — calls `_GRAPHICAddSmokeAdder` |
| `0x25` | 3 | Graphic remove — calls `_GRAPHICRemove` |
| `0x26` | 0x0B | Weapon state update — writes entity flags at `DAT_0050CE81`, heading `DAT_0050CEB4`, speed `DAT_0050CE8E` |
| `0x27` | 9 | Projectile target update — writes `DAT_0050CF5E` flags, calls `_PROJSetTarget` |
| `0x28` | 0x10 | NPC state sync — writes `DAT_0050CF5E`, calls `_NPCSetStateTarget`, updates preferred target and nav fields |
| `0x29` | 5 | Fuel quantity — writes `DAT_0050CF9A` |
| `0x2A` | 0x1E | Full aircraft state — updates HUD state flags `DAT_0050CFEF`, aircraft type name at `DAT_0050D095`, airport pointer |
| `0x2B` | 7 | Fuel + throttle fast path — updates `DAT_0050CEB4` (throttle), `DAT_0050D10A`, fires damage timers from `DAT_0050CFEF` bits |
| `0x2C` | 0x1A | Weapon stats — calls `_WpnStats` |
| `0x2D` | 5 | Kill stats — calls `_KillStats` |
| `0x2E` | 7 | Landing stats — calls `_LandingStats` |
| `0x2F` | 0x79 | HUD message string — calls `_HUDMessage` |
| `0x30` | 0x1B | Player revive — calls `FUN_00472670`, increments `_playerRevives[peer]` |
| `0x31` | 10 | Mission score — calls `_MISSIONAddScore` |
| `0x32` | 0x0D | Packet tick write to `DAT_00546D68` buffer |
| `0x33` | 0x12 | Mission file sync — saves to file or calls `_MISSIONTextProc`, sets `DAT_00546D64 = 1` |
| `0x34` | 0x12 | Campaign save file sync — saves or deletes file, sets `DAT_00546E98 = 1` |
| `0x35` | 5 | Random seed sync — writes `_randomSeed`, sets `DAT_00546DD8 = 1` |
| `0x36` | 9 | Game prefs sync — updates `_gamePrefs`, `_gameMultiPrefs`, calls `_MISSIONPrefsChanged` |
| `0x37` | 5 | Screen transition — sets `_masterNextScreen` if not already in-mission |
| `0x38` | 2 | Human chose side (pre-mission) |
| `0x39` | 9 | Quick mission button press |
| `0x3A` | 0x55 | Quick mission button text |
| `0x3B` | 2 | Human chose side (variant) |
| `0x3C` | 9 | Fort mission button press |
| `0x3D` | 0x55 | Fort mission button text |
| `0x3E` | 9 | Fort mission 2 button press |
| `0x3F` | 0x55 | Fort mission 2 button text |
| `0x40` | 0x12 | Anti-cheat data — copies into `_globalAntiCheat` table |
| `0x41` | 2 | Cheats on flag — writes `_globalCheatsOn[peer]` |
| `0x42` | 5 | Frame rate report — writes `_globalFrameRate[peer]` |
| `0x43` | 2 | Game mode update (`_gameMode`) |
| `0x44` | 0x0E | Single mission filename sync |
| `0x45` | 1 | Ready signal — sets `DAT_00546D58[peer] = 1` |
| `0x46` | 5 | MP status update — writes `_mpStatus[peer]` |
| `0x47` | 5 | MP status-to-draw update — writes `_mpStatusToDraw[peer]` |
| `0x48` | 8 | Object control assignment — calls `_OBJSetControl` |
| `0x49` | 3 | Map control assignment — calls `_MAPMaybeSetControl` |
| `0x4A` | 0x10 | Plane type change — calls `_ChangePlaneType` |
| `0x4B` | 7 | Fuel quantity fast path — writes `DAT_0050D07C` |
| `0x4C` | 0x1C5 | Hardpoints sync — walks hardpoint slots via `_HARDPtrs`, resolves resource names |
| `0x4D` | 0x334 | Waypoints sync — resolves aliases via `_OBJAliasWaypoint`, calls `_MAPSetNewWP` / `_MAPMaybeClearSelWP` |
| `0x4E` | 1 | Keepalive / noop |
| `0x4F` | 2 | Game mode byte only |
| `0x50` | 5 | Scenario end time — writes `_endScenarioSetTime` |
| `0x51` | 5 | Mission succeeded — writes `_missionSucceeded` |

**Position interpolation** (cases 0x14–0x16): After updating the entity's canonical position
fields (`+0xA8/AC/B0`) and angles (`+0xCC/D0`), MPReceive computes per-axis velocity
interpolants: `vel = (new - old) * 0x100 / tick_delta`, written to `+0xB4/B8/BC`. Angular
rates go to `+0xD2/D4/D6`. The tick delta comes from `_packetTicks[peer] - obj[+0xA0]`.
The last two snapshot positions are preserved at `+0xC0/C4/C8` (copied from `+0x11/15/19/1D`).

### Callers of MPReceive

`MPReceive` is polled from 19 call sites. The principal ones are:

| Caller VA | FA.SMS Name | Context |
|-----------|-------------|---------|
| `0x46BE50` | `?MPCheckConnection@@YGXXZ` | Connection health check loop |
| `0x46C28C` | `FUN_0046c28c` | `MPMissionInit1` helper |
| `0x471A90` | `FUN_00471a90` | Wait-for-everyone-status loop |
| `0x404C70` | `?FlyingLoop@@YAXXZ` | **Main flight loop** — called every frame in-mission |
| `0x422A71` | `FUN_00422a71` | Mission map screen (pre-flight lobby) |
| `0x488490` | `@DialogUpdate@4` | Dialog modal pump |
| `0x46BDE0` | `?MPFlushAll@@YIXD@Z` | Flush-all helper |
| `0x4A08A0` | `_ChooseActivity@0` | Activity selection screen |
| `0x4A1DD0` | `@BriefScreen@16` | Briefing screen |
| `0x4A5620` | `@ChooseSidesDialog@4` | Choose sides dialog |
| `0x41FB60` | `_FortMission@4` | Fort mission setup |
| `0x474800` | `_FlightMenu` | In-flight menu |

---

## Transport Layers

### IPX

The IPX transport is referenced by protocol name strings at several addresses
(`0x4FC9C8`, `0x4FFF2B`, `0x500107`, `0x501400`, `0x5036B6`).
Selection via `?NetSetProtocol@@YADPADJ@Z` (`0x4B0610`), which matches the string `"IPX"`.

The master/slave session model uses:

| Function | VA | FA.SMS Name | Role |
|----------|----|-------------|------|
| `NET_MasterInit` | `0x40AE50` | `?NET_MasterInit@@YADPAUCN_INFO@@...` | Host: initialises session, registers player callbacks |
| `NET_MasterStartGame` | `0x40AFA0` | `?NET_MasterStartGame@@YADXZ` | Host: signals all players to start |
| `NET_MasterRejectPlayer` | `0x40AFF0` | `?NET_MasterRejectPlayer@@YAXPAUNET_ADDRESS@@PAD@Z` | Host: kick a player |
| `NET_MasterShutdown` | `0x40B080` | `?NET_MasterShutdown@@YAXXZ` | Host: tear down session |
| `NET_SlaveInit` | `0x4016C0` | `?NET_SlaveInit@@YADPAUCN_INFO@@PAUNET_ADDRESS@@...` | Client: join session at address |
| `NET_SlaveShutdown` | `0x401780` | `?NET_SlaveShutdown@@YAXXZ` | Client: leave session |
| `NET_RequestPlayerList` | `0x4017B0` | `?NET_RequestPlayerList@@YADPAUNET_ADDRESS@@...` | Query lobby for player list |
| `NET_CancelPlayerList` | `0x401850` | `?NET_CancelPlayerList@@YAXXZ` | Cancel pending player list query |
| `NET_Initialize` | `0x4B0830` | `?NET_Initialize@@YAJPAUCN_INFO@@J@Z` | General network init (all transports) |
| `NET_Shutdown` | `0x4B0A10` | `?NET_Shutdown@@YAXXZ` | Tear down active transport |
| `NET_StartQuery` | `0x4B0940` | `?NET_StartQuery@@YADPAUCN_INFO@@PAXP6AX...` | Begin async session query |
| `NET_ShutdownQuery` | `0x4B0A90` | `?NET_ShutdownQuery@@YAXXZ` | Cancel session query |
| `NET_Often` | `0x4B0AC0` | `?NET_Often@@YAXXZ` | Per-frame transport pump (called from MPReceive tail) |

Low-level send helpers used by all transports:

| Function | VA | Role |
|----------|----|------|
| `pkt_send` | `0x45D970` | Send packet to one peer by index |
| `pkt_sock_send` | `0x45D940` | Send packet to one socket |
| `pkt_send_all` | `0x4B1B80` | `?net_send_all@@YAXPAUNET_PKT@@@Z` — broadcast to all peers |
| `pkt_set_header` | `0x45DA10` | Fill NET_PKT header fields |
| `pkt_sock_read` | `0x45DB00` | Read one packet from a socket |

### TCP/IP

`tcpinit` (`0x4ABBF0`, `?tcpinit@@YADPAUNET_ADDRESS_LIST@@@Z`) creates a TCP socket
(`socket(AF_INET, SOCK_STREAM, 0)` via WinSock), binds to the local address, calls
`gethostbyname` to enumerate local interfaces, and returns up to three local IP addresses
in a `NET_ADDRESS_LIST` struct. It is called during the TCP configuration screen.

| Function | VA | FA.SMS Name | Role |
|----------|----|-------------|------|
| `tcpinit` | `0x4ABBF0` | `?tcpinit@@YADPAUNET_ADDRESS_LIST@@@Z` | Socket create + local IP enumeration |
| `tcpinit2` | `0x4ABD40` | `?tcpinit2@@YADPAUNET_PROTOCOL@@PAUCN_INFO@@@Z` | Full TCP init with CN_INFO |
| `tcpconnect` | `0x4ABF00` | `?tcpconnect@@YAIPAUNET_ADDRESS@@@Z` | Connect to remote peer |
| `winsock_load` | `0x4A5990` | `?winsock_load@@YADPAU_winsock_funcs@@PAD@Z` | Load WinSock DLL, populate function table |
| `winsock_cleanup` | `0x4A5CB0` | `?winsock_cleanup@@YAXXZ` | Unload WinSock |
| `socket_close` | `0x4A5FA0` | `?socket_close@@YAXI@Z` | Close one socket |
| `socket_close_all` | `0x4A61A0` | `?socket_close_all@@YAXXZ` | Close all sockets |
| `socket_get_state_ptr` | `0x4A6220` | `?socket_get_state_ptr@@YAPAUsocket_state@@I@Z` | Look up socket_state by index |
| `socket_set_state` | `0x4A62A0` | `?socket_set_state@@YAXPAUsocket_state@@W4SOCK_STATE@@@Z` | Update socket FSM state |
| `net_do_accept_connection` | `0x4B1BF0` | `?net_do_accept_connection@@YADIP6ADIJHPAUsocket_state@@@Z@Z` | Accept incoming TCP connection |
| `NET_GetLocalAddressString` | `0x4B0730` | `?NET_GetLocalAddressString@@YAXPAD@Z` | Return IP string |
| `NET_Addr2string` | `0x4B1380` | `?NET_Addr2string@@YADPAUNET_ADDRESS@@PAD@Z` | Format NET_ADDRESS to string |

The socket FSM types are `SOCK_STATE` (enum) and `SOCK_TYPE` (enum). State-function dispatch is
registered per-socket via `socket_add_state_func` (`0x4A6260`) and
`socket_set_all_players_state_funcs` (`0x4A62D0`).

The TCP configuration screen is `?RunNetConfigurationScreen@@YGXJ@Z` (`0x405DF0`), which
renders a dialog with the string `"NETIPX3"` as background. It calls `MP_Initialize` with
transport type `3` (TCP).

### Serial / Modem

COM port strings (`COM1`–`COM8`) appear at `0x501730`–`0x501762`. Modem init strings appear at
`0x4F543D` onward.

| Function | VA | FA.SMS Name | Role |
|----------|----|-------------|------|
| `SER_Initialize` | `0x4ACB20` | `?SER_Initialize@@YAJPAUCN_INFO@@J@Z` | Full serial init (calls phases 1–5) |
| `SER_Initialize1` | `0x44C570` | `?SER_Initialize1@@YAJPAUCN_INFO@@J@Z` | Phase 1: port selection |
| `SER_Initialize2` | `0x44C5D0` | `?SER_Initialize2@@YAJPAUCN_INFO@@J@Z` | Phase 2: baud rate |
| `SER_Initialize2_5` | `0x44C6E0` | `?SER_Initialize2_5@@YAJPAUCN_INFO@@J@Z` | Phase 2.5: flow control |
| `SER_Initialize3` | `0x44C980` | `?SER_Initialize3@@YAJPAUCN_INFO@@J@Z` | Phase 3: open COM port |
| `SER_Initialize4` | `0x44C990` | `?SER_Initialize4@@YAJPAUCN_INFO@@J@Z` | Phase 4: modem dial / wait |
| `SER_Initialize5` | `0x44CA70` | `?SER_Initialize5@@YAJPAUCN_INFO@@J@Z` | Phase 5: exchange names |
| `SER_Shutdown1` | `0x44CBD0` | `?SER_Shutdown1@@YAXXZ` | Shutdown phase 1 |
| `SER_Shutdown2` | `0x44CC00` | `?SER_Shutdown2@@YAXXZ` | Shutdown phase 2 |
| `SER_Write` | `0x44BDB0` | `?SER_Write@@YAJJPAEJ@Z` | Write bytes to COM port |
| `SER_Read` | `0x44BFC0` | `?SER_Read@@YAJJPAEJ@Z` | Read bytes from COM port |
| `SER_Flush` | `0x44BD40` | `?SER_Flush@@YADJD@Z` | Flush COM send buffer |
| `SER_ExchangeNames` | `0x44C260` | `?SER_ExchangeNames@@YADPAE@Z` | Handshake: exchange player names |
| `SER_ForegroundGetPacket` | `0x44BC00` | `?SER_ForegroundGetPacket@@YADAAUSERIAL_PACKET_WRAPPER@@@Z` | Pull packet from foreground queue |
| `SER_ForegroundPutPacket` | `0x44BCB0` | `?SER_ForegroundPutPacket@@YADAAUSERIAL_PACKET_WRAPPER@@@Z` | Push packet to foreground queue |
| `SER_ReadIncomingPackets` | `0x492690` | `?SER_ReadIncomingPackets@@YAXXZ` | Process all pending incoming serial packets |
| `SER_ProcessIncomingPacket` | `0x492570` | `?SER_ProcessIncomingPacket@@YAXUSERIAL_PACKET@@@Z` | Dispatch one incoming serial packet |
| `serIO` | `0x44CCC0` | `?serIO@@YAXJ@Z` | Serial I/O background thread body |
| `MOD_Initialize` | `0x49AFF0` | `?MOD_Initialize@@YAJPAUCN_INFO@@J@Z` | Modem full init |
| `MOD_FindModemAndInit` | `0x49A850` | `?MOD_FindModemAndInit@@YAJPAUCN_INFO@@J@Z` | Auto-detect modem |
| `MOD_DoConnect` | `0x49AD70` | `?MOD_DoConnect@@YAJPAUCN_INFO@@JPAJ@Z` | Dial and connect |
| `MOD_WaitForCall` | `0x49AC00` | `?MOD_WaitForCall@@YAJXZ` | Answer incoming call |
| `MOD_Shutdown` | `0x49B0D0` | `?MOD_Shutdown@@YAXXZ` | Hang up and release modem |
| `RunSerialConfigurationScreen` | `0x49B1D0` | `?RunSerialConfigurationScreen@@YGXXZ` | Serial setup dialog |
| `RunModemConfigurationScreen` | `0x49C780` | `?RunModemConfigurationScreen@@YGXXZ` | Modem setup dialog |

Serial packets are wrapped in `SERIAL_PACKET_WRAPPER` structs queued in `SERIAL_QUEUE`
ring buffers. Reliability is handled by sequence numbers, retransmit requests
(`SER_SendRequests`, `0x4AC2E0`), and CRC verification (`verifyPacketCRC`, `0x49A040`).

---

## Packet Encode / Decode

No dedicated `packetinit` / `packencode` / `packdecode` symbols are present in FA.SMS for
this range. Packet framing is inline in the transport send/receive paths. The key structures
confirmed from decompilation:

- **`NET_PKT`** — generic network packet; header set by `pkt_set_header` (`0x45DA10`),
  read by `pkt_sock_read` (`0x45DB00`).
- **`SERIAL_PACKET`** / **`SERIAL_PACKET_WRAPPER`** — serial framing layer.
  `setPacketInfo` (`0x499F70`) fills header fields. `assignPacketCRC` (`0x49A070`) computes
  and writes the CRC. `verifyPacketCRC` (`0x49A040`) validates on receive.
- **`PKT_PLAYER_AD`** — player advertisement packet; name is obfuscated by `net_mung_name`
  (`0x4B2120`) and decoded by `net_unmung_name` (`0x4B2180`).

Packet queue helpers: `insertQueue` (`0x49A400`), `retrieveFromQueue` (`0x49A4A0`),
`updateQueueHead` (`0x49A3E0`), `fetchFromQueueTail` (`0x49A4D0`).

---

## Session Management

FA uses a master/slave model with no dedicated `netsession` / `sessionjoin` / `sessionhost`
symbols. The equivalent functions are:

| Function | VA | Role |
|----------|----|------|
| `NET_MasterInit` | `0x40AE50` | Create session (host) |
| `NET_SlaveInit` | `0x4016C0` | Join session (client) |
| `NET_MasterStartGame` | `0x40AFA0` | Advance all players to in-mission |
| `NET_RequestPlayerList` | `0x4017B0` | Query lobby for current player list |
| `MP_Initialize` | `0x494BC0` | `?MP_Initialize@@YAJPAUCN_INFO@@J@Z` — high-level MP init (calls NET_Initialize + transport setup) |
| `MP_Shutdown` | `0x494CC0` | `?MP_Shutdown@@YAXXZ` — high-level teardown |
| `MP_SetTransmitTimeout` | `0x494D40` | `?MP_SetTransmitTimeout@@YAXJ@Z` — set send retry timeout |
| `MPMissionInit1` | `0x46C280` | `?MPMissionInit1@@YGXXZ` — sync players before mission start |
| `MPMissionInit2` | `0x46C470` | `?MPMissionInit2@@YGXXZ` — post-start sync |
| `MPCheckConnection` | `0x46BE50` | `?MPCheckConnection@@YGXXZ` — per-frame connection health |
| `MPConnect` | `0x46C190` | `?MPConnect@@YGXXZ` — initiate connection sequence |
| `MPTimeSync` | `0x46C260` | `?MPTimeSync@@YGXXZ` — time synchronization handshake |
| `net_start_game` | `0x4B2530` | `?net_start_game@@YAXXZ` — signal start from host |
| `fill_in_mpinfo` | `0x4B1590` | `?fill_in_mpinfo@@YAXXZ` — populate MP_Info struct |
| `dlg_list_init` | `0x464660` | `?dlg_list_init@@YAPAXG@Z` — player list dialog init |
| `dlg_list_add` | `0x4648B0` | `?dlg_list_add@@YAXPAXW4PLAYER_ACTION@@PADPAUNET_ADDRESS@@J@Z` — add player to lobby list |
| `dlg_list_get_selection` | `0x464800` | `?dlg_list_get_selection@@YA?AW4DLG_SELECTION@@PAXPAUNET_ADDRESS@@PAD@Z` — get selected player |

The `PLAYER_ACTION` and `NET_CONNECTED_STATE` enums are used in callback signatures
throughout the session management layer.

---

## MP Entity Sync

These functions broadcast entity state changes to all peers. They all guard on
`_numComputers != 1` (no-op in single-player) and call `FUN_0046c0a0` to enqueue the
outbound packet, then `MPFlushAll` to send.

| Function | VA | FA.SMS Name | Packet type | What is synced |
|----------|----|-------------|-------------|----------------|
| `MPSetFuel` | `0x4723A0` | `?MPSetFuel@@YIXG@Z` | `0x4B` | Current fuel quantity (`DAT_0050D07C`) for object alias |
| `MPSetHardpoints` | `0x472400` | `?MPSetHardpoints@@YIXG@Z` | `0x4C` | All hardpoint slots — resource name (13 bytes each) + load data (0x11 bytes each); up to `DAT_0050D31D` slots |
| `MPSetWaypoints` | `0x472520` | `?MPSetWaypoints@@YIXGPAUWAYPOINT@@@Z` | `0x4D` | Full waypoint chain (up to 0x334 bytes) with alias substitution via `_OBJAliasWaypoint` |
| `MPChangePlaneType` | `0x472330` | `?MPChangePlaneType@@YGXGPAD@Z` | `0x4A` | Aircraft type change — object alias + `.PT` resource name |
| `MPProjAdd` | `0x470010` | `?MPProjAdd@@YGXXZ` | — | Fires `0x17` packet for current projectile |
| `MPEjectAdd` | `0x470150` | `?MPEjectAdd@@YIXGG@Z` | — | Fires `0x18` packet for ejection event |
| `MPManAdd` | `0x4701A0` | `?MPManAdd@@YGXGPADEPAUF24_POINT3@@1J@Z` | — | Fires `0x19` packet for manual entity |
| `MPSetControl` | `0x472260` | `?MPSetControl@@YGXJGD@Z` | `0x48` | Object control assignment |
| `MPRequestControl` | `0x4722D0` | `?MPRequestControl@@YIXG@Z` | — | Request control of object |
| `MPGraphicAddCrater` | `0x4708B0` | `?MPGraphicAddCrater@@YIXPAUF24_POINT3@@J@Z` | `0x1B` | Crater position + type |
| `MPGraphicAddExp` | `0x470900` | `?MPGraphicAddExp@@YGXJPAUF24_POINT3@@GDDD@Z` | `0x1C` | Explosion position + params |
| `MPGraphicAddSmoke` | `0x470990` | `?MPGraphicAddSmoke@@YGXJPAUF24_POINT3@@JJJG@Z` | `0x1D` | Smoke position + params |
| `MPGraphicAddDebris` | `0x470A20` | `?MPGraphicAddDebris@@YGXJPAUF24_POINT3@@PAUWORD_POINT3@@JD@Z` | `0x1E` | Debris |
| `MPGraphicAddFire` | `0x470C30` | `?MPGraphicAddFire@@YGXEPAUF24_POINT3@@GPAUWORD_POINT3@@JJJ@Z` | `0x23` | Fire |
| `MPGraphicRemove` | `0x470DC0` | `?MPGraphicRemove@@YGXXZ` | `0x25` | Remove graphic for current object |
| `MPSend` | `0x46EED0` | `?MPSend@@YGXXZ` | varies | Outbound packet flush / batch send |
| `MPFlushAll` | `0x46BDE0` | `?MPFlushAll@@YIXD@Z` | — | Flush all queued packets, calls MPReceive |
| `MPService` | `0x46C520` | `?MPService@@YGXXZ` | — | Per-frame MP service (called from main loop) |
| `MPRemoveCurObj` | `0x46FDB0` | `?MPRemoveCurObj@@YGXXZ` | `0x13` | Broadcast current object removal |
| `MPEndMission` | `0x46FE00` | `?MPEndMission@@YGXXZ` | — | Signal mission end to all peers |
| `MPMsgSend` | `0x470640` | `?MPMsgSend@@YIXPAUT_MSG@@@Z` | `0x1A` | Send chat/voice message |
| `MPHUDMessage` | `0x471490` | `?MPHUDMessage@@YIXPAD@Z` | `0x2F` | Broadcast HUD message string |
| `MPSendAntiCheat` | `0x471010` | `?MPSendAntiCheat@@YIXPADJ@Z` | `0x40` | Send anti-cheat hash for named file |
| `MPSendCheatsOn` | `0x471070` | `?MPSendCheatsOn@@YIXD@Z` | `0x41` | Broadcast cheat state |
| `MPSendFrameRate` | `0x4710B0` | `?MPSendFrameRate@@YIXJ@Z` | `0x42` | Broadcast frame rate |
| `MPSendPrefs` | `0x471190` | `?MPSendPrefs@@YGXXZ` | `0x36` | Broadcast game prefs |
| `MPSendMissionSucceeded` | `0x46FEF0` | `?MPSendMissionSucceeded@@YGXXZ` | `0x51` | Broadcast mission success |
| `MPCheckDisconnect` | `0x4733E0` | `?MPCheckDisconnect@@YGDXZ` | — | Check for peer disconnection |

**Position update path** (`0x14`/`0x15`/`0x16`): Position/angle data is sent from entity
fields `+0xA8/AC/B0` (XYZ world coords) and `+0xCC/D0` (pitch/yaw/roll angles).
There is no dedicated `MPSetPos` / `MPSetState` symbol in FA.SMS; position broadcasting is
inline in `FUN_0046c0a0` (the packet enqueue helper) called from the flying loop context.

---

## Dark Zone: `0x482200`–`0x4AACEF`

This range lies between the confirmed MP functions and the terrain/airport system. The
`AnalyzeNetwork.txt` output for this range covers mission stats, fort logic, and text parsing
helpers. Notable functions:

| VA | FA.SMS Name | What it does |
|----|-------------|--------------|
| `0x483C90` | `FUN_00483c90` | Token scanner — skips whitespace (SP/TAB/CR/LF via `FUN_00483D10`) then copies the next non-whitespace token from a global text cursor (`DAT_0055281C`) into a caller buffer. Used by mission text / scripting parser. |
| `0x483D10` | `FUN_00483d10` | `isspace` equivalent — returns 1 for SP, TAB, CR, LF. |
| `0x483D30` | `FUN_00483d30` | Read next token, convert to number via `_StringToNumber`. |
| `0x483D50` | `FUN_00483d50` | Map-key aircraft type remapper — translates aircraft type index 5/6/0xD/0xE/0xF for Turkey/Ukraine/Korea theaters (maps letters T/t/U/u/K/k in `_mapName`). Returns remapped index. |
| `0x484D90` | `_EndOfMissionStats@0` | Computes end-of-mission scoring: player damage percentage (`_dam._2_2_` vs `_startingDamage`), wingman survival, target/protect object tallies (`_stats_targets`, `_stats_targetsDead`, `_stats_protects`, `_stats_protectsAlive`). Sets `DAT_0054DDB8 = 1`. |
| `0x485040` | `_EndOfFortMissionStats@0` | Fort (Fortress) mission variant of end-of-mission stats. |
| `0x4851C0` | `_MISSIONFortDestroyed@4` | Check/update fort destruction state for one fort slot. Tests `param_1 & 0x80` for destroyed flag, iterates fort objects. |
| `0x485260` | `_MISSIONFortDestroyedByFort@4` | Fort-vs-fort destruction: checks if a fort was destroyed by another fort's fire. |
| `0x4852F0` | `_MISSIONFortStatus@4` | Returns current fort status bitmask for a given fort index. |
| `0x485380` | `FUN_00485380` | Fort status aggregation — walks all fort objects and tallies counts. |
| `0x485AE0` | `_ConvertPilotFiles@0` | Pilot file format migration — converts old-format `.PLT` files to current format. |
| `0x485EF0` | `_CheckCD` | CD presence check — verifies disc is inserted before mission. |
| `0x486010` | `_MISSIONLoadCommonResources@0` | Load shared mission resources (called during mission setup). |
| `0x4ABBF0` | `?tcpinit@@YADPAUNET_ADDRESS_LIST@@@Z` | TCP socket init (see TCP/IP section above). |
| `0x4A5990` | `?winsock_load@@YADPAU_winsock_funcs@@PAD@Z` | WinSock DLL loader. |

The text cursor globals used by the token scanner (`DAT_0055281C` = current position,
`DAT_005528C0` = end pointer) suggest this is the mission-text / `.MT` file parser, operating
on a buffer loaded into the memory manager.

---

## Key Globals Referenced

| Address | Symbol | Role |
|---------|--------|------|
| `0x546D58` | — | `ready_flags[peer]` — set to 1 when peer sends `0x45` ready packet |
| `0x546D64` | — | `mission_file_received` flag — set by packet `0x33` |
| `0x546D68` | `DAT_00546D68` | Packet tick receive buffer (13 bytes per peer, written by `0x32`) |
| `0x546DD8` | — | `random_seed_received` flag — set by packet `0x35` |
| `0x546E08` | `DAT_00546E08` | Last-send tick per peer |
| `0x546E30` | `DAT_00546E30` | Last-receive tick per peer (keepalive check) |
| `0x546E78` | `DAT_00546E78` | Last-successful-packet tick per peer (disconnect check) |
| `0x546E98` | — | `campaign_save_received` flag — set by packet `0x34` |
| `0x547328` | `DAT_00547328` | Per-peer frame rate table (written by `0x11`) |
| `0x4F7798` | `?mpStatus@@3PAJA` | MP status array |
| `0x4F77B8` | `?mpStatusToDraw@@3PAJA` | MP status-to-draw array |
| `0x5026B4` | `_doMPStatusDraw` | Flag: whether to draw MP status overlay |
| `0x55281C` | `DAT_0055281C` | Mission text parser cursor pointer |
| `0x5528C0` | `DAT_005528C0` | Mission text parser end pointer |

## Functions

Full record: [`db/symbols/network.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/network.csv).

| VA | Symbol | Role |
|----|--------|------|
| `0x4016C0` | `NET_SlaveInit` | client-side session join |
| `0x401A60` | `NETSlaveConnect` | slave connect helper (proto vtable `open`) |
| `0x4024D0` | `NETProcessPlayerList` | process a `NET_PLAYER_LIST` packet |
| `0x46C980` | `MPReceive` | receive + dispatch multiplayer packets |
| `0x405CD0` | `NETFormatIP` | format an IP address string |
| `0x402330` | `NETArmKeepalive` | arm the socket send/keepalive timer |
| `0x496F40` | `spxinit` | SPX/IPX transport init — enumerate adapters |
| `0x4970C0` | `spxopensocket` | open an SPX socket (`socket(6)` + IPX ioctl) |
| `0x497150` | `spxconnect` | SPX connect to a `NET_ADDRESS` peer |

### IPX/SPX transport

Alongside the IP/UDP path, FA ships an **IPX/SPX** transport (Novell NetWare LANs) built on
a dynamically-resolved winsock function-pointer table (`socket`/`bind`/`listen`/`ioctl`/
`closesocket` at `0x580CAA…`). `spxinit`/`spxinit2` enumerate adapters, `spxopensocket`
opens and configures an SPX socket, `spxlisten`/`spxconnect` accept or dial a peer, and
`convert_addr_ipx2usnf`/`convert_addr_usnf2ipx` translate between an IPX `sockaddr` and the
engine's `NET_ADDRESS`. These leaves were recovered via the
[reproducibility audit](https://github.com/jomkz/fighters-codex/blob/main/db/reproducibility-audit.md) (#241).

## Open Questions

### 1. Dark zone `0x482200`–`0x4AACEF`

The MP-adjacent code in this span (see § Dark Zone) is partly traced; a targeted pass would
resolve the remaining session/sync helpers referenced but not yet named.

**Recharacterized.** The span is not a small set of unnamed session/sync helpers: it holds
~97 still-`FUN_` functions, and most are *not* network — it overlaps the TIME/FPS timing
cluster (`0x4869A0–0x486E60`, see [campaign.md](campaign.md)) and other unmapped FA.EXE code.
The genuinely network-referenced helpers in it are already named; the remainder is a broad
**unclaimed region**, the same class of gap as the in-flight view/replay cluster, and naming it
is a discovery-scale effort rather than a targeted network read — folded into the unmapped-code
discovery [#257](https://github.com/jomkz/fighters-codex/issues/257).

*Status: resolved — re-static (broad unclaimed region; naming folded into #257).*

## Related

- [input.md](input.md) — joystick/mouse input (the sibling of the serial/modem link owned here).
- [campaign.md](campaign.md) — multiplayer missions share the mission-load and scoring path.
- [objects.md](objects.md) — MP entity sync mirrors object state across peers.
- [formats/DAT.md](formats/DAT.md) — the `NET.DAT` / `EA.CFG` config `CN_INFO` is read from.
