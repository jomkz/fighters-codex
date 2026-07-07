---
format: CFG
name: Game Configuration
extensions: [".CFG"]
variants: ["EA.CFG", "IP.CFG"]
category: system
endianness: little
spec:
  status: partial
  gaps:
    - kind: re-static
      issue: 54
      note: "CONFIG fields +0x004/+0x008/+0x0E2 semantics"
codec:
  direction: round-trip
  byte_identical: true
  lib: [lib/src/cfg.cpp]
  commands: [cfg]
  tests: [tests/test_cfg.cpp]
  fuzz: [fuzz/fuzz_cfg.cpp]
  fixtures:
    synthetic: true
    real_manifest: false
related: [P, DAT]
---

# CFG — Game Configuration (.CFG)

Two loose configuration files in the FA install directory — neither packed
into any LIB archive. `EA.CFG` is the binary settings file written by FA on
first run and updated whenever the player changes settings in-game; it
persists graphics, controls, audio, and pilot selection state between
sessions. `IP.CFG` is a small plain-text file read by `IP.EXE` (the
multiplayer session launcher) on startup.

## Tools

### fx

```
fx cfg info <EA.CFG>       # dump the CONFIG struct + round-trip check
```

`cfg_read`/`cfg_write` map every documented field and pass the three
untraced fields (+0x004/+0x008/+0x0E2, gap #54) through verbatim —
byte-identical round-trip, verified against the install's live EA.CFG by an
`FX_FA_ROOT`-gated test. IP.CFG is two lines of plain text and needs no
codec.

## File Layout

All multi-byte integers are little-endian (`IP.CFG` is plain text).

### EA.CFG — binary CONFIG struct (347 bytes)

Fully mapped from `?UCONFIG_save_EA_CFG@@YGDXZ` (0x004b2980) and
`?UCONFIG_load_EA_CFG@@YGPAUCONFIG@@XZ` (0x004b2930) decompiles in
`DumpAllFunctions.txt`. No gameplay diff required.

**Load validation:** `UCONFIG_load_EA_CFG` rejects the file unless magic ==
`0x24` AND file size == `0x15b` (347 bytes). If either check fails it returns
null and the engine falls back to defaults.

**Note:** `CN_ReadConfig` / `CN_WriteConfig` are for **NET.DAT** (3552 bytes,
CN_INFO struct — see [DAT.md](DAT.md)). EA.CFG is a separate, smaller config
handled by the `UCONFIG_*` functions.

| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| `0x000` | 4 | magic | Always `0x24` (36) — version/format tag |
| `0x004` | 4 | `DAT_00520ac8` | **Unknown** — possibly display mode or renderer flag |
| `0x008` | 4 | `DAT_00520a08` | **Unknown** |
| `0x00C` | 4 | `_stickDevice` | Joystick device index (0 = none/keyboard) |
| `0x010` | 4 | `_rudderDevice` | Rudder pedal device index |
| `0x014` | 4 | `_throttleDevice` | Throttle controller device index |
| `0x018` | 4 | `_throttle100__3JA` | Throttle axis 100% calibration value |
| `0x01C` | 192 | `_mainV[0..47]` | 48 × dword joystick axis mapping table (button/axis assignments) |
| `0x0DC` | 6 | `_windowTypes[6]` | Display window mode per view (6 bytes) |
| `0x0E2` | 1 | `DAT_004eb5f0` | **Unknown** |
| `0x0E3` | 1 | `_soundOn` | Sound enabled flag |
| `0x0E4` | 1 | `_stereoSwap` | Stereo channel swap |
| `0x0E5` | 2 | `_overallVol__3GA` | Overall volume |
| `0x0E7` | 2 | `_engineVol__3GA` | Engine sound volume |
| `0x0E9` | 2 | `_lockVol__3GA` | Radar lock / missile tone volume |
| `0x0EB` | 2 | `_rwrVol__3GA` | RWR (radar warning receiver) volume |
| `0x0ED` | 2 | `_stallVol__3GA` | Stall warning tone volume |
| `0x0EF` | 2 | `_radioVol__3GA` | Radio chatter volume |
| `0x0F1` | 2 | `_flightMusicVol__3GA` | In-flight music volume |
| `0x0F3` | 2 | `_otherMusicVol__3GA` | Menu / other music volume |
| `0x0F5` | 2 | `_stereoSeparation__3GA` | Stereo separation width |
| `0x0F7` | 1 | `_dMusic` | MIDI music device index |
| `0x0F8` | 4 | `_gamePrefs` | Gameplay preference flags dword |
| `0x0FC` | 4 | `_gameMultiPrefs` | Multiplayer preference flags dword |
| `0x100` | 4 | `_gameDebugPrefs` | Debug preference flags dword |
| `0x104` | 4 | `_hudBrightness__3JA` | HUD brightness level |
| `0x108` | 33 | `_campaignPilot[33]` | Active campaign pilot name (null-terminated string) |
| `0x129` | 32 | multiplayer callsign | From `DAT_004f8bf9` — confirmed (see globals below) |
| `0x149` | 13 | squadron / wing tag | From `DAT_004f8c19` — confirmed (see globals below) |
| `0x156` | 4 | `_glasses3dAmount__3JA` | 3D glasses convergence amount |
| `0x15A` | 1 | `_adCount__3EA` | AD (advertising/demo?) count byte |

The three string fields (0x108, 0x129, 0x149) are only written if the source
global is non-empty (null-check guards the copy loop). Both load and save
functions are named `UCONFIG_*`, distinct from the network
`CN_ReadConfig`/`CN_WriteConfig` pair which handles NET.DAT.

### IP.CFG — plain-text launcher flags (27 bytes)

One flag per line, CRLF endings. No section headers, no `=`-separated
key/value pairs except for the `/n=` flag.

```
/s
/n="Fighters Anthology"
```

| Flag | Value | Meaning |
|------|-------|---------|
| `/s` | (none) | Start IP.EXE in server/standalone mode (not as a client join) |
| `/n=` | `"Fighters Anthology"` | Session/game name advertised to connecting players in the lobby |

## Engine Notes

Confirmed globals behind the EA.CFG string fields:

| Address | Size | Name | Confirmed in |
|---------|------|------|-------------|
| `DAT_004f8bf9` | 32 bytes | Multiplayer callsign | `_WriteConfig` (0x41e8e0), `FUN_004900f0` (0x4900f0) |
| `DAT_004f8c19` | 13 bytes | Squadron / wing tag | `_WriteConfig` (0x41e8e0), `FUN_004900f0` (0x4900f0) |

`FUN_004900f0` (entity name lookup): when `param_1 == _playerId` and
`DAT_004f8bf9 != '\0'`, uses these globals as the player's display name + tag
pair (passed to `FUN_0048e3f0`); otherwise reads the name from `entity+10`.

IP.EXE notes:

- IP.EXE is an MFC Win32 application (`AfxWinMain` entrypoint at `0x436ef0`).
  It reads `IP.CFG` at launch and applies these as default session parameters.
- The session name `"Fighters Anthology"` is the value shown in the LAN/IPX
  game browser on joining clients.
- `FA.EXE` launches `IP.EXE` as a child process when the player selects a
  multiplayer connection type from the main menu.

## Open Questions

### 1. Unmapped CONFIG fields

Three EA.CFG fields have no traced consumer semantics: the dwords at `+0x004`
(`DAT_00520ac8` — possibly display mode or renderer flag) and `+0x008`
(`DAT_00520a08`), and the byte at `+0x0E2` (`DAT_004eb5f0`).

*Status: open — re-static (#54)*

## Related

**Formats:** [P](P.md) — pilot save files (`PLTnnn.P`) whose active slot is
referenced here; [DAT](DAT.md) — the multiplayer transport configs handled by
the separate `CN_*` function family.
