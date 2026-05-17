# Game Configuration (EA.CFG)

`EA.CFG` is a binary configuration file written by FA on first run and updated whenever the player changes settings in-game. It persists graphics, controls, audio, and pilot selection state between sessions.

## Location

Loose file in the FA install directory — not packed into any LIB archive.

## Observed Properties

| Property | Value |
|----------|-------|
| Filename | `EA.CFG` |
| Size     | 347 bytes |
| Format   | Binary (not plain text) |

## Known Content

String analysis reveals at minimum a pilot slot reference: `"Pilot 3"` — consistent with the game supporting multiple pilot saves (PLT files). The majority of the file is binary-encoded settings.

## TODO — Deep Dive

- Map all fields by comparing `EA.CFG` before and after toggling individual settings in-game
- Correlate with FA.EXE symbols: search FA.SMS for `CN_ReadConfig` and related config I/O functions to identify the struct layout
- Determine byte offset and encoding for: graphics quality, joystick type, audio mode (MIDI vs digital), screen resolution, last pilot slot used

## Related

- [PLT.md](PLT.md) — pilot save files (`PLT_*.PLT`) whose slot is referenced here
