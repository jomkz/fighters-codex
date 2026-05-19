# Research Backlog

Outstanding RE and documentation tasks, grouped by blocker type. Only open items are listed — resolved items are recorded inline in the individual format docs.

## Requires Gameplay (differential save pass)

- **PLT text-region gaps (`0xB0`–`0xC1`, `0xCF`–`0x5AE`, `0x2018`–`0x20B7`, `0x21F8`–`0x25DF`)**: The numeric stats (kill tallies, mission counters, weapon accuracy at `0x1F80`–`0x21F7`) are fully mapped from RE. Remaining unknowns span four ranges: 18 bytes at `0xB0`–`0xC1` (possibly score level or rank index), 1,344 bytes at `0xCF`–`0x5AE` (between secondary string and mission log), 160 bytes at `0x2018`–`0x20B7` (between kill tallies and weapon accuracy blocks), and ~1,000 bytes at `0x21F8`–`0x25DF` (tail region — likely fort/campaign-phase stats and multiplayer scoring). RE confirmed: none of the four gap VA ranges have individual field accesses anywhere in the full decompile — only bulk block copies (`_SaveFile`/`_LoadFile_16` over the entire 0x25E0-byte struct). Differential save is genuinely the only path: vary rank/score/missions, compare saves at those ranges. See [formats/PLT.md](formats/PLT.md).

## Requires Ghidra GUI (FA.EXE project)

The remaining items genuinely require the interactive GUI — either because access is through a pointer (invisible to static offset scans), or because the consumer is not present in `DumpAllFunctions.txt` at all. The FA.EXE project is at `%FA_PROJECT%\fa-re.gpr`.

- **AI / BI opcode 0x28 FRAME consumer (reader)**: Writer fully confirmed: `FUN_00466a80` case 0x28 (`DumpAllFunctions.txt` line 78118) reads 4 bytes from bytecode stream → `DAT_00546c44`/`DAT_00546c46` (CT state block `+0x7c`/`+0x7e`), advances PC by 4, with a stack-imbalance guard. Reader not found — `DAT_00546c44`/`DAT_00546c46` have no direct-address reads in the full decompile; access is via a pointer (`*(DAT_0050cf90) + 0x7c`), invisible to static offset scans. All 16 `findFunctionsReadingOffsets` candidates at offset +0x7c were false positives. *Approach: cross-reference `DAT_00546bc8` in the FA.EXE Ghidra GUI to find all struct-pointer loads of the 128-byte CT state block.* See [formats/AI.md](formats/AI.md) and [formats/BI.md](formats/BI.md).

- **JT physics gap (`+0x50`–`+0x54`, residual)**: `_PROJMoveProc` (0x4c11b0) decompiled (2026-05-19). Motor phase thresholds, seeker search params, smoke trail, and warhead bits 1/5/6/12/13 confirmed. Remaining: `+0x50`–`+0x54` (entity offset table: reaction params / mode byte) and scattered bytes `+0x56`/`+0x58`/`+0x5A`/`+0x5C`/`+0x5E`–`+0x64` — these addresses overlap the aircraft flight model (BRF entity in scratchpad); no missile-specific reads isolated. *Approach: Ghidra GUI, filter `_PROJMoveProc` decompile for reads to `entity+0xF6`–`0xFA` specifically, or inspect `FUN_004c1630` / `FUN_004c1660` for offset reads in that range.* See [formats/JT.md](formats/JT.md).

- **NET `CN_INFO` IPX sub-block (`[0xc0]`–`[0x8e3]`, ~2,180 bytes)**: No direct field accesses for this range visible in the FA.EXE decompile — all IPX-specific reads go through the protocol vtable at `_proto_ptr + 0x3e`. *Approach: Ghidra GUI xref on `_proto_ptr` vtable to find the IPX vtable implementation, or import `IP.EXE` into a Ghidra project and trace the IPX sub-block field layout directly.* See [formats/NET.md](formats/NET.md).

