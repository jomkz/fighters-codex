# Documentation

The format specifications here were made possible by the
**[OpenFA project](https://gitlab.com/openfa/openfa)**, a GPLv3 open-source
reverse-engineering effort in Rust. Our role was to re-implement the formats in C++17,
validate them against the full game dataset, and document the findings.

---

## Contents

| Document | Description |
|----------|-------------|
| [cli.md](cli.md) | Full CLI command reference |
| [modding.md](modding.md) | Step-by-step modding recipes |
| [api.md](api.md) | C++ library API reference |

---

## File Format Reference

| Format | Spec | Description |
|--------|------|-------------|
| EALIB + PKWare DCL | [formats/EALIB.md](formats/EALIB.md) | Archive container and compression algorithm |
| PIC | [formats/PIC.md](formats/PIC.md) | Palettized image (dense, sparse, JPEG) |
| PAL | [formats/PAL.md](formats/PAL.md) | VGA 6-bit palette |
| SEQ | [formats/SEQ.md](formats/SEQ.md) | Cutscene / animation event timeline |
| Audio (.11K / .5K) | [formats/AUDIO.md](formats/AUDIO.md) | Raw PCM audio |
| BRF (OT/NT/PT/JT/SEE/ECM/GAS) | [formats/BRF.md](formats/BRF.md) | Type definitions (aircraft, weapons, objects) |
| M / MM | [formats/MISSION.md](formats/MISSION.md) | Mission and map files |
| SH | [formats/SH.md](formats/SH.md) | 3D shape / model (Phar Lap PE bytecode) |

---

## Known .LIB Files (Fighters Anthology)

| File | Location | Key Contents |
|------|----------|--------------|
| FA_1.LIB | Install dir | Fonts (.FNT), UI graphics (.PIC) |
| FA_2.LIB | Install dir | Shapes, missions, audio, type defs, cutscenes, text, palette — 5405 files |
| FA_3.LIB | Disk 2 (Red) | 822 JPEG-format .PIC textures, 269 .INF aircraft data files |
| FA_4B.LIB | Install dir | Additional assets |
| FA_4C.LIB | Disk 1 (Blue) | — |
| FA_4D.LIB | Install dir | — |
| FA_7.LIB | Disk 1 (Blue) | — |
| FA_10.LIB | Disk 2 (Red) | — |
| FA_10B.LIB | Disk 2 (Red) | — |
| FA_11.LIB | Disk 2 (Red) | — |
| FA_11B.LIB | Disk 2 (Red) | — |

---

## Verification Results

| Test | Result |
|------|--------|
| FA_2.LIB full unpack (5405 files) | 5405 OK, 0 failed |
| M/MM round-trip (592 files) | 592/592 byte-exact |
| SH geometry extraction (1275 files) | 1210/1275 with geometry; 65 x86-only effects |
| `F16C_0.PIC` decoded | 512×384 PNG OK |
| `BALTIC.TXT` decompressed | 147 bytes OK |

---

## Out of Scope

| Format | Notes |
|--------|-------|
| `.HUD`, `.DLG`, `.LAY`, `.MNU` | PE wrapper files; require PE disassembly, low modding value |
| `.MT` | Undocumented; needs hex analysis |
| `.XMI` | Standard Extended MIDI; use external tools for playback |
| SH pack (OBJ→SH) | Too complex given animation/LOD/damage states |
| LZSS (flags=1) | Rare; not yet implemented in ft_lib |
