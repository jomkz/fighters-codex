# Campaign / Mission / Pilot

The single-player meta-game: the theater **mission map** screen, the scripted **ZONE**
threats, **pilot** save/logbook, and the **.CAM** campaign state machine that strings
missions together with scoring. Re-carved from a grab-bag nominal range into its true
clusters (the mission-map editor `0x421C70–0x42B800` plus pilot/campaign/mission cores in
`0x467110–0x490000`).

> **Provenance:** Ghidra static analysis of the game executable with [FA.SMS](formats/SMS.md) symbols applied; recorded in the [symbol database](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/campaign.csv) and applied to the Ghidra project. Progress: [reconstruction matrix](reconstruction.md). Markers follow [spec-authoring.md](../spec-authoring.md): confirmed · inferred · unknown.

## The pieces

- **Mission map** (`MAP*`, `0x421C70–0x42B800`): the theater-map screen — world↔screen
  coordinate transforms (`MAPScreenToWorld`), background/grid/icon/path drawing, waypoint
  editing (`MAPWPListBounds`), and the object placement the mission builder uses.
- **ZONE threats**: scripted threat zones that fire projectiles/explosions on a time
  schedule — `ZONEActive` (active-window test), `ZONEServiceRange`, `ZONEPickTarget`.
- **Pilot** (`PLT*`, `0x467180–0x468FD0`): the pilot save (`PILOT.BKP`), sorted rosters,
  and the logbook / "paper" record builder.
- **Campaign** (`.CAM` state machine, `0x480750–0x4869A0`): `_CallCampaignProc` drives the
  `_campaignState` (active → dead/fail → planning); mission load runs through
  `_CallMissionProc` + `_MISSIONInit2`, and scoring accumulates via `_MISSIONAddScore`.

![Campaign flow: the .CAM state machine drives mission selection; each mission loads, plays, and scores back into the pilot record.](diagrams/campaign.svg)

## Functions

Full record: [`db/symbols/campaign.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/campaign.csv).

| VA | Symbol | Role |
|----|--------|------|
| `0x4224B3` | `MAPScreenToWorld` | inverse map projection (screen → world) |
| `0x42256A` | `MAPLoadBG` | load the theater-map background |
| `0x421D40` | `ZONEActive` | scripted-threat active-window test |
| `0x422120` | `ZONEPickTarget` | pick a target plane for a zone |
| `0x422190` | `MAPWPListBounds` | walk a waypoint list |

## Mission runtime — the `.M` interpreter (#485)

`_MISSIONTextProc@16` (`0x481C10`, 8.3 KB — the largest single function this subsystem
owns) is the consumer of the [`.M`](formats/M.md) / [`.MT`](formats/MT.md) format specs:
the interpreter that **executes** a mission file to populate the live mission. Until it was
traced, those specs described a file nobody had watched run.

**Tokenizer.** The file is plain whitespace-delimited text walked by a global cursor
(`0x55281C` current, `0x5528C0` end). `TextNextToken` (`0x483C90`) skips delimiters
(`TextIsDelim`) then copies one token; `TextNextNumber` (`0x483D30`) reads a token and runs
it through `_StringToNumber`; `TextTokenToValue` (`0x483D50`) applies a theater-specific
object-type remap keyed on the first character of `_mapName` (`T/U/K…`). `_MISSIONTextProc`
calls `TextNextNumber` **106 times** — the format is overwhelmingly numeric (coordinates,
type ids, counts).

**Dispatch → live objects.** A jump-table `switch` on each section keyword drives
construction. Header directives set the globals `_layerName`, `_missionDLLName` (the `.MC`
DLL), `_mapName` (theater; `$`/`~` prefixes are special-cased), and `_missionHours`
(fed to `_TIMEInit`). Each object placement calls `_T_AddObj`, then — by flags — `_WNGAdd`
(attach to a wing), `_GRPAdd` (a group), `_HARDUnload`/`_HARDLoad` (apply a loadout), and
`MAPAddSpecial` (a map marker). Named objects are resolved with `_OBJAlias` and given
waypoint lists via `_WPSetWaypoints`. This is the mechanical proof of the `.M` grammar:
"object placement and waypoints" is `T_AddObj` + `WPSetWaypoints`, exactly.

**Mission-logic handoff.** `_CallMissionProc@8` (`0x481940`) dispatches into the mission's
compiled [`.MC`](formats/MC.md) DLL proc — the per-mission condition/event logic —
which `_MISSIONCheckSuccess@0` (`0x486860`) polls each tick.

**Construction variants.** `__SingleMission@0` (one-off missions), `__CreateQuickMission@4`,
and `__CreateFortMission@4`/`2` set up the non-campaign game modes; `_FortMission@4`
(`0x41FB60`) / `_FortMission2@4` build the base-assault ("Fort") scenarios, whose
`HARD*Fort*` helpers save/restore/rearm the defending loadouts.

**Scoring & end-of-mission.** `_MISSIONAddScore@12` (`0x486580`) accumulates the score;
`_EndOfMissionStats@0` (`0x484D90`) and `_EndOfFortMissionStats@0` tally kills/losses at
mission end; the Fort scenarios track objectives through `_MISSIONFortDestroyed@4`,
`_MISSIONFortDestroyedByFort@4`, `_MISSIONFortStatus@4`, and the `_MISSIONFortWin` test.
(The `Score*` functions in the sound range are the **music** score, not this.)

## Open Questions

### 1. TIME/FPS utility block — resolved

`0x4869A0–0x486E60` is a self-contained **game-clock + FPS + timer-interrupt** cluster, not
campaign code. Its functions are almost all FA.SMS-named: `TIMESystemTime` (`0x4869A0`),
`TIMEInit`/`TIMERestart`/`TIMEUpdate`, `TIMESetCompression` (`0x486C60`), `FPSInit`/`FPSUpdate`/
`FPSPrint`/`FPSPrint2`/`FPSReturn`, and `InstallTimerInt` (`0x486E20`), plus two helpers —
`FUN_00486BF0` (a `QueryPerformanceCounter`-based high-resolution frame-time via `__alldiv`) and
`FUN_00486DC0` (a `strlen` used by the FPS string formatting). It drives the very globals in
[game-loop.md](game-loop.md) § Frame Timing (`_timerTicks`, `_currentTime`, `_timeCompression`).
**Disposition:** it is its own small timing subsystem — recommend a standalone `timing` row in
`db/subsystems.csv` (≈13 functions, 2 still `FUN_`) rather than annexing it to campaign; tracked
alongside the game-loop discovery [#257](https://github.com/jomkz/fighters-codex/issues/257).

*Status: resolved — re-static (identified as the TIME/FPS timing cluster; homing tracked in #257).*

## Related

- [formats/CAM.md](formats/CAM.md) / [formats/P.md](formats/P.md) — the campaign and pilot save formats.
- [objects.md](objects.md) — mission objects are entities; scoring reads the mirror.
- [network.md](network.md) — multiplayer missions share the mission-load path.
