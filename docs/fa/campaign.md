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

## The pilot record, engine-side (#492)

The [`.P`](formats/P.md) spec maps the file from the byte side; the #492 read recovered
how the engine populates it. `_campaignPilot` — the live in-memory record — sits at
`0x4F8BB8`, which resolves every "campaign state" global into a **pilot-record field**:
confirmed

| offset | field | writer |
|---|---|---|
| `+0x00` | format version byte `0x0F` | `PilotFormatRank` (new pilot); `PilotScreen` validates it + the 0x25E0 size |
| `+0x01` / `+0x41` | name / callsign | `EditPilot` (`MISSIONEnsureLegalName`d) |
| `+0x61` | personal insignia | `CallsignChoose`: `^<CALLSIGN>.5K` when that [.5K](formats/11K.md) exists |
| `+0x95` | photo PIC name | the photo picker (`MakePicList("LEFT"…)` pairs LEFT/RIGHT facings) |
| `+0xA2` | rank string | from `_pilotRanks` |
| `+0xC2` | campaigns-won list | appended (", "-joined) by `CallCampaignProc` cmd 5 on victory |
| `+0x5AF` | service-record lines | the dossier's free-text block (≤10 lines) |
| `+0xD7F` | active campaign file | `InitCampaignPilot` |
| `+0xDAC` | status word | 0 available · 1 on campaign ("%s, mission %d") · 2 MIA · 3 KIA · 4 retired |
| `+0xDB0` | plane roster | 0xBC-byte per-plane slots: damage array copy at `+0x30`, **repair %** at `+0x2E` (`dam×100/max + type+0x1B4`, clamp 100); a bail zeroes the slot |
| `+0x1C60` | campaign stores pool | 50 × 16-byte `{type, count@+0xE}`; `LoadCampaignStores` returns/deducts vs the plane hardpoints, −1 = unlimited |
| `+0x1F80` | stats counters | missions flown, failures, wingman missions/losses, bails (`CallCampaignProc` cmd 3) — the block `fx plt dump` reads |

Saves go to `PLT%03d.P` (`PilotFindFreeSlot` probes from a random slot; that is why the
filename numbers look random). `PILOT.BKP` is the **retry snapshot**: written before each
campaign mission (cmd 1), restored on "try this mission again" after a death/failure.
`ConvertPilotFiles` migrates 0x15B4-byte legacy (pre-FA) records field-by-field into the
0x25E0 layout — and renames any other wrong-size `.P` file to `.POO`. confirmed

