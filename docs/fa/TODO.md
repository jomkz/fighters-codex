# Research Backlog

Outstanding RE and documentation tasks, grouped by effort.

---

## Binary Analysis (no disassembly tool required)

- **PLT field gap (0xB0–campaign block start)**: Field layout from offset `0xB0` to the campaign block start is unmapped. Method: diff two pilot saves with known differences (aircraft, loadout) byte-by-byte. See [formats/PLT.md](formats/PLT.md). *(Requires gameplay — 4-pass methodology documented in PLT.md)*

- **T2 sub-header class constants and surface class**: Bytes 4–16 are class constants (3 distinct values by grid size — confirmed). `DumpT2Loader.java` ran: **BIT2 magic not in FA.EXE** — T2 binary loading is in an overlay DLL. FA.SMS has no T2-related symbols. MM text parser confirmed as `FUN_00481c10`: `tmap` (4 × s16, max 3500), `tmap_named` (name + 2 × s16, 53-byte struct), `tdic` (u32 + 32 × u8, max 300). Sub-header constant 0x95 found in `FUN_0043faf0`, `FUN_004672c0`, `FUN_0046eedf`, `FUN_00480230` — not yet analyzed. Surface class byte → PIC atlas mapping and tile-summary record 0 algorithm also unresolved. See [formats/T2.md](formats/T2.md).

---

## Win32 PE DLL Disassembly

For each item: load the overlay DLL in Ghidra, import the FA.SMS symbol list via `scripts/ghidra/import_sms.py`, trace from the DLL's exported entry point.

- **HUD advisory icon bit names**: Bits 0–4 confirmed from `_DAMAGEDoHit@12` damage states; bits 6–11 and 13 confirmed with actuator functions; bits 15–18 confirmed as written by `FUN_0049fb70` (`_PLANECheckFuel@0`) via `FUN_00452140`. Bit 14 writer partially identified: `DumpHUDBit14Search.java` ran — 215 write refs across 55 functions, none use a direct `0x4000` constant. MP writer = `?MPReceive@@YGDXZ` (0x46C980, decompile failed); SP writer = unanalyzed code at 0x4bc177/0x4bc190. Read in `FUN_004164b0` during ejection states 0x11/0x12. Structural: `DAT_0050cfef` = player entity + 0x16F. To resolve SP writer: raw disasm of 0x4bc177. Damage overlay function reading bits 0–4 and 28–31 not yet identified. See [formats/HUD.md](formats/HUD.md).

- **HGR hangar layout**: Two files confirmed: `H_AIRB.HGR` (land base) and `H_AIRB2.HGR` (carrier / alternate airbase). Loading mechanism confirmed (`FUN_004543c0`): slot entries at offset +0x2D, 30 × 8-byte entries, X/Y coordinate arrays built at load. Remaining: decode the 8-byte slot entry structure and `FUN_004a6cc0` sub-resource. See [formats/HGR.md](formats/HGR.md).

- **OT/NT `ot_flags` bit semantics**: Bits 5, 8, 11, 15, 22 confirmed via Ghidra; bits 17, 20, 21 confirmed by BRF survey. Still inferred only: bit 10 (OT — civilian/dual-use infrastructure) and NT bits 18, 19, 20, 25, 26 — no entity+0x09 bit-test found in captured functions. See [formats/OT.md](formats/OT.md) and [formats/NT.md](formats/NT.md).

---

## Mission System Formats

These formats (AI scripts, campaign state, mission conditions, theater maps) interact at runtime. Most are text-based but some have binary sections or reference binary resources.

- **AI script `.BI` bytecode opcode 0x28 FRAME**: Opcode table (all 40 opcodes) confirmed from `FUN_00466a80`. FRAME writes two s16 values to `DAT_00546c44`/`DAT_00546c46` — binary survey of all 9 `.BI` files confirms every dispatch entry and handler label is prefixed with FRAME; first s16 is a sequential block ID (IDs 1–6 compiler-reserved; dispatch chains start at 7); second s16 is monotonically increasing and is not a valid code pointer. DAT_00546c44/46 are written but no reader was found in the scanned interpreter range — likely a profiling/priority subsystem. Remaining: find what reads these globals. See [formats/AI.md](formats/AI.md).

- **CAM binary layout**: Disassemble `UKRAINE.CAM` to confirm the binary layout of the mission state and weapon tables (offsets, sizes, field encoding). Identify which `.MC` files correspond to which campaigns/missions. Determine how `.CAM` references theater `.MM` files (if at all). See [formats/CAM.md](formats/CAM.md).

