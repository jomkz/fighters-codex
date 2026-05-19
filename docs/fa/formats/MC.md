# Mission Condition Script (.MC)

FA_2.LIB contains 21 `.MC` files. Each implements the runtime condition checks for a specific mission event — trigger conditions, completion logic, and failure detection. Each is a **Win32 PE DLL** loaded at runtime.

## File Inventory

All 21 filenames:

`CATFAIL.MC`, `EXTRA01.MC`, `FOO.MC`, `K16.MC`, `K17.MC`, `TRAIN01.MC`, `U01.MC`, `U07.MC`, `U08.MC`, `U11.MC`, `U12.MC`, `U15.MC`, `U22.MC`, `U23.MC`, `U24.MC`, `U25.MC`, `U29.MC`, `U34.MC`, `UKR01.MC`, `UKR02.MC`, `VIET03.MC`

- `U*.MC` — Ukraine campaign mission conditions (not all 50 missions have a dedicated `.MC`; only those with non-trivial trigger logic)
- `K*.MC` — Kurile campaign missions
- `VIET03.MC` — Vietnam campaign mission 3
- `CATFAIL.MC` — carrier takeoff failure condition
- `TRAIN01.MC` — training mission condition
- `UKR01.MC`, `UKR02.MC` — Ukraine campaign-level events
- `EXTRA01.MC` — generic bonus-mission condition gate (uses `@OBJAlias@8`, `@OBJGet@4`, `_MISSIONSuccess@0`; no debug strings). Used by all `EXTRA01.M`–`EXTRA20.M` standalone bonus missions and `BEXTRA01.M`–`BEXTRA13.M` Baltic bonus missions via the `code extra01` directive in each `.M` file.
- `FOO.MC` — developer timing test: embeds debug string `"The time is now >= 10 seconds!"`, uses `_currentTime` and `_MSGSendChatter@24` with `RADIOBP.5K` audio; name is a classic programmer placeholder

## Content

String analysis of all `.MC` files reveals the mission condition API:

| Import | Description |
|--------|-------------|
| `@OBJAlias@8` | Look up a game object by its alias ID |
| `@OBJGet@4` | Get a game object by index (EXTRA01.MC) |
| `_Dist@8` | Compute distance between two objects |
| `_MISSIONSuccess@0` | Trigger mission success outcome |
| `_MSGSendChatter@24` | Send a radio chatter message to the player |
| `_OnTheGround@0` | Test whether an object is on the ground |
| `_PopCurObj@0` | Pop the current object from the evaluation stack |
| `_PushCurObj@4` | Push an object onto the evaluation stack |
| `_currentTime` | Global: current game time in ticks |
| `_playerId` | Global: the player's object ID |

These are physics/world-state query functions — the `.MC` DLL polls game state each tick to detect mission trigger conditions (e.g. player landed, target destroyed, distance threshold crossed).

## Format

Win32 PE DLL. All observed `.MC` files decompressed to **4608 bytes**.

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 21 |

## Campaign Condition Text Files (.mc_M, .mc_nato_M)

These are **distinct** from the `.MC` PE DLL files above. The campaign engine (`FUN_00428412`) loads campaign-wide condition scripts as plain-text files with suffix `.mc_M` (standard campaign) or `.mc_nato_M` (NATO campaign variant). Loaded via `FUN_00481940` → `FUN_00481c10` (text parser using `FUN_00483c90` as tokenizer + `__strlwr`).

Confirmed keywords parsed by `FUN_00481c10`:

| Keyword | Effect |
|---------|--------|
| `textformat` | Sets file format version/type flag |
| `briefmap` | Sets `DAT_005516e0 = 1` (briefing map active) |
| `selectplane` | Sets `DAT_0054e498 = 1` (plane selection screen) |
| `armplane` | Sets `DAT_00552820 = 1` (arming screen) |
| `layer` | Reads layer name and integer parameter |
| (startup coords) | Reads 3 fixed-point world-space coordinates (× 256) |

These files are stored in the LIB archive with `.mc_M` / `.mc_nato_M` suffixes, not `.MC`. They are text keyword files, not Win32 PE DLLs.

## Dispatch Chain (Confirmed)

The full MC dispatch chain from game entry:

```
?usnfmain@@YAXXZ (0x403700)          -- main loop
FUN_00428412 (0x428412)              -- campaign/mission loader
  └─ _CallMissionProc_8 (0x481940)   -- central DLL dispatcher
       ├─ _MISSIONInit2_0 (0x480b50) -- post-load init; assigns _eventFilterProc
       ├─ _MISSIONTextProc@16 (0x481c10)   -- re-entry: text condition parse
       └─ _MISSIONCheckSuccess@0 (0x486860) -- re-entry: success test
```

`_MISSIONInit2_0` (0x480b50) iterates all `_objPtrs`, calls `_MAPSetSide_4`, `_OBJFindHumans_0`, `_OBJAliasAll_12`, `_OBJAliasForMulti_0`, `_TIMEInit_12`, `_G_SetScaleMax_8`, then loads the mission DLL via `_RMAccess_8(&_missionDLLName__3PADA, 0x8000)`, storing the result in `_eventFilterProc`.

Confirmed condition keyword consumers:

| Keyword | Consumer functions |
|---------|-------------------|
| `DESTROY` | `FUN_0043a5c0`, `FUN_00431ab0`, `FUN_0044fe10` |
| `FAIL` | `FUN_004a2a41` |
| `tmap` | `_MISSIONTextProc@16` (string at `0x4fc228`); `FUN_00495e80` (`.MC` handler at `0x495e80`, string at `0x5010f4`) |

The `cond` keyword **does not appear** as a handled keyword in `_MISSIONTextProc@16` — exhaustive read of the ~1,500-line decompile (`DumpAllFunctions.txt` lines 101716–103259) found no `"cond"` string comparison or dispatch branch. `cond` is absent from FA's condition parser; it may be a keyword from a different Jane's title or an unreachable dead branch stripped from this build.

## Condition Function Protocol (Confirmed)

`AnalyzeMCDLL.java` confirmed the condition function for U34.MC (representative of all single-condition MC DLLs). The exported entry is at PE code offset `0x1000`.

**Function signature:**
```c
short FUN_00001000(short param_1, undefined4 param_2, undefined4 param_3, short *param_4);
```

**Command encoding via `*param_4`:**

| `*param_4` | Behavior |
|-----------|----------|
| `0x20` (32) | Pass-through: return `param_1` unchanged (query current state) |
| `0x00` | Evaluate condition: if `DAT_00001212 != 0` AND `DAT_00001211 == 0`, return `DAT_00001212`; else return 0 |

**Data section layout (PE offsets):**
- `DAT_00001211` (offset `0x1211`): alive/active inhibit flag — condition fires only when this is zero
- `DAT_00001212` (offset `0x1212`): target status byte — the success code returned when the condition triggers

FA.EXE writes these bytes to track the mission state for the DLL. When `*param_4 = 0x00`, if the target is destroyed (`DAT_00001212 != 0`) and not still alive (`DAT_00001211 == 0`), the MC DLL returns the success code and FA.EXE calls `_MISSIONSuccess@0` to end the mission.

The `.idata` string scan confirmed `_MISSIONSuccess` at offset `0x207F` and `_OBJAlias` at offset `0x2065`.

## Related

- [CAM.md](CAM.md) — campaign engine that loads `.MC` files to evaluate mission state
- [M.md](M.md) — `.M` mission files whose events `.MC` evaluates
