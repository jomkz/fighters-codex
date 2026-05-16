# Documentation

This documentation is an attempt to preserve the knowledge built up from the great community of people who put many hours into making the game more realistic or just enjoyable to play. This is a work in progress and will be updated as more information is discovered. If something here is wrong or needs more detail, please sumbit an issue.  

## Contents

| Document | Description |
|----------|-------------|
| [cli.md](cli.md) | Full CLI command reference |
| [modding.md](modding.md) | Step-by-step modding recipes |
| [api.md](api.md) | C++ library API reference |

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
| CB8 (DRBC/VooM/MRFA/MRFI) | [formats/CB8.md](formats/CB8.md) | FMV video (intros, cutscenes, per-aircraft clips) |
| VDO (RATVID) | [formats/VDO.md](formats/VDO.md) | Mission briefing video frames |
| FBC | [formats/FBC.md](formats/FBC.md) | Per-frame byte-size index for paired .VDO files |
| INF | [formats/INF.md](formats/INF.md) | Aircraft technical info sheet (RTF) |
| RAW | [formats/RAW.md](formats/RAW.md) | In-game screenshot capture (structure under investigation) |
| PLT | [formats/PLT.md](formats/PLT.md) | Pilot save file — career stats, callsign, history (structure under investigation) |

## Known .LIB Files (Fighters Anthology)

See [formats/EALIB.md](formats/EALIB.md#known-lib-files-fighters-anthology).

## Out of Scope

| Format | Notes |
|--------|-------|
| `.HUD`, `.DLG`, `.LAY`, `.MNU` | PE wrapper files; require PE disassembly, low modding value |
| `.MT` | Mission text / briefing companion to `.M` files. Plain ASCII with section markers (`.section 1`–`4`) and inline markup directives (`.center`, `.underline`, `.header`, `.left`, `.body`). Sections: 1=title block, 2=mission briefing, 3=debrief success, 4=debrief failure. No binary parsing needed; editable as-is. |
| `.XMI` | Standard Extended MIDI; use external tools for playback |
| SH pack (OBJ→SH) | Too complex given animation/LOD/damage states |
| `.PTS` | Phar Lap PE binary identical to `.SH`; used for aircraft shadow/crash shapes by some mods (name pattern: `NAME_S.SH` in-LIB, renamed to `.PTS` when distributed standalone). Parse with the same SH parser. |
| `.MUS` | Phar Lap PE binary (4608 bytes); acts as a pointer to a `.11K` file. The engine reads a 4-char name from a fixed offset and appends `001.11k` to get the audio filename (e.g. slot `M_air.MUS` → `dogf001.11k`). Nine slots: `M_air`, `M_danger`, `M_deck`, `M_eject`, `M_home`, `M_launch`, `M_normal`, `M_succ`, `M_valk`. Replace the referenced `.11K` to swap in-game music. |
| LZSS (flags=1) | Rare EALIB compression variant; not yet implemented in ft_lib |
