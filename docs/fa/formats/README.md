# File Format Reference

All formats used by Jane's Fighters Anthology, organized by subsystem.

## Archive

The container format that holds all game assets; almost every file in the game is packed into one of several `.LIB` archives.

| Format | Spec | Description |
|--------|------|-------------|
| LIB | [LIB.md](LIB.md) | Main asset archive container with LZSS, PXPK, and DCL compression |

## Graphics & Images

Paletted image formats used for aircraft skins, cockpit art, icons, and in-game screenshots.

| Format | Spec | Description |
|--------|------|-------------|
| PIC | [PIC.md](PIC.md) | Compressed image codec — dense, sparse, and JPEG sub-formats |
| PAL | [PAL.md](PAL.md) | VGA 256-color 6-bit palette |
| RAW | [RAW.md](RAW.md) | Uncompressed in-game screenshot capture |

## Terrain & Maps

Theater-level tile maps and the top-down mini-map used during mission selection and in-flight navigation.

| Format | Spec | Description |
|--------|------|-------------|
| T2 | [T2.md](T2.md) | Terrain tile map with height and texture data |
| MM | [MM.md](MM.md) | Theater map and mini-map layout |

## 3D & Scene

Aircraft and object geometry, briefing room backgrounds, and the atmosphere/sky lookup tables loaded as Win32 overlay DLLs.

| Format | Spec | Description |
|--------|------|-------------|
| SH | [SH.md](SH.md) | 3D aircraft and object shape meshes (Phar Lap PE) |
| INF | [INF.md](INF.md) | Aircraft technical info sheet with briefing room scene data |
| HGR | [HGR.md](HGR.md) | Hangar 3D background scene (Win32 DLL overlay) |
| LAY | [LAY.md](LAY.md) | Sky and atmosphere rendering lookup tables (Win32 DLL overlay) |

## Audio

Raw PCM sound effects and MIDI-based music, plus the in-flight music sequencer bytecode.

| Format | Spec | Description |
|--------|------|-------------|
| 11K / 5K / 8K | [11K.md](11K.md) | Raw PCM audio clips at 11 kHz, 5 kHz, or 8 kHz mono |
| XMI | [XMI.md](XMI.md) | Extended MIDI music sequences |
| MUS | [MUS.md](MUS.md) | In-flight music sequencer bytecode (Win32 DLL overlay) |

## Video & Cutscenes

Full-motion video frames for intros and per-aircraft clips, mission briefing video streams, and the scripted cutscene timeline format.

| Format | Spec | Description |
|--------|------|-------------|
| CB8 | [CB8.md](CB8.md) | FMV video frame decoder for intros, cutscenes, and per-aircraft clips |
| VDO | [VDO.md](VDO.md) | Streaming mission briefing video frames |
| FBC | [FBC.md](FBC.md) | Per-frame byte-size index for paired .VDO briefing files |
| SEQ | [SEQ.md](SEQ.md) | Cutscene and animation event timeline |

## Mission & Campaign

Mission definitions, briefing text, campaign state, AI scripts, and their compiled runtime companions — most loaded as Win32 PE DLLs.

| Format | Spec | Description |
|--------|------|-------------|
| M | [M.md](M.md) | Mission definition — bracketed text format with object placement and waypoints |
| MT | [MT.md](MT.md) | Mission briefing and debrief text with section/markup directives |
| CAM | [CAM.md](CAM.md) | Campaign PE DLL embedding mission lists, weapon tables, and state data |
| MC | [MC.md](MC.md) | Per-mission condition evaluator (PE DLL) |
| AI | [AI.md](AI.md) | AI script bytecode — plain-text DSL per object category |
| BI | [BI.md](BI.md) | Compiled AI binary runtime companion to .AI script files |

## Type Definitions (BRF DSL)

Seven file types share a plain-text assembly-like DSL that defines aircraft, weapons, objects, and sensors. The BRF spec below is the format overview.

| Format | Spec | Description |
|--------|------|-------------|
| BRF | [BRF.md](BRF.md) | Overview of the text-based type definition DSL |
| OT | [OT.md](OT.md) | Object type definitions |
| NT | [NT.md](NT.md) | Nation and country type definitions |
| PT | [PT.md](PT.md) | Pilot and aircraft performance type definitions |
| JT | [JT.md](JT.md) | Jet engine and weapon type definitions |
| SEE | [SEE.md](SEE.md) | Sensor and electronics type definitions |
| ECM | [ECM.md](ECM.md) | Electronic countermeasures type definitions |
| GAS | [GAS.md](GAS.md) | Guided armament system type definitions |

## UI & Win32 Overlays

The FA menu system is built from Win32 PE DLLs; each dialog, menu screen, font, and HUD element is a separate overlay loaded at runtime.

| Format | Spec | Description |
|--------|------|-------------|
| HUD | [HUD.md](HUD.md) | Heads-up display overlay DLL |
| DLG | [DLG.md](DLG.md) | Dialog box overlay DLL |
| MNU | [MNU.md](MNU.md) | Menu screen overlay DLL |
| FNT | [FNT.md](FNT.md) | Bitmap font overlay DLL |
| PTS | [PTS.md](PTS.md) | Points and scoring overlay DLL |

## System & Config

Game configuration, multiplayer network settings, the recovered C++ symbol map, and pilot save files.

| Format | Spec | Description |
|--------|------|-------------|
| CFG | [CFG.md](CFG.md) | Binary game configuration — graphics, controls, audio, pilot slot |
| DAT | [DAT.md](DAT.md) | Binary multiplayer network settings (NET.DAT / MODEM.DAT / SERIAL.DAT) |
| SMS | [SMS.md](SMS.md) | the game executable symbol map — 3,829 MSVC-mangled C++ symbols with virtual addresses |
| P | [P.md](P.md) | Pilot save file — career stats, callsign, and campaign progress |
| BIN | [BIN.md](BIN.md) | Lookup tables and palette subsets (INSIGMAP.BIN, VFONTPAL.BIN) |

## Installer

Files used by the EA disc-based setup program — the install script and splash screen click-zone maps.

| Format | Spec | Description |
|--------|------|-------------|
| SSF | [SSF.md](SSF.md) | EA installer script — plain-text keywords driving the setup UI |
| RGN | [RGN.md](RGN.md) | Installer UI region maps — splash screen click zones and button sprite atlas |

## Text

Plain-text files embedded in the LIB archives for menu labels and UI screens, sharing the same directive engine as mission briefing files.

| Format | Spec | Description |
|--------|------|-------------|
| TXT | [TXT.md](TXT.md) | Plain text files used in FA menus and UI screens; shares the .MT directive engine |