- **MC condition check logic**: `.mc_M` / `.mc_nato_M` text-format campaign condition scripts now confirmed and keywords partially mapped (see [formats/MC.md](formats/MC.md)). `.MC` PE DLL per-mission condition files: disassemble `UKR01.MC` to trace complete condition check logic and all object aliases. Clarify `EXTRA01.MC` purpose. Clarify how `.CAM` references or orders `.MC` files.

- **MM world-space fields**: `?MAPWorldToScreen` formula confirmed; **world-space unit = 1 foot confirmed** (calibrated from JT.md seeker ranges: 50,000 units = 8.2 nm ✓). Remaining: confirm `obj flags` bit 9 (mission-critical?) and bit 10 (friendly vs hostile ownership?) semantics — requires Ghidra. Confirm `tdic id=256` meaning (tile type index into T2?). See [formats/MM.md](formats/MM.md).

---

## Format Deep Dives (BRF Numeric Fields)

- **JT physics gap bytes**: PROJ_TYPE+0x50–0x54 and +0x56–0x6E (30 bytes total) unresolved — likely turn rate, g-limit, and other flight-model parameters. Prior `DumpPROJPhysics3` scanned for PROJ_TYPE offsets 0x50–0x7F but found nothing; the entity-relative equivalents (entity+0xF6–0x114) were NOT scanned. New `DumpPROJDispatch.java` scans the full binary for entity offsets 0xF6–0x114 and dumps the complete 0x4C0000–0x4C3000 PROJ range (pending run). See [formats/JT.md](formats/JT.md).

- **JT warhead flags bits 1–3, 5–6**: All other warhead flag bits confirmed. Bits 1–3 and 5–6 have no function found testing them from `missile+0xa6`; may be structural/unused flags. See [formats/JT.md](formats/JT.md).

---

## Undocumented Loose Files

- **EA.CFG**: Map all fields by toggling settings in-game and diffing. Cross-reference `CN_ReadConfig` symbol in FA.SMS. See [formats/CFG.md](formats/CFG.md).

- **NET.DAT**: Map multiplayer network config fields. Cross-reference `CN_INFO` struct via FA.SMS. Confirm whether NET.DAT holds one transport block or a union of all transport configs. See [formats/NET.md](formats/NET.md).

---

## Implementation (RE complete — ready to build)

Formats whose structure is fully or sufficiently documented to implement. All need a lib parser and a `ft <fmt>` CLI command unless noted.

### Trivial (existing lib, just needs CLI wiring)

- **`ft pal` CLI command**: `pal.cpp` / `pal.h` already exist in lib. No CLI command is wired up. Add `ft pal info <file>` and `ft pal unpack <file>` to expose palette dump and PNG export.

### Low effort (format fully documented, straightforward parsing)

- **INF parser**: Dot-command markup (`.body`, `.title`, `.center`, `.left`) plus `LENGTH`/`HEIGHT`/`WINGSPAN`/`WEIGHT`/`PERFORMANCE` footer key-values. Implement text parser → `ft inf unpack <file>` outputs structured JSON. See [formats/INF.md](formats/INF.md).

- **HUD data-section parser** *(distinct from HUD advisory bits RE task above)*: Fixed 0x2BB CODE-section layout confirmed; gauge parameter offsets and anchor-point coordinates documented. Implement PE section reader → decode gauge table → `ft hud dump <file>` outputs `{aircraft, gauges: [{name, x, y}]}`. See [formats/HUD.md](formats/HUD.md).

### Medium effort (PE DLL reader required, structs known)

- **LAY parser**: LAYER struct (fog_density, color_entry_ptr, vis_lo/vis_hi, fog_alt_low/alt_high) and 30-dword header block confirmed. Implement PE data-section extractor → decode LAYER array → `ft lay dump <file>` exports sky/atmosphere parameters. See [formats/LAY.md](formats/LAY.md).

- **FNT glyph extractor**: FONT struct (font_height dword + 256 glyph function pointers + 256 width table) fully mapped; x86 glyph execution semantics (ADD EDI,ECX / MOV [EDI],AL) documented. Implement PE section parser → extract glyph metrics → `ft fnt unpack <file>` exports metrics CSV and per-glyph PNGs. See [formats/FNT.md](formats/FNT.md).

- **MUS bytecode disassembler**: All opcodes documented (FF playlist ID, FA setup, FB play XMI, FE conditional, FD loop/jump, FC shuffle); game-state IDs confirmed. Implement PE CODE-section reader → decode opcode sequence → `ft mus dump <file>` exports human-readable script. See [formats/MUS.md](formats/MUS.md).
