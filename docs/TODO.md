# Research Backlog

Outstanding RE and documentation tasks, grouped by effort.

---

## Binary Analysis (no disassembly tool required)

- **PLT field gap (0xB0–campaign block start)**: Field layout from offset `0xB0` to the campaign block start is unmapped. Method: diff two pilot saves with known differences (aircraft, loadout) byte-by-byte. See [formats/PLT.md](formats/PLT.md). *(Requires gameplay — 4-pass methodology documented in PLT.md)*

- **GAS capacity word**: The `word` field (108/198/248/315) does not map linearly to US gallons. The `dword` mass is confirmed as fuel weight in lbs (6.6× gallon count). The capacity `word` requires FA.EXE fuel-system disassembly to decode. See [formats/GAS.md](formats/GAS.md).

- **FA_3.LIB PIC naming pattern**: Confirm whether the `<AC>_<N>.PIC` N suffix encodes LOD level, paint scheme, or texture region. Requires disc 2 (FA_3.LIB not on disc 1 or the hard drive install). Method: extract a full aircraft skin set, load each into GUI PIC viewer, compare against SH UV coordinates. See [formats/PIC.md](formats/PIC.md).

---

## Win32 PE DLL Disassembly

For each item: load the overlay DLL in Ghidra, import the FA.SMS symbol list via `scripts/ghidra/import_sms.py`, trace from the DLL's exported entry point.

- **FNT glyph encoding**: Bytes `{03, F9, 88, 07}` are confirmed code values (not raw pixels); nibble-packed rows, RLE, or advance-width encoding still unresolved. Trace the glyph-drawing routine in FA.EXE. Also resolve count discrepancy (15 files inventoried vs 13 in LIB). See [formats/FNT.md](formats/FNT.md).

- **HUD gauge offset mapping**: Diff A7.HUD vs F22.HUD offset tables byte-by-byte to map each `(dx, dy)` pair to a specific gauge. Confirm anchor point encoding (two u16s at fixed VA, or derived). Identify `_l`/`_lh`/`_ls` gauge state variant semantics. See [formats/HUD.md](formats/HUD.md).

- **MUS opcode semantics**: Decode `FA` sub-opcode meanings (`0x19`, `0x21`, `0x32`, `0x50` confirmed, semantics unknown). Confirm `FB` 3-byte vs 4-byte form rule (does mode byte determine F9 terminator presence?). Decode `FE`/`FD` branch condition argument (game-state enum — cross-reference FA.SMS). See [formats/MUS.md](formats/MUS.md).

- **DLG non-Action record sizes**: `_DrawAction` = 38 bytes (confirmed). Measure exact sizes for `_DrawEditBox` (shorter — `w=9` reads as characters not pixels), `_DrawText` (`str_va` at offset 0, remaining layout unknown), `_DrawRocker`, `_DrawCampaignList`. Decode `_ChoosePreload` bounding-box params. See [formats/DLG.md](formats/DLG.md).

- **LAY gradient table**: Decode header bytes `31 00 00 00 00 00 10 10` at VA 0x10B0 (confirm `0x31` as entry count, `10 10` as stride/channels). Identify all sub-block types in the pointer table at 0x1000. Explain DAY1.LAY size difference (20992 vs 16896 bytes) and CLOUD1B = CLOUD1 (byte-for-byte identical). See [formats/LAY.md](formats/LAY.md).

---

## Format Deep Dives (BRF Numeric Fields)

- **SEE dual-lobe switch trigger**: Primary=search, secondary=track confirmed by range/angle comparison; the engine condition that switches lobes needs Ghidra. Also confirm sentinel values `$80000000`/`$7fffffff` (heading-error limits vs no-limit flags). See [formats/SEE.md](formats/SEE.md).

- **JT agility and hit-probability bytes**: Map the byte sequence after seeker lobe data — turn rate, g-limit, fuze delay, Pk values. See [formats/JT.md](formats/JT.md).

- **JT warhead flags bits 0–15**: Bits 16–17 (AA/AG capability) confirmed; lower bits control fuze/warhead type. Needs Ghidra weapon evaluation function. See [formats/JT.md](formats/JT.md).

- **ECM effectiveness byte roles**: Variable bytes at positions 1, 5, 9 identified; whether they map to radar jamming / chaff / flare effectiveness needs Ghidra. Also confirm whether `$1f0` is a bitmask (five frequency bands) or an enumerated power level. See [formats/ECM.md](formats/ECM.md).

---

## Undocumented Loose Files

- **EA.CFG**: Map all fields by toggling settings in-game and diffing. Cross-reference `CN_ReadConfig` symbol in FA.SMS. See [formats/CFG.md](formats/CFG.md).

- **NET.DAT**: Map multiplayer network config fields. Cross-reference `CN_INFO` struct via FA.SMS. See [formats/NET.md](formats/NET.md).

- **RGN**: Decode `POSTER.RGN` (324 bytes) and `BUTTONS.RGN` binary region record structures. Confirm count word at offset 0, parse all 32 region records, explain size difference between the two files. See [formats/RGN.md](formats/RGN.md).

---

## Future Inventory

- **FA_3.LIB PIC naming pattern**: *(See Binary Analysis above — blocked on disc 2)*

- **OT/NT `ot_flags` bit semantics (bits 5, 8, 10, 11, 22+)**: Bit patterns catalogued from full OT/NT survey (see OT.md and NT.md); specific bit meanings need Ghidra confirmation of the damage/targeting/collision evaluation functions.
