# Research Backlog

Outstanding RE and documentation tasks, grouped by effort.

---

## Binary Analysis (no disassembly tool required)

- **PLT field gap (0xB0–campaign block start)**: Field layout from offset `0xB0` to the campaign block start is unmapped. Method: diff two pilot saves with known differences (aircraft, loadout) byte-by-byte. See [formats/PLT.md](formats/PLT.md). *(Requires gameplay — 4-pass methodology documented in PLT.md)*

- **GAS capacity word**: The `word` field (108/198/248/315) does not map linearly to US gallons or lbs, and no PT field shares the unit. Binary analysis is exhausted — requires FA.EXE fuel-system trace (see Win32 DLL section). See [formats/GAS.md](formats/GAS.md).

- **T2 sub-header class constants and surface class**: Bytes 4–16 are class constants (3 distinct values by grid size — confirmed). Remaining: decode their world-space meaning (requires Ghidra trace). Determine surface class byte → PIC atlas tile mapping. Confirm tile-summary record 0 algorithm (not NW corner, not dominant type — requires Ghidra). See [formats/T2.md](formats/T2.md).

---

## Win32 PE DLL Disassembly

For each item: load the overlay DLL in Ghidra, import the FA.SMS symbol list via `scripts/ghidra/import_sms.py`, trace from the DLL's exported entry point.

- **FNT glyph encoding**: Bytes `{03, F9, 88, 07}` are confirmed code values (not raw pixels); nibble-packed rows, RLE, or advance-width encoding still unresolved. Trace the glyph-drawing routine in FA.EXE. Also resolve count discrepancy (15 files inventoried vs 13 in LIB). See [formats/FNT.md](formats/FNT.md).

- **HUD gauge offset mapping**: Diff A7.HUD vs F22.HUD offset tables byte-by-byte to map each `(dx, dy)` pair to a specific gauge. Confirm anchor point encoding (two u16s at fixed VA, or derived from PT file). Identify `_l`/`_lh`/`_ls` gauge state variant semantics. See [formats/HUD.md](formats/HUD.md).

- **HUD advisory icon bit names**: Name the individual advisory icons — which bits of `DAT_0050cfef` correspond to GEAR, LOW FUEL, MASTER CAUTION, etc. Also identify callers of `FUN_00407930` outside the main HUD render loop to confirm which subsystems set each advisory-enable bit. See [formats/HUD.md](formats/HUD.md).

- **MUS opcode semantics**: `FA`/`FB`/`FC` are Miles Sound System XMIDI extensions. `FE`/`FD` confirmed via Ghidra to have **no FA.EXE callbacks** — `_AIL_register_*_callback` imports absent; branch conditions are Miles-internal. Full sub-opcode decode requires MSS/AIL SDK headers. See [formats/MUS.md](formats/MUS.md).

- **DLG non-Action record sizes and header gap**: `_DrawAction` = 38 bytes (confirmed). Measure exact sizes for `_DrawEditBox`, `_DrawText`, `_DrawRocker`, `_DrawCampaignList`. Fill in the unknown fields at common header offsets +0x02..+0x09 (present in all record types). Decode `_ChoosePreload` bounding-box params via `FUN_004a6e20`. See [formats/DLG.md](formats/DLG.md).

- **LAY gradient table and slot mapping**: Confirm `0x31` and `10 10` in gradient sub-block header (entry count vs. type ID; stride/channel meaning). Map `layer <name>.LAY <index>` slot indices from `.MM` files to rendering layers. Document CLOUD/DAY prefix naming convention. Map parameter fields at header offsets 0x00–0x0F and gap at 0x1C–0x28. See [formats/LAY.md](formats/LAY.md).

- **HGR hangar layout**: Identify the second `.HGR` filename (likely a carrier or alternate airbase). Disassemble to extract the hangar layout table — aircraft slot positions, icon placement, camera angle. See [formats/HGR.md](formats/HGR.md).

- **OT/NT `ot_flags` bit semantics**: Full survey complete; per-bit labels documented in OT.md and NT.md. Ghidra confirmation still needed for bits 5, 8, 9, 10, 11, 15, 18, 19, 20, 22, 25, 26 — current labels inferred from category patterns. See [formats/OT.md](formats/OT.md) and [formats/NT.md](formats/NT.md).

- **NT hardpoint bit 1 (`$2`) meaning**: "Surface-strike missile" hypothesis ruled out by BRF survey — IOWA `$a` HPs carry PHALANX/SEA_SPAR; KIROV `$a` carries AAA30 guns; SSN9 uses `$8`. Trace ship fire-control dispatcher (`_GVProc` or internal dispatch) in Ghidra. See [formats/NT.md](formats/NT.md).

