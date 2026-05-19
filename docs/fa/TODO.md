# Research Backlog

Outstanding RE and documentation tasks, grouped by blocker type. Only open items are listed — resolved items are recorded inline in the individual format docs.

---

## Requires Gameplay (differential save pass)

- **PLT text-region gaps (`0xB0`–`0xC1`, `0xCF`–`0x5AE`, `0x2018`–`0x20B7`, `0x21F8`–`0x25DF`)**: The numeric stats (kill tallies, mission counters, weapon accuracy at `0x1F80`–`0x21F7`) are fully mapped from RE. Remaining unknowns span four ranges: 18 bytes at `0xB0`–`0xC1` (possibly score level or rank index), 1,344 bytes at `0xCF`–`0x5AE` (between secondary string and mission log), 160 bytes at `0x2018`–`0x20B7` (between kill tallies and weapon accuracy blocks), and ~1,000 bytes at `0x21F8`–`0x25DF` (tail region — likely fort/campaign-phase stats and multiplayer scoring). Differential save is the most direct path: vary rank/score/missions, compare saves at those ranges. See [formats/PLT.md](formats/PLT.md).

- **NET.DAT callsign / session-name offsets**: Callsign and session name are user-visible in the multiplayer lobby screen but their byte offsets within `NET.DAT` are not mapped. Differential approach: write two NET.DAT files with different callsigns and compare. Also needed: confirm whether the file holds one active transport block or a union of all transport configs (IPX + TCP/IP + serial/modem all present simultaneously). See [formats/NET.md](formats/NET.md).

---

## Requires Ghidra GUI (FA.EXE project)

