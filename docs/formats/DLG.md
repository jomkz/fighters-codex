# In-Game Menu Dialog Layout (.DLG)

FA_2.LIB contains 92 `.DLG` files. Each defines one dialog box in the FA menu system. All are **Win32 PE DLLs** (MZ stub + PE32 image) loaded at runtime; they import rendering functions from `main.dll` and embed their label strings in the PE data section.

## Format

Win32 PE DLL. All DLG files import from `main.dll`. The set of imported drawing functions varies per dialog and reveals the control types used:

| Import | Control rendered |
|--------|-----------------|
| `_DrawAction` | Clickable button |
| `_DrawRocker` | Toggle / rocker selector |
| `_DrawEditBox` | Editable text input field |
| `_DrawText` | Static text label |
| `_DrawFormattedText` | Multi-line formatted text block |
| `_DrawCampaignList` | Campaign list box |
| `_cancelString` | Localized "Cancel" button label |
| `_okString` | Localized "OK" button label |

The engine associates dialogs with their parent MNU file; the DLG is loaded when the corresponding menu item is selected.

## Complete Filename → Screen Mapping

Derived from embedded label strings in the `.DLG` PE data sections.

### Mission setup

| File | Screen |
|------|--------|
| CHOOSEAC.DLG | Main start screen — Play Single Mission / Create Quick Mission / Create Pro Mission / Replay Last Mission / Start New Campaign |
| BRIEFSCR.DLG | Mission briefing paper screen |
| SNGLMISS.DLG | Single mission list picker |
| QUIKMISS.DLG | Quick mission dialog |
| LOADORD.DLG | Arm plane / ordnance loadout — Select Plane + weapons dial |
| ACFTSLEC.DLG | Aircraft selection sub-screen (Arm Plane / Brief Map tabs) |
| ACFTRPAR.DLG | Aircraft parameters dialog |

### Quick Battle wizard (23 dialogs — one per wizard step)

| File | Prompt |
|------|--------|
| QUICKB3.DLG | Choose nationality of friendly forces |
| QUICKB4.DLG | Select number of friendly pilots |
| QUICKB5.DLG | Choose skill of friendly forces |
| QUICKB6.DLG | Choose type of plane for friendly forces |
| QUICKB7.DLG | Choose altitude of friendly forces |
| QUICKB8.DLG | Choose map to fly over |
| QUICKB9.DLG | Choose time of day |
| QUICKB10.DLG | Choose weather conditions |
| QUICKB11.DLG | Choose advantage level over enemy |
| QUICKB12.DLG | Choose weapons (guns only / missiles+guns) |
| QUICKB13.DLG | Choose nationality of enemy forces |
| QUICKB14.DLG | Select number of enemy pilots — flight 1 |
| QUICKB15.DLG | Choose skill of enemy — flight 1 |
| QUICKB16.DLG | Choose plane type — enemy flight 1 |
| QUICKB17.DLG | Select number of enemy pilots — flight 2 |
| QUICKB18.DLG | Choose skill of enemy — flight 2 |
| QUICKB19.DLG | Choose plane type — enemy flight 2 |
| QUICKB20.DLG | Select number of enemy pilots — flight 3 |
| QUICKB21.DLG | Choose skill of enemy — flight 3 |
| QUICKB22.DLG | Choose plane type — enemy flight 3 |
| QUICKB23.DLG | Choose ground target |
| QUICKB24.DLG | Choose AAA defense strength |
| QUICKB25.DLG | Choose SAM defense strength |
| QUICK14.DLG | Quick mission theater / map selection list |

### Campaign and pilot

| File | Screen |
|------|--------|
| CAMPAIGN.DLG | Campaign list picker |
| SHWPILOT.DLG | Pilot roster screen — New Pilot / Delete / Copy Pilot / Select |
| CONTPLT.DLG | Continue with existing pilot — Delete / Copy Pilot / Select |
| VIEWPLT.DLG | View pilot record — Delete / Copy Pilot |
| AR_DLG.DLG | After-action report — General / Details / Videos / Photo Album / Parts List tabs |
| CALLSIGN.DLG | Choose callsign from list or enter custom |
| EDITNAME.DLG | Enter pilot name |
| EDITSIGN.DLG | Enter callsign |
| EDITSND.DLG | Enter callsign sound file (.5K or .11K) |
| EDITSQAD.DLG | Enter squadron name |

