# Research Backlog

Outstanding RE and documentation tasks, grouped by effort.

---

## Binary Analysis (no disassembly tool required)

- **PLT field gap (0xB0–campaign block start)**: Field layout from offset `0xB0` to the campaign block start is unmapped. Method: diff two pilot saves with known differences (aircraft, loadout) byte-by-byte. See [formats/PLT.md](formats/PLT.md). *(Requires gameplay — 4-pass methodology documented in PLT.md)*

- **T2 sub-header class constants and surface class**: Bytes 4–16 are class constants (3 distinct values by grid size — confirmed). Remaining: decode their world-space meaning (requires Ghidra trace). Determine surface class byte → PIC atlas tile mapping. Confirm tile-summary record 0 algorithm (not NW corner, not dominant type — requires Ghidra). See [formats/T2.md](formats/T2.md).

---

## Win32 PE DLL Disassembly

For each item: load the overlay DLL in Ghidra, import the FA.SMS symbol list via `scripts/ghidra/import_sms.py`, trace from the DLL's exported entry point.

- **HUD advisory icon bit names**: Bits 0–4 confirmed from `_DAMAGEDoHit@12` damage states; bits 6–11 and 13 confirmed with actuator functions; bits 15–18 confirmed as carrier glideslope phase indicators. Remaining: bit 14 (`0x04000`) writer unknown; damage overlay function that reads bits 0–4 and 28–31 not yet identified. See [formats/HUD.md](formats/HUD.md).

- **HGR hangar layout**: Two files confirmed: `H_AIRB.HGR` (land base) and `H_AIRB2.HGR` (carrier / alternate airbase). Disassemble to extract the hangar layout table — aircraft slot positions, icon placement, camera angle. See [formats/HGR.md](formats/HGR.md).

- **OT/NT `ot_flags` bit semantics**: Bits 5, 8, 11, 15, 22 confirmed via Ghidra; bits 17, 20, 21 confirmed by BRF survey. Still inferred only: bit 10 (OT — civilian/dual-use infrastructure) and NT bits 18, 19, 20, 25, 26 — no entity+0x09 bit-test found in captured functions. See [formats/OT.md](formats/OT.md) and [formats/NT.md](formats/NT.md).

- **NT hardpoint bit 1 (`$2`) meaning**: "Surface-strike missile" hypothesis ruled out by BRF survey. Carrier approach functions test `OBJ_TYPE+0xba & 2` and `& 8` for approach-sequence dispatch — not hardpoint flags. Fire-control dispatcher trace still needed to confirm exact role. See [formats/NT.md](formats/NT.md).

- **GAS capacity word unit**: `word` values (108/198/248/315) have no linear or volumetric relationship to gallons or lbs. Search FA.SMS for fuel-system symbols (e.g. `GAS`, `fuel`, `tank`), trace the routine that reads the `word` field and adds it to the aircraft fuel pool. See [formats/GAS.md](formats/GAS.md).

---

## Mission System Formats

These formats (AI scripts, campaign state, mission conditions, theater maps) interact at runtime. Most are text-based but some have binary sections or reference binary resources.

- **AI script `move`/`jink` semantics and `.BI` bytecode**: `<speed_mode>` and `<value>` arguments for `move` and `jink` not yet confirmed (requires FA.EXE script interpreter trace). `.BI` bytecode format (opcode table, argument encoding) not decoded. See [formats/AI.md](formats/AI.md).

- **CAM binary layout**: Disassemble `UKRAINE.CAM` to confirm the binary layout of the mission state and weapon tables (offsets, sizes, field encoding). Identify which `.MC` files correspond to which campaigns/missions. Determine how `.CAM` references theater `.MM` files (if at all). See [formats/CAM.md](formats/CAM.md).

- **MC condition check logic**: Disassemble `UKR01.MC` to trace the complete condition check logic and identify all object aliases it monitors. Determine how the `.CAM` file loads `.MC` files at mission start. Clarify `EXTRA01.MC` purpose (bonus mission, multiplayer extra, or other — imports are known but mission context is not). See [formats/MC.md](formats/MC.md).

- **MM world-space fields**: Determine world-space coordinate scale and origin for `pos`/`view` values. Confirm `obj flags` bit 9 (mission-critical?) and bit 10 (friendly vs hostile ownership?) semantics — requires Ghidra. Confirm `tdic id=256` meaning (tile type index into T2?). See [formats/MM.md](formats/MM.md).

---

## Format Deep Dives (BRF Numeric Fields)

- **JT physics gap bytes**: PROJ_TYPE+0x50–0x54 and +0x56–0x6E (30 bytes total) unresolved — likely turn rate, g-limit, and other flight-model parameters. `_PROJProc` symbol not found in Ghidra; requires virtual-dispatch trace from the projectile update loop. See [formats/JT.md](formats/JT.md).

- **JT warhead flags bits 1–3, 5–6**: All other warhead flag bits confirmed. Bits 1–3 and 5–6 have no function found testing them from `missile+0xa6`; may be structural/unused flags. See [formats/JT.md](formats/JT.md).

---

## Undocumented Loose Files

- **EA.CFG**: Map all fields by toggling settings in-game and diffing. Cross-reference `CN_ReadConfig` symbol in FA.SMS. See [formats/CFG.md](formats/CFG.md).

- **NET.DAT**: Map multiplayer network config fields. Cross-reference `CN_INFO` struct via FA.SMS. Confirm whether NET.DAT holds one transport block or a union of all transport configs. See [formats/NET.md](formats/NET.md).