## Requires Ghidra GUI (Terrain Renderer)

- **T2 surface class → PIC atlas mapping**: No terrain overlay DLL exists — `ft lib ls FA_2.LIB` lists no unrecognized types; `FA_1.LIB` contains only FNT/PIC. The BIT2 parser and all terrain rendering functions (`@T_Load@4` 0x4c5d70, `@T_GetLeaf@12` 0x4c6040, `FUN_004d3064` 0x4d3064) are in FA.EXE. `@DoSetTmapRemaps@0` (0x4cc518) builds a 256-byte `_tmapRemapTable` by iterating `Remap()` 256 times; the surface class byte (0–255) is the table index. Sub-header bytes 4–16 class constants and tile-summary record 0 selection algorithm remain open. *Approach: Ghidra GUI, trace from `@T_GetLeaf@12` return value → terrain polygon builder → how surface class indexes into `_tmapRemapTable`; inspect `@DoSetTmapRemaps@0` callers.* See [formats/T2.md](formats/T2.md).

## Implementation (RE complete — ready to build)

Formats whose structure is fully documented. All need a lib parser and a `ft <fmt>` CLI command.

### Trivial (existing lib, just needs CLI wiring)

- **`ft pal` CLI command**: `pal.cpp` / `pal.h` already exist in lib. Add `ft pal info <file>` and `ft pal unpack <file>` to expose palette dump and PNG export.

### Low effort (format fully documented, straightforward parsing)

- **INF parser**: Dot-command markup (`.body`, `.title`, `.center`, `.left`) plus `LENGTH`/`HEIGHT`/`WINGSPAN`/`WEIGHT`/`PERFORMANCE` footer key-values. Implement text parser → `ft inf unpack <file>` outputs structured JSON. See [formats/INF.md](formats/INF.md).

- **HUD data-section parser**: Fixed 0x2BB CODE-section layout confirmed; gauge parameter offsets and anchor-point coordinates documented. Implement PE section reader → decode gauge table → `ft hud dump <file>` outputs `{aircraft, gauges: [{name, x, y}]}`. See [formats/HUD.md](formats/HUD.md).

### High effort (compiler / toolchain work)

- **AI→BI compiler**: All 40 bytecode opcodes fully documented; PE layout confirmed (CODE section = pure bytecode at raw offset `0x400`; all `_CTDo_*`/`_CTEval_*` in FA.EXE, imported via `.idata`). Implementation: (1) parse `.AI` source → AST, (2) emit bytecode with CALL_BY_NAME / CALL_DIRECT / FRAME opcodes, (3) assemble a Phar Lap PE DLL with correct CODE + `.idata` sections importing `_CTDo_*`/`_CTEval_*` from `main`. No x86 code needed in the output DLL. See [formats/AI.md](formats/AI.md) and [formats/BI.md](formats/BI.md).

### Medium effort (PE DLL reader required, structs known)

- **LAY parser**: LAYER struct (fog_density, color_entry_ptr, vis_lo/vis_hi, fog_alt_low/alt_high) and 30-dword header block confirmed. Implement PE data-section extractor → decode LAYER array → `ft lay dump <file>` exports sky/atmosphere parameters. See [formats/LAY.md](formats/LAY.md).

- **FNT glyph extractor**: FONT struct (font_height dword + 256 glyph function pointers + 256 width table) fully mapped; x86 glyph execution semantics (ADD EDI,ECX / MOV [EDI],AL) documented. Implement PE section parser → extract glyph metrics → `ft fnt unpack <file>` exports metrics CSV and per-glyph PNGs. See [formats/FNT.md](formats/FNT.md).

- **MUS bytecode disassembler**: All opcodes documented (FF playlist ID, FA setup, FB play XMI, FE conditional, FD loop/jump, FC shuffle); game-state IDs confirmed. Implement PE CODE-section reader → decode opcode sequence → `ft mus dump <file>` exports human-readable script. See [formats/MUS.md](formats/MUS.md).