### Mission Creator (MC) dialogs

| File | Prompt |
|------|--------|
| MC_DLG.DLG | Mission Creator main options panel |
| MC_SCR.DLG | Set which screens player can access |
| MC_WETH.DLG | Set weather conditions |
| MC_TIME.DLG | Set time limit |
| MC_KILLS.DLG | Set kill count to end scenario |
| MC_KILLT.DLG | Set how kills end scenario (total / by side / by player) |
| MC_LIVES.DLG | Set number of revives |
| MC_DELAY.DLG | Set time delay before revive |
| MC_DIST.DLG | Set distance away after revive |
| MC_NAT.DLG | Assign nationalities to enemy side |
| MC_NAT2.DLG | Choose nationality of individual object |
| MC_NATF.DLG | Assign nationalities — full version (all sides) |
| MC_NAME.DLG | Enter pilot name (MC context) |
| PICKOBJ.DLG | Choose an object (mission editor object picker) |
| FORTAIRB.DLG | Multiplayer airbase — Deploy / Evacuate |
| FORTOPT.DLG | Multiplayer / fortification options |

### Preferences and configuration

| File | Screen |
|------|--------|
| GRAFPREF.DLG | Graphics preferences (640×480) |
| GRAF320.DLG | Graphics preferences (320×200) |
| SNDPREF.DLG | Sound preferences |
| SOUND320.DLG | Sound preferences (320×200) |
| AUDIOD.DLG | Audio device options |
| UCONFIGD.DLG | User configuration dialog |

### Multiplayer — network

| File | Screen |
|------|--------|
| NEWNET.DLG | New network session options |
| NETJOIN.DLG | Join network game — game list |
| NETNEW.DLG | Host new game — wait for players |
| NETDIR.DLG | Network directory — player name / address |
| NETIPX.DLG | IPX/SPX connection — answer / status |
| NETIPX2.DLG | IPX/SPX settings (default / custom) |
| NETTCP.DLG | TCP/IP settings |
| NETEDT.DLG | Network player entry edit |
| NETBEDT.DLG | NetBEUI / transport-B player edit |
| NETCEDT.DLG | Transport-C player edit |

### Multiplayer — modem / serial

| File | Screen |
|------|--------|
| MODEM.DLG | Modem connection — Answer / player name / phone |
| MODEMCOM.DLG | Modem AT command strings (init / dial / listen) |
| MODEMSTS.DLG | Modem connection status |
| MODLIST.DLG | Modem selection list |
| SERIAL.DLG | Serial / null-modem connection |
| COM.DLG | Communications dialog |
| COMLIST.DLG | Communications transport list |

### Generic system dialogs

| File | Screen |
|------|--------|
| INFO320.DLG | Generic info box — OK only (320×200) |
| INFO640.DLG | Generic info box — OK only (640×480) |
| INFO0320.DLG | Info variant 0 — OK only (320×200) |
| INFO0640.DLG | Info variant 0 — OK only (640×480) |
| INFO2320.DLG | Info variant 2 — OK + Cancel (320×200) |
| INFO2640.DLG | Info variant 2 — OK + Cancel (640×480) |
| INFO2642.DLG | Info variant 2 alternate — OK + Cancel (640×480) |
| INFOY320.DLG | Yes / No confirmation dialog (320×200) |
| MDIAG.DLG | Generic message dialog (references GrafPrefPreload) |
| CDIAG.DLG | Continue / Cancel dialog |
| DDIAG.DLG | Disconnect confirmation dialog |
| LISTTST.DLG | Developer test dialog (placeholder/lorem ipsum text) |

The `CHOOSEAC.DLG` labels are the top-level game start menu items — displayed before any campaign or mission is active.

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 92 |

## Dispatch Table Layout (Confirmed)

DLG files use a **Phar Lap PE format** (signature `PL\0\0` instead of `PE\0\0`). There is no compiled x86 code — the CODE section is a **dispatch table** of fixed-size records.

### Record structure (per-type, confirmed via Ghidra)