**Pilot screens.** `PilotScreen` (`0x468020`) serves three modes off one body — SHWPILOT
(select), CONTPLT (continue campaign), VIEWPLT (view roster) — with name-sorted
available/unavailable rosters, photo + nose-art + tail-art PIC lists, and the dossier
"paper" (`PilotBuildPaper`: `SHWPILOT.TXT` template lines interleaved with record fields,
then the `AddStats` block, rendered by the shell's
[format engine](shell-ui.md#the-textformat-engine-492)). confirmed

## The campaign driver (#492)

`_CallCampaignProc(cmd)` (`0x481440`) is the state machine the roadmap doc used to name
without describing. The campaign logic itself is a loaded resource proc (`LoadCampaignProc`
→ `_campaignProc`, invoked via `CampaignProcInvoke` with the campaign's state byte in the
high bits); the driver wraps it: confirmed

| cmd | phase | what the driver does around the proc |
|---|---|---|
| 0 | init | invoke(0), clear the backup flag |
| 1 | mission start | write `PILOT.BKP`, invoke, `SeqEnd` (disk-error path → `CampaignDiskError`) |
| 3 | post-mission | bail test (`AlmostHome` / `AtFriendlyAP`) — a bail zeroes the plane's roster slot; a home landing banks damage → **repair %** and returns stores to the pool; bumps the missions/failures/bails/wingman counters |
| 4 | debrief | dead → "You have died / MIA — try again?" (restore `PILOT.BKP`, state 4) else failed → same offer; declining saves + `CampaignOff`, state `0x11` |
| 5 | campaign end | victory appends the campaign to the pilot's campaigns-won field, save + off |
| 6/8 | query | invoke and return the proc's result |
| 7 | bail notify | invoke only |

`CampaignMenu` exposes 1 = replay (restore `PILOT.BKP`) and 2 = `AbortCampaign` from the
menu bar. The quit-mission guard (`ConfirmQuitMission`, shared body of the per-theater
`KurileQuit`/`VietnamQuit` thunks) evaluates mission success via `CallMissionProc` and
warns "If you quit before reaching home…" / "You have not yet fulfilled the mission…".
confirmed

**Briefing flow.** The shell sequences screens through small trampolines with skip flags
(`_doScreens`, `_doBriefPaper`, `_doBriefMap`, `_doSelectPlane` — a windowed test harness
left in the shipping binary): `_StartCampaign` = `CampaignSelect` (the `.CAM` chooser,
descriptions from `<name>.TXT` sections) + `PilotScreen`; `_BriefPaper` shows
`BriefScreen` on the mission's [`.MT`](formats/MT.md) text (one of four random
BRIEFSCR/DEBSCR backgrounds; multiplayer ready-status sync; Jane's Online stats hooks);
`_BriefMap` is `MAPScreen`; `_SelectPlane`/`_RepairPlane` run `SelectRepairPlane`;
`_CreateProMission` opens the map editor; `_AircraftReference` and `_ViewPilots` jump to
the reference and roster screens. Cancelling a campaign brief runs `AbortCampaign`.
confirmed

![Campaign flow: the .CAM state machine drives mission selection; each mission loads, plays, and scores back into the pilot record.](diagrams/campaign.svg)

## The theater map screen & waypoint editor

The mission-planning map and its waypoint list, both fully named by FA.SMS. The **map
screen** projects the world onto the theater bitmap (`MAPWorldToScreen`, inverse of
`MAPScreenToWorld`), draws the grid/background/special markers, and edits object placement
and side (`MAPObjAlts`/`MAPSetSide`/`MAPMaybeSetControl`, `MAPOnSpecial` hit-testing a
`WORD_POINT`). The **waypoint manager** (`WP*`) owns the current route: set/optimize the
list, advance through it, and query the active waypoint's position/object/target.
`ZONEInit`/`ZONEUpdate` bracket the scripted-threat zones (see `ZONEActive` above).

| VA | Symbol | Role |
|----|--------|------|
| `0x422380` | `MAPWorldToScreen` | project a `F24_POINT3` to a map `WORD_POINT` |
| `0x4223BE` | `MAPDrawGrid` | draw the map lat/long grid |
| `0x4224EE` | `MAPDrawBG` | draw the theater background |
| `0x422851` | `MAPDrawSpecials` | draw special markers (bases, threats) |
| `0x422A0D` | `MAPOnSpecial` | hit-test a click against the specials |
| `0x4221D0` | `MAPObjAlts` | raise placed objects to terrain altitude |
| `0x422300` | `MAPSetSide` | set the selected object's side |
| `0x422320` | `MAPMaybeSetControl` | toggle player control of the selection |
| `0x42267F` | `MAPUpdateWPPtrs` | refresh the waypoint back-pointers |
| `0x4226F0` | `MAPSetNewWP` | drop a new waypoint |
| `0x4227AD` | `MAPMaybeClearSelWP` | clear the selected waypoint |
| `0x499380` | `WPSetWaypoints` | install a waypoint list |
| `0x4993C0` | `WPSetupCurrent` | initialise the current-waypoint cursor |
| `0x499680` | `WPMaybeAdvance` | advance to the next waypoint when reached |
| `0x499840` | `WPChange` | change the active waypoint |
| `0x4999B0` | `WPPos` | current waypoint position |
| `0x499A50` | `WPObj` | current waypoint object |
| `0x499AA0` | `WPTarget` | current waypoint target |
| `0x499AD0` | `WPDoingWaypoints` | are waypoints active |
| `0x499AF0` | `WPDirString` | heading-to-waypoint text |
| `0x499C50` | `WPOptimizeWaypoints` | drop redundant waypoints |
| `0x499640` | `WPGoalObjEvent` | goal-object event on a waypoint |
| `0x421C70` | `ZONEInit` | init the scripted-threat zones |
| `0x421DD0` | `ZONEUpdate` | per-frame zone service |

## Functions

Full record: [`db/symbols/campaign.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/campaign.csv).

| VA | Symbol | Role |
|----|--------|------|
| `0x4224B3` | `MAPScreenToWorld` | inverse map projection (screen → world) |
| `0x42256A` | `MAPLoadBG` | load the theater-map background |
| `0x421D40` | `ZONEActive` | scripted-threat active-window test |
| `0x422120` | `ZONEPickTarget` | pick a target plane for a zone |
| `0x422190` | `MAPWPListBounds` | walk a waypoint list |
| `0x481440` | `CallCampaignProc` | the campaign driver (cmd table above) |
| `0x468020` | `PilotScreen` | the pilot roster screen (3 modes) |
| `0x4674F0` | `PilotBuildPaper` | compose + print the pilot dossier |
| `0x480EA0` | `LoadCampaignStores` | sync the campaign stores pool with the plane |
| `0x4A1DD0` | `BriefScreen` | briefing/debrief screen over `.MT` |
| `0x4A2A30` | `AddStats` | compose the marked-up debrief stats text |
| `0x485AE0` | `ConvertPilotFiles` | legacy pilot-file migration (0x15B4 → 0x25E0) |
| `0x480750` | `MISSIONInit1Impl` | mission bring-up (RNG, score reset, subsystem init order) |
| `0x480A30` | `MISSIONInit2Impl` | post-load: sides, humans, aliases, `.MC` proc |
| `0x480230` | `ComputeCRC` | anti-cheat CRC-32 over an `OBJ_TYPE` record |
| `0x4A10E0` | `SingleMission` | single-mission browser + launcher |
| `0x486160` | `MISSIONEndScenario` | multiplayer end-condition test |
| `0x485820` | `KillStats` | tally a kill into its category bucket |

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

## Mission lifecycle — init, anti-cheat, shutdown (#485)

Every mission — campaign, single, quick, or fort — is bracketed by the same bring-up and
tear-down. **`MISSIONInit1`** (`0x480750`) seeds the RNG (`Rand16 ^ system-time ^
waitCounter`), zeros the eight-slot-per-player score arrays (`_playerKills`/`Deaths`/
`Damage`/`Revives`/`KillRatio`/`Scores`, plus the human-vs-AI splits), resets the scenario
end-conditions, randomises the wind, and then initialises **every sim subsystem in a fixed
order** — `T_Init`, `OBJInit(300000)`, `COLInit`, `CTInit`, `MSGInit`, `SAYInit`, `WNGInit`,
`GRPInit`, `APInit`, `PLANEInit`, `ROInit`, `PROJInit`, `ZONEInit`, `GRAPHICInit`. That call
list is the authoritative dependency order for the reconstruction's mission boot. confirmed

`MISSIONTextProc` then builds the objects, and **`MISSIONInit2`** (`0x480A30`) runs the
post-load pass: assign sides (`MAPSetSide`), find the human stations, resolve name aliases
(`OBJAliasAll`/`OBJAliasForMulti`), set the mission clock, load the `.MC` event proc on the
host, and take the first `MISSIONCheckSuccess`. `MISSIONInit3` seeds the radio home/succeeded
flags, and `MISSIONInitMedalInfo` records whether the player has wingmen (medal
eligibility). `MISSIONShutdown` (`0x4819F0`, guarded so it runs once) unwinds the same
subsystems and frees the mission's RM/MM allocation id. confirmed

**Anti-cheat.** In multiplayer the mission verifies that no station has edited its aircraft
or weapon data. `ComputeCRC` (`0x480230`) runs a CRC-32 (`_crc_table`) over a *class-specific
prefix* of each `OBJ_TYPE` record — `0xA6` bytes for a plane, `0xBA` for a ground vehicle,
`0x1BC` for ordnance, `0x13B` for a projectile — after zeroing the record's variable tail so
only the performance-tuning bytes count. Each station shares its per-file CRCs
(`UpdateAntiCheat` → `PostAntiCheat` → `MPSendAntiCheat`) into a shared `FILE_CRC` table, so a
souped-up `.PT`/`.OT` is detectable; `PostCheatsOn` broadcasts whether a player has the cheat
menu enabled. Fort missions exempt the ordnance class (defenders carry fixed loadouts).
confirmed

## Scoring, stats & end-of-mission (#485)

`_MISSIONAddScore@12` (`0x486580`) accumulates the running score. The **stats accumulators**
bin each event: `WpnStats` (shots / hits / kills / misses per weapon, into the
`StatsBucketFor` bucket), `KillStats` (one kill into the right one of 13 categories —
friendly-fire, plane, fort-gun, helicopter, ship, ground vehicle, SAM, AAA, structure, …,
split player vs wingman, keyed on the victim's class and type-flag bits), and `LandingStats`
(landing count + a quality bonus). Each mirrors to peers via its `MP*` twin, and Jane's
Online games count only human-target events. `_EndOfMissionStats@0` (`0x484D90`) and
`_EndOfFortMissionStats@0` roll these into the pilot record at mission end. confirmed

The **scoreboard** ranks players by `MISSIONScore` (`0x4864D0`) under the `_scoreBy` metric
(kills / kill-ratio / damage); `MISSIONSortPlayers` qsorts them and `MISSIONScoreSides` sums
the friendly-vs-enemy team totals (capped at `0x3E700`). **`MISSIONEndScenario`** (`0x486160`)
is the multiplayer end-condition test: a time limit, four kill-goal modes (team total /
either side reaches N / any single player / the enemy team), the Fort win via
`MISSIONFortWin` (`0x80` friendly, `0x800000` enemy), and the Jane's Online all-dead /
out-of-revives condition — arming `SetScenarioEndTime` when any fires. The Fort scenarios
track objectives through `_MISSIONFortDestroyed@4`, `_MISSIONFortDestroyedByFort@4`,
`_MISSIONFortStatus@4`, and the multiplayer **Fort-setup dialog** (`FortMissionSetup`
`0x420830`, the `FortMultiButton*` radio groups, and the fort-type name list). (The `Score*`
functions in the sound range are the **music** score, not this.) confirmed

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
