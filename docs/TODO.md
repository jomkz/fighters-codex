# Research Backlog

Outstanding RE and documentation tasks, grouped by effort.

---

## Binary Analysis (no disassembly tool required)

- **PLT field gap (0xB0–campaign block start)**: Field layout of the pilot save file from offset `0xB0` to the campaign block start is unmapped. Method: diff two pilot saves with known differences (different aircraft, different loadout) byte-by-byte. See [formats/PLT.md](formats/PLT.md).

---

## Win32 PE DLL Disassembly

For each item: load the overlay DLL in Ghidra, import the FA.SMS symbol list to auto-name engine imports, trace from the DLL's exported entry point.

- **FNT glyph format**: Pixel format (1/4/8-bpp?), grid stride, character metrics, baseline offset. See [formats/FNT.md](formats/FNT.md).
- **HUD element layout**: Screen-space coordinate table, gauge types, indicator IDs. See [formats/HUD.md](formats/HUD.md).
- **MUS playlist logic**: Which XMI tracks it sequences, trigger conditions (game state machine), transition logic. See [formats/MUS.md](formats/MUS.md).
- **DLG control positions**: Per-dialog control coordinate table, control type encoding. See [formats/DLG.md](formats/DLG.md).
- **LAY rendering algorithm**: Sky/cloud/ocean draw calls, atmosphere parameters, horizon math. See [formats/LAY.md](formats/LAY.md).

---

## Format Deep Dives (BRF numeric fields)

These fields are documented structurally but their numeric semantics need calibration.

- **SEE range encoding**: Calibrate `^XXXXXX` fixed-point range unit against known radar/weapon ranges. See [formats/SEE.md](formats/SEE.md).
- **SEE seeker type byte**: Confirm full enum (0=visual, 1=IR, 3=radar confirmed; what is 2?). See [formats/SEE.md](formats/SEE.md).
- **SEE dual-lobe meaning**: Does primary/secondary lobe map to search vs. track mode, or two radar bands? See [formats/SEE.md](formats/SEE.md).
- **JT PROJ_TYPE fields**: Decode warhead capability flags dword, agility byte sequence, hit-probability bytes. See [formats/JT.md](formats/JT.md).
- **ECM effectiveness bytes**: Decode the three groups of three bytes (RWR bands? jamming categories? decoy effectiveness?). See [formats/ECM.md](formats/ECM.md).
- **GAS capacity word**: Decode internal unit (not US gallons directly). See [formats/GAS.md](formats/GAS.md).
- **OT/NT capability flags**: Decode `ot_flags` dword and `obj_class` word fully. See [formats/OT.md](formats/OT.md), [formats/NT.md](formats/NT.md).

---

## Undocumented Loose Files

- **EA.CFG**: Map all fields by toggling settings in-game and diffing. Cross-reference `CN_ReadConfig` symbol in FA.SMS. See [formats/CFG.md](formats/CFG.md).
- **NET.DAT**: Map multiplayer network config fields. Cross-reference `CN_INFO` struct via FA.SMS. See [formats/NET.md](formats/NET.md).
- **RGN**: Decode `POSTER.RGN` (324 bytes) binary region record structure. See [formats/RGN.md](formats/RGN.md).

---

## Future Inventory

- **FA_3.LIB PIC naming pattern**: Confirm whether the `<AC>_<N>.PIC` N suffix encodes LOD level, paint scheme, or texture region. Extract a sample aircraft's full texture set and cross-reference against SH file UV mapping.
- **FA.EXE SMS import script**: Write a Ghidra or IDA script that reads FA.SMS and bulk-imports all 3,829 symbols as labels. See [formats/SMS.md](formats/SMS.md).
- **`~` prefix files**: Confirm semantics of `~`-prefixed entries in FA_2.LIB (e.g. `~BNK5.OT`, `~MOOSE.JT`). Are these campaign/theater overrides or runtime aliases?
