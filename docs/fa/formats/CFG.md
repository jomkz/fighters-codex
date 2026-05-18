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

## Differential Mapping

EA.CFG is 347 bytes — small enough to fully map by toggling one setting at a time.

### Step-by-step approach

1. **Baseline**: copy `EA.CFG` before launching FA (call it `cfg_base.bin`).
2. **One change per session**: change a single option (e.g. set joystick type from "keyboard" to "joystick 1"), exit to desktop without changing anything else.
3. **Diff**: `HxD → Compare → cfg_base.bin` vs the updated `EA.CFG`. The changed bytes are that option's field.
4. Repeat for each setting category below.

### Settings to map (in order of isolation difficulty)

| Setting | Where to change | Expected encoding |
|---------|-----------------|-------------------|
| Last pilot slot | Main menu → select a different pilot | 1 byte index or short string offset |
| Sound device | Audio options → change MIDI device | 1 byte enum |
| Music/FX volume | Audio options → sliders | 1–2 bytes, 0–100 scale |
| Joystick type | Controls → joystick selection | 1 byte enum (0 = keyboard, 1–N = joystick n) |
| Screen detail level | Graphics → detail slider | 1 byte |
| Gamma / brightness | Graphics → brightness | 1 byte |
| Screen resolution | Graphics → resolution (if configurable) | word or 2 bytes (width + height index) |

### Ghidra cross-reference

`CN_ReadConfig` is confirmed in FA.SMS. Load FA.EXE with `ImportFASms` labels, locate `CN_ReadConfig`, and decompile it. The function reads `EA.CFG` into a struct — the field offsets and types will be directly readable from the decompiled code, bypassing the need for differential passes for harder-to-toggle options (e.g. network-related settings stored in EA.CFG).

## TODO

- Complete differential mapping pass for all settings categories above
- Cross-reference with `CN_ReadConfig` in Ghidra to confirm field layout and find any non-obvious fields

## Related

- [PLT.md](PLT.md) — pilot save files (`PLT_*.PLT`) whose slot is referenced here