Each record begins with a `u32 thunk_va` that identifies the draw function. Remaining fields are type-specific — record sizes vary. All offsets below are from the start of the record (i.e. the same base as `thunk_va`).

Label strings are packed consecutively after the dispatch table in the same CODE section.

#### _DrawAction — 38 bytes

| Offset | Type | Field |
|--------|------|-------|
| +0x00 | u32 | `thunk_va` |
| +0x01 | u8 | `flags` — bit 7 = dim/dark colour variant |
| +0x02 | u8[8] | unknown |
| +0x0A | i16 | `x` |
| +0x0C | u16 | `y` |
| +0x0E | i16 | unknown |
| +0x12 | i16 | unknown |
| +0x14 | i16 | unknown |
| +0x16 | i16 | unknown |
| +0x17 | u8 | `action_type` — 1 = radio/checkbox, 3 = type-3, 4 = type-4, else = standard button |
| +0x18 | i16 | `width_px` — control width in pixels |
| +0x1A | u32 | `label_ptr` — ptr to label string or icon resource |
| +0x1C | i16 | unknown |
| +0x1E | u32 | `icon_ptr` |
| +0x22 | i16 | `text_x` — text x offset within button |
| +0x24 | i16 | `text_y` — text y offset within button |

The empirically derived x/y offsets (+4, +6) in earlier documentation were incorrect; Ghidra confirms x at +0x0A, y at +0x0C. The 38-byte total size is confirmed by both methods.

#### _DrawText — 22 bytes

| Offset | Type | Field |
|--------|------|-------|
| +0x00 | u32 | `thunk_va` |
| +0x01 | u8 | `flags` — bit 7 = dim variant |
| +0x02 | u8[8] | unknown |
| +0x0A | u32 | `text_ptr` — `char*` to label string |
| +0x0E | u32 | `font_ptr` — font override (`0` → default `PANELFNT`/`PANELFND` loaded from bit 7 of `flags`) |
| +0x12 | i16 | `x` |
| +0x14 | i16 | `y` |

#### _DrawFormattedText — 36 bytes

| Offset | Type | Field |
|--------|------|-------|
| +0x00 | u32 | `thunk_va` |
| +0x01 | u8[9] | unknown |
| +0x0A | i16 | `x` |
| +0x0C | i16 | `y` |
| +0x0E | i16 | `width` |
| +0x10 | i16 | `height` |
| +0x12 | i16 | unknown |
| +0x14 | i16 | unknown |
| +0x16 | i16 | `visible_rows` — items per page |
| +0x18 | i16 | `selection` — updated by engine at render time |
| +0x1A | i16 | `current_item` |
| +0x1C | i16 | `last_rendered` |
| +0x1E | i16 | unknown |
| +0x20 | u32 | `text_ptr` — `char**` string array |

#### _DrawCampaignList — 36 bytes

Same field layout as `_DrawFormattedText`. The render logic differs (rows are campaign entries with a highlight sprite from `CAMPHI.PIC`; row height is 0x4B px).

#### _DrawRocker — 40 bytes

| Offset | Type | Field |
|--------|------|-------|
| +0x00 | u32 | `thunk_va` |
| +0x01 | u8[9] | unknown |
| +0x0A | i16 | `x` |
| +0x0C | i16 | `y` |
| +0x0E | i16 | unknown |
| +0x10 | i16 | unknown |
| +0x12 | i16 | unknown |
| +0x14 | i16 | unknown |
| +0x16 | i16 | unknown |
| +0x18 | i16 | unknown |
| +0x1A | i16 | `up_value` — option index for up/left arrow |
| +0x1C | i16 | `down_value` — option index for down/right arrow |
| +0x1E | i16 | `current_value` |
| +0x20 | i16 | unknown |
| +0x22 | u32 | `parent_ref` — ptr to linked parent control for auto-positioning |
| +0x26 | u8 | `size_flag` — non-zero = tall/large rocker variant |
| +0x27 | u8 | pad |

#### _DrawEditBox — 24 bytes

| Offset | Type | Field |
|--------|------|-------|
| +0x00 | u32 | `thunk_va` |
| +0x01 | u8[9] | unknown |
| +0x0A | i16 | `char_count` — field width in characters |
| +0x0C | i16 | `y` |
| +0x0E | i16 | `x` |
| +0x10 | i16 | `pixel_width` — computed at render: `char_count × 10 + 16`; written back to record |
| +0x12 | i16 | `height` — always written as 24 (0x18) at render time |
| +0x14 | u32 | `text_buffer` — `char*` to editable text |