Overlay DLL Ghidra projects already imported under `%FA_PROJECT%\overlay_projects\`. FA.EXE project at `%FA_PROJECT%\fa-re.gpr`. These items have inline-only, pointer-accessed, or vtable-dispatched consumers that headless scripts cannot resolve.

- **NT `ot_flags` bit 25 (`$2000000`)**: No entity `ot_flags` test found in FA.EXE full decompile (`DumpAllFunctions.txt`) or in any analyzed overlay DLL. Only `_gamePrefs & 0x2000000` hits exist — not entity-level tests. Label "Emplaced AA artillery" remains inferred from unit distribution (KS12/KS19/M1939 all carry `$2000000`). *Approach: import terrain overlay DLL (not yet identified — search FA install dir for DLLs containing `0x2000000` constant), then search for entity+0x09 tests.* See [formats/NT.md](formats/NT.md).

- **AI / BI opcode 0x28 FRAME consumer (reader)**: Writer fully confirmed: `FUN_00466a80` case 0x28 (`DumpAllFunctions.txt` line 78118) reads 4 bytes from bytecode stream → `DAT_00546c44`/`DAT_00546c46` (CT state block `+0x7c`/`+0x7e`), advances PC by 4, with a stack-imbalance guard (`DAT_00546c8c != '\0' && DAT_00546c42 != 0 → FUN_00466820(0xc)`). Reader not found — accessed via pointer to the state block (`*(DAT_0050cf90) + 0x7c`), invisible to direct-address offset scans. All 16 `findFunctionsReadingOffsets` candidates at offset +0x7c were false positives. *Approach: cross-reference `DAT_00546bc8` in the FA.EXE Ghidra GUI to find all struct-pointer loads of the 128-byte CT state block.* See [formats/AI.md](formats/AI.md) and [formats/BI.md](formats/BI.md).

- **JT physics gap (`+0x50`–`+0x54`, residual)**: `_PROJMoveProc` (0x4c11b0) decompiled via `dumpAtForced` (2026-05-19). Motor phase thresholds (`+0x57`/`+0x59`/`+0x5B`/`+0x5D`), seeker search params (`+0x69`/`+0x6B`/`+0x6D`), smoke trail (`+0x70`–`+0x74`), and warhead bits 1/5/6/12/13 are confirmed. Remaining: `+0x50`–`+0x54` (entity offset table: reaction params / mode byte) and scattered bytes `+0x56`/`+0x58`/`+0x5A`/`+0x5C`/`+0x5E`–`+0x64` — these addresses overlap the aircraft flight model (BRF entity in scratchpad), so missile-specific semantics cannot be isolated without a type-filtered trace. *Approach: Ghidra GUI, filter `_PROJMoveProc` decompile for reads to `entity+0xF6`–`0xFA` range specifically, or inspect `FUN_004c1630` / `FUN_004c1660` (guidance algorithm targets) for offset reads in that range.* See [formats/JT.md](formats/JT.md).

- **HUD state flags (`DAT_0050cfef`) bits 28–31 — cockpit display consumer**: `_DAMAGEDoHit@12` writes these bits; bit 30 (`0x40000000`) is also read by the carrier HGR hangar renderer (`AnalyzeHGR.txt` lines 4143, 4315). The cockpit warning-display consumer for all four bits has not been identified in the decompile. *Approach: search `DumpAllFunctions.txt` for `& 0x10000000`, `& 0x20000000`, `& 0x40000000`, `& 0x80000000` in functions near the HUD draw loop; or Ghidra GUI xref on `DAT_0050cfef`.* See [formats/HUD.md](formats/HUD.md).

- **HUD per-aircraft anchor point source**: `FUN_00406040` initializes the HUD anchor with `0x10 0x10` defaults. Whether the actual per-aircraft anchor position is loaded from the `.PT` type record or a separate config is unconfirmed. *Approach: Ghidra GUI xref on `FUN_00406040` to find the caller that overrides the default, then trace to the PT field.* See [formats/HUD.md](formats/HUD.md).

- **FA.CFG — `DAT_004f8bf9` / `DAT_004f8c19`**: Written at session end; likely a joystick calibration filename or NATO/campaign-mode name string saved to config. *Approach: Ghidra GUI xref on both addresses to find all read and write sites.* See [formats/CFG.md](formats/CFG.md).

- **LAY DLL header `+0x00` — count/flags dword**: Copied to `DAT_00580db0` during `ParseLayerFile` but no function reads `DAT_00580db0` anywhere in the headless decompile. *Approach: cross-reference `DAT_00580db0` in the Ghidra GUI to locate the consumer; confirm whether this is a format-version guard or a runtime count.* See [formats/LAY.md](formats/LAY.md).

- **NET.DAT — `CN_INFO` struct full layout**: `CN_ReadConfig` / `CN_WriteConfig` copy 0xDDC bytes (3548 bytes) between the file and the struct. The field layout of all transport-specific sub-blocks (IPX, TCP/IP, serial/modem offsets, port numbers, baud rate) is not confirmed. *Approach: Ghidra GUI, open the `CN_ReadConfig` area, trace all field-write instructions to recover the struct skeleton; cross-check against `CN_WriteConfig` symmetry.* See [formats/NET.md](formats/NET.md).

---

## Requires Overlay DLL Ghidra Project

- **T2 surface class → PIC atlas mapping**: BIT2 binary parser absent from FA.EXE — confirmed via `DumpAllFunctions.txt` (no BIT2 string, no inline magic comparison found). `@T_Load@4` (0x4c5d70) and `FUN_004d3064` are FA.EXE call sites only; `FUN_004d3064` computes a Chebyshev LOD metric and writes the result into `DAT_00515f94 + surface_class_index + 3`. Sub-header bytes 4–16 class constants and tile-summary record 0 selection algorithm remain open. **Terrain overlay DLL not yet identified** — not among the standard overlay types (BI/CAM/MC/HUD/LAY/FNT/MUS) extracted from FA_2.LIB. *Approach: search the FA install directory for DLLs containing the "BIT2" string; import into a new Ghidra project; locate the BIT2 magic handler; trace surface class → PIC atlas mapping.* See [formats/T2.md](formats/T2.md).

- **CAM weapon/aircraft table — binary gap (`0x16eb`–`0x17bb`, 209 bytes)**: UKRAINE.CAM CODE section string tables (weapon filenames, aircraft IDs, mission names, state ID strings) are fully decoded. The 209-byte binary block between the mission filename list and the campaign outcome strings contains numeric data — likely mission availability bitmasks (7 bytes = 56 bits for 50 missions), per-mission completion counters, or campaign-phase boundary indices. *Approach: UKRAINE.CAM Ghidra project (`%FA_PROJECT%\overlay_projects\cam\`); trace writes from the campaign DLL entry point (`FUN_00001000`) into the binary gap to identify field semantics.* See [formats/CAM.md](formats/CAM.md).

- **BI DLL PE layout (AI→BI compiler prerequisite)**: The BI DLL interleaves compiled AI bytecode with native x86 `_CTDo_*`/`_CTEval_*` implementations. The PE section layout — specifically which bytes are bytecode vs. x86 code, and where the bytecode array begins — is not yet mapped, blocking correct bytecode embedding for an AI→BI compiler. BI overlay projects already imported under `%FA_PROJECT%\overlay_projects\bi\`. *Approach: open any BI DLL in the Ghidra overlay project; inspect CODE section; locate the bytecode start via the `_CTExecProgram_4` load call that sets `DAT_00546be6`.* See [formats/BI.md](formats/BI.md).

---

## Minor RE / Low Priority

Items likely solvable from existing data (DumpAllFunctions.txt, format file inspection, or a brief live test) but not yet pursued.

- **AI `move` / `jink` argument names and semantics**: `move` bytecode pops heading, angle, alt/roll, speed, duration — the exact correspondence to AI source syntax needs live confirmation. `_MVRJink@40` (0x4ac9e0) jink params confirmed: `param_8`=count, `param_9`=ctrl, `param_10`=duration, `param_3`/`param_4`=deflection angles — `ctrl` meaning needs live test. See [formats/AI.md](formats/AI.md).

- **MC `cond` keyword handler**: `_MISSIONTextProc@16` handles many `.MC` keywords; the `cond` handler was not identified in the headless decompile. *Approach: grep `DumpAllFunctions.txt` for the string `"cond"` near `MISSIONTextProc` or its callees.* See [formats/MC.md](formats/MC.md).

- **NT `ot_flags` bit 18 (`$40000`) — carrier-landing handler**: Confirmed as fuel-bar capacity doubler in two display functions. Whether an additional carrier-landing code path also tests this bit is unconfirmed. Low impact — does not affect gameplay modding. See [formats/NT.md](formats/NT.md).

- **DLG secondary-display rendering semantics**: `secondary_display_x/y` (+0x12/+0x14) and `scroll_base` (+0x1E) fields confirmed. Exact rendering behaviour of `FUN_0048a7d0` (what constitutes "secondary item display" vs. primary) not yet confirmed. See [formats/DLG.md](formats/DLG.md).

- **HGR second filename and `0x8104` resource key**: The second `.HGR` filename (likely `H_AIRB2.HGR` — carrier hangar) is not confirmed. The `_RMAccess_8(local_50, 0x8104)` sub-resource key meaning (combined flags+type passed to the resource manager) is unconfirmed. See [formats/HGR.md](formats/HGR.md).

---

## Implementation (RE complete — ready to build)

Formats whose structure is fully documented. All need a lib parser and a `ft <fmt>` CLI command.

### Trivial (existing lib, just needs CLI wiring)

- **`ft pal` CLI command**: `pal.cpp` / `pal.h` already exist in lib. Add `ft pal info <file>` and `ft pal unpack <file>` to expose palette dump and PNG export.

### Low effort (format fully documented, straightforward parsing)

- **INF parser**: Dot-command markup (`.body`, `.title`, `.center`, `.left`) plus `LENGTH`/`HEIGHT`/`WINGSPAN`/`WEIGHT`/`PERFORMANCE` footer key-values. Implement text parser → `ft inf unpack <file>` outputs structured JSON. See [formats/INF.md](formats/INF.md).

- **HUD data-section parser**: Fixed 0x2BB CODE-section layout confirmed; gauge parameter offsets and anchor-point coordinates documented. Implement PE section reader → decode gauge table → `ft hud dump <file>` outputs `{aircraft, gauges: [{name, x, y}]}`. See [formats/HUD.md](formats/HUD.md).

### Medium effort (PE DLL reader required, structs known)

- **LAY parser**: LAYER struct (fog_density, color_entry_ptr, vis_lo/vis_hi, fog_alt_low/alt_high) and 30-dword header block confirmed. Implement PE data-section extractor → decode LAYER array → `ft lay dump <file>` exports sky/atmosphere parameters. See [formats/LAY.md](formats/LAY.md).

- **FNT glyph extractor**: FONT struct (font_height dword + 256 glyph function pointers + 256 width table) fully mapped; x86 glyph execution semantics (ADD EDI,ECX / MOV [EDI],AL) documented. Implement PE section parser → extract glyph metrics → `ft fnt unpack <file>` exports metrics CSV and per-glyph PNGs. See [formats/FNT.md](formats/FNT.md).

- **MUS bytecode disassembler**: All opcodes documented (FF playlist ID, FA setup, FB play XMI, FE conditional, FD loop/jump, FC shuffle); game-state IDs confirmed. Implement PE CODE-section reader → decode opcode sequence → `ft mus dump <file>` exports human-readable script. See [formats/MUS.md](formats/MUS.md).
