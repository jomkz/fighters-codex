# Game Configuration (EA.CFG)

`EA.CFG` is a binary configuration file written by FA on first run and updated whenever the player changes settings in-game. It persists graphics, controls, audio, and pilot selection state between sessions.

## Location

Loose file in the FA install directory — not packed into any LIB archive.

## Observed Properties

| Property | Value |
|----------|-------|
| Filename | `EA.CFG` |
| Size     | 347 bytes |
| Format   | Binary (not plain text) |

## Binary Layout (Confirmed — 2026-05-19)

Fully mapped from `?UCONFIG_save_EA_CFG@@YGDXZ` (0x004b2980) and `?UCONFIG_load_EA_CFG@@YGPAUCONFIG@@XZ` (0x004b2930) decompiles in `DumpAllFunctions.txt`. No gameplay diff required.

**Load validation:** `UCONFIG_load_EA_CFG` rejects the file unless magic == `0x24` AND file size == `0x15b` (347 bytes). If either check fails it returns null and the engine falls back to defaults.

**Note:** `CN_ReadConfig` / `CN_WriteConfig` are for **NET.DAT** (3552 bytes, CN_INFO struct). EA.CFG is a separate, smaller config handled by the `UCONFIG_*` functions.

### CONFIG struct (347 bytes)

| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0x000 | 4 | magic | Always `0x24` (36) — version/format tag |
| 0x004 | 4 | `DAT_00520ac8` | Unknown — possibly display mode or renderer flag |
| 0x008 | 4 | `DAT_00520a08` | Unknown |
| 0x00C | 4 | `_stickDevice` | Joystick device index (0 = none/keyboard) |
| 0x010 | 4 | `_rudderDevice` | Rudder pedal device index |
| 0x014 | 4 | `_throttleDevice` | Throttle controller device index |
| 0x018 | 4 | `_throttle100__3JA` | Throttle axis 100% calibration value |
| 0x01C | 192 | `_mainV[0..47]` | 48 × dword joystick axis mapping table (button/axis assignments) |
| 0x0DC | 6 | `_windowTypes[6]` | Display window mode per view (6 bytes) |
| 0x0E2 | 1 | `DAT_004eb5f0` | Unknown |
| 0x0E3 | 1 | `_soundOn` | Sound enabled flag |
| 0x0E4 | 1 | `_stereoSwap` | Stereo channel swap |
| 0x0E5 | 2 | `_overallVol__3GA` | Overall volume |
| 0x0E7 | 2 | `_engineVol__3GA` | Engine sound volume |
| 0x0E9 | 2 | `_lockVol__3GA` | Radar lock / missile tone volume |
| 0x0EB | 2 | `_rwrVol__3GA` | RWR (radar warning receiver) volume |
| 0x0ED | 2 | `_stallVol__3GA` | Stall warning tone volume |
| 0x0EF | 2 | `_radioVol__3GA` | Radio chatter volume |
| 0x0F1 | 2 | `_flightMusicVol__3GA` | In-flight music volume |
| 0x0F3 | 2 | `_otherMusicVol__3GA` | Menu / other music volume |
| 0x0F5 | 2 | `_stereoSeparation__3GA` | Stereo separation width |
| 0x0F7 | 1 | `_dMusic` | MIDI music device index |
| 0x0F8 | 4 | `_gamePrefs` | Gameplay preference flags dword |
| 0x0FC | 4 | `_gameMultiPrefs` | Multiplayer preference flags dword |
| 0x100 | 4 | `_gameDebugPrefs` | Debug preference flags dword |
| 0x104 | 4 | `_hudBrightness__3JA` | HUD brightness level |
| 0x108 | 33 | `_campaignPilot[33]` | Active campaign pilot name (null-terminated string) |
| 0x129 | 32 | second name string | From `DAT_004f8bf9` (possibly joystick calibration filename or NATO variant name) |
| 0x149 | 13 | third name string | From `DAT_004f8c19` |
| 0x156 | 4 | `_glasses3dAmount__3JA` | 3D glasses convergence amount |
| 0x15A | 1 | `_adCount__3EA` | AD (advertising/demo?) count byte |

The three string fields (0x108, 0x129, 0x149) are only written if the source global is non-empty (null-check guards the copy loop). Both load and save functions are named `UCONFIG_*`, distinct from the network `CN_ReadConfig`/`CN_WriteConfig` pair which handles NET.DAT.

## Confirmed Globals

| Address | Size | Name | Confirmed in |
|---------|------|------|-------------|
| `DAT_004f8bf9` | 32 bytes | Multiplayer callsign | `_WriteConfig` (0x41e8e0), `FUN_004900f0` (0x4900f0) |
| `DAT_004f8c19` | 13 bytes | Squadron / wing tag | `_WriteConfig` (0x41e8e0), `FUN_004900f0` (0x4900f0) |

`FUN_004900f0` (entity name lookup): when `param_1 == _playerId` and `DAT_004f8bf9 != '\0'`, uses these globals as the player's display name + tag pair (passed to `FUN_0048e3f0`); otherwise reads the name from `entity+10`.

## Related

- [PLT.md](PLT.md) — pilot save files (`PLT_*.PLT`) whose slot is referenced here

---

# Multiplayer Launcher Config (IP.CFG)

`IP.CFG` is a plain-text file read by `IP.EXE` (the multiplayer session launcher) on startup. It supplies default command-line arguments for the IP networking session.

## Location

Loose file in the FA install directory — not packed into any LIB archive.

## Observed Properties

| Property | Value |
|----------|-------|
| Filename | `IP.CFG` |
| Size     | 27 bytes |
| Format   | Plain text (CRLF line endings) |

## Format

One flag per line. No section headers, no `=`-separated key/value pairs except for the `/n=` flag.

```
/s\r\n
/n="Fighters Anthology"
```

## Known Flags

| Flag | Value | Meaning |
|------|-------|---------|
| `/s` | (none) | Start IP.EXE in server/standalone mode (not as a client join) |
| `/n=` | `"Fighters Anthology"` | Session/game name advertised to connecting players in the lobby |

## Notes

- IP.EXE is an MFC Win32 application (`AfxWinMain` entrypoint at `0x436ef0`). It reads `IP.CFG` at launch and applies these as default session parameters.
- The session name `"Fighters Anthology"` is the value shown in the LAN/IPX game browser on joining clients.
- `FA.EXE` launches `IP.EXE` as a child process when the player selects a multiplayer connection type from the main menu.

## Related

- `IP.EXE` — the multiplayer launcher binary this config file drives