### JMP thunks and state dispatch table

At the end of the CODE section, each imported function has a 6-byte JMP thunk:

```
FF 25 [iat_va LE]    ; JMP DWORD PTR [IAT slot]
```

Immediately before the thunk block there is a fixed 9-byte **state machine dispatch table**:

```
01 02 03 02 01 02 03 02 01
```

This same sequence appears in MUS CODE sections (after the `FC` opcode), confirming it is a shared engine construct — not DLG-specific. The `thunk_va` field in each dispatch record points to one of the JMP thunks, identifying which draw function the record invokes.

### _cancelString and _okString (button label indirection)

Records whose `thunk_va` points to the `_cancelString` or `_okString` thunk do **not** embed a string directly. The `label_ptr` field holds the VA of the thunk itself, which the engine dereferences at runtime to call the localized label function.

### CHOOSEAC.DLG decoded (main start screen)

| VA | x | y | width | Label |
|----|---|---|-------|-------|
| 00001015 | 44 | 24 | 144 | Play Single Mission |
| 0000103B | 44 | 56 | 144 | Create Quick Mission |
| 00001061 | 44 | 88 | 144 | Create Pro Mission |
| 00001087 | 44 | 120 | 144 | Replay Last Mission |
| 000010AD | 44 | 251 | 144 | Start New Campaign |
| 000010D3 | 37 | 283 | 158 | Continue Old Campaign |
| 000010F9 | 44 | 315 | 144 | View Pilot Records |
| 0000111F | 32 | 174 | 170 | Reference |

Y gap between 120 and 251 (131 px) divides the menu into two groups; buttons 1–4 are mission-start options, 5–8 are management/info.

### `_ChoosePreload` header record

Every DLG begins with one `_ChoosePreload` record (4-byte thunk + params) that initialises assets and sets dialog state. In CHOOSEAC.DLG this record appears at VA 0x1000 with params `(379, 80, 238, 361)` — encoding not yet decoded (see TODO).

**`_ChoosePreload` (`FUN_004897f0`) confirmed behaviour** (Ghidra):
1. Calls `FUN_0040d5f0` — pushes current screen state (`DAT_0053824c`) onto dialog stack (`DAT_00522310[DAT_004ec31c++]`), sets state to `6`
2. Calls `FUN_00489840` (`__fastcall char param_1`) — loads action-button PIC and font assets keyed by `action_type`:
   - type 1: `ACTDFDxx.PIC` / `ACTDFNxx.PIC` (default, dim/normal); font `LMR`
   - type 3: `ACTI2Nxx.PIC` / `ACTI2Dxx.PIC`; fonts `fontact`/`fontacd`
   - type 4: `ACTI3Nxx.PIC` / `ACTI3Dxx.PIC`; same fonts
   - else:   `ACTIONxx.PIC` / `ACTIODxx.PIC`; same fonts
3. Decrements `DAT_004ec31c`, pops `DAT_0053824c` from the stack

`_ChoosePreload` is dispatched via computed indirect call — no direct CALL references (confirmed by Ghidra reference scan). The four params in the record are read by the DLG dispatcher through `FUN_004a6e20`; their exact bounding-box or dialog-type semantics require tracing that dispatcher.

## Toolkit Roadmap

- New `lib/src/dlg.cpp` + `lib/include/ft/dlg.h` — parse dispatch table from PE CODE section
- New `cli/cmd_dlg.cpp` — `ft dlg dump <file.DLG>` prints control table as JSON `[{func, x, y, width, label}]`
- GUI: `dlg_editor.h/cpp` — visual dialog layout editor that lets modders reposition controls

## TODO — Deep Dive

- Decode `_ChoosePreload` params — function body confirmed (see above); the four i16 param semantics require tracing `FUN_004a6e20` (the DLG record-params accessor) within the dispatcher
- Fill in the unknown fields at +0x02..+0x09 (common header gap) and the per-type unknowns above

## Related

- [MNU.md](MNU.md) — top-level menu files that surface DLG dialogs