- **GAS capacity word unit**: `word` values (108/198/248/315) have no linear or volumetric relationship to gallons or lbs. Search FA.SMS for fuel-system symbols (e.g. `GAS`, `fuel`, `tank`), trace the routine that reads the `word` field and adds it to the aircraft fuel pool. See [formats/GAS.md](formats/GAS.md).

---

## Mission System Formats

These formats (AI scripts, campaign state, mission conditions, theater maps) interact at runtime. Most are text-based but some have binary sections or reference binary resources.

- **AI script semantics**: Locate the script parser/interpreter in FA.EXE (xref to `.AI` filename loading). Confirm full `move`/`jink`/`patrol` argument semantics (heading reference points, speed modes). Confirm relationship between `.AI` script and companion `.BI` overlay. See [formats/AI.md](formats/AI.md).

- **CAM binary layout**: Disassemble `UKRAINE.CAM` to confirm the binary layout of the mission state and weapon tables (offsets, sizes, field encoding). Identify which `.MC` files correspond to which campaigns/missions. Determine how `.CAM` references theater `.MM` files (if at all). See [formats/CAM.md](formats/CAM.md).

- **MC condition check logic**: Disassemble `UKR01.MC` to trace the complete condition check logic and identify all object aliases it monitors. Determine how the `.CAM` file loads `.MC` files at mission start. Clarify `FOO.MC` and `EXTRA01.MC` (developer test missions or multiplayer extras?). See [formats/MC.md](formats/MC.md).

- **MM world-space fields**: Confirm `sides` entry count semantics (version/type vs. faction count). Determine world-space coordinate scale and origin for `pos`/`view` values. Document all `flags` bit assignments for `obj` blocks. Clarify `tmap_named` second and third argument semantics (position vs. tile_id/variant). Survey all `w_goal` values to enumerate waypoint goal types. Confirm `tdic id=256` meaning (tile type index into T2?). See [formats/MM.md](formats/MM.md).

---

## Format Deep Dives (BRF Numeric Fields)

- **SEE dual-lobe switch trigger**: Lobe-check internals confirmed — `FUN_004c2eb0` (search) manages a 40-tick acquisition timer at `target+0x11a`; `FUN_004c31f0` (track) additionally requires `target+0xde & 0x100000`. Neither function writes the transition bit. Remaining: identify the missile-service function that advances `missile+0xa6` from `0x10000` (search) to `0x20000` (track) and sets `target+0xde & 0x100000`. See [formats/SEE.md](formats/SEE.md).

- **JT agility and hit-probability bytes**: Hit-probability bytes at PROJ_TYPE+0x79–0x81 confirmed via `_PROJHitChance@28`; proximity fuze range at PROJ_TYPE+0x4F confirmed via `FUN_004c3960`. Remaining: map PROJ_TYPE+0x50–0x78 (turn rate, g-limit, physics params) via `_PROJProc` callback. See [formats/JT.md](formats/JT.md).

- **JT warhead flags bits 0–8**: Bits 16–17 (AA/AG), bits 4/9/10/21 confirmed from `_PROJLock@24` and `_PROJHitChance@28`. Remaining: bits 0–8 (fuze type, damage model — pattern: AIM-9M `0x4f`, MK-82 `0x12`, 20mm `0xc4`). See [formats/JT.md](formats/JT.md).

- **ECM band-bit map**: +0x0A = chaff, +0x0E = flare, +0x12 = radar Pk, +0x17 = IR Pk, `$1f0` bitmask (bit 4=radar, bit 8=IR) — all confirmed. Remaining: cross-reference five fixed constants (35, 95, 24, 159, 31) against known RWR band frequencies to confirm band-bit map for bits 5–7. See [formats/ECM.md](formats/ECM.md).

---

## Undocumented Loose Files

- **EA.CFG**: Map all fields by toggling settings in-game and diffing. Cross-reference `CN_ReadConfig` symbol in FA.SMS. See [formats/CFG.md](formats/CFG.md).

- **NET.DAT**: Map multiplayer network config fields. Cross-reference `CN_INFO` struct via FA.SMS. Confirm whether NET.DAT holds one transport block or a union of all transport configs. See [formats/NET.md](formats/NET.md).

---

## Developer and Meta

- **SMS version cross-reference**: Cross-reference selected VAs in FA.SMS against the live FA.EXE to confirm the symbol map matches the shipped binary. Identify build configuration (debug vs. release indicators in mangled names). See [formats/SMS.md](formats/SMS.md).

- **SMS IDA Pro import script**: Add `scripts/ida/import_sms.py` mirroring the existing Ghidra import script. See [formats/SMS.md](formats/SMS.md).
