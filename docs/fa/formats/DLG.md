# In-Game Menu Dialog Layout (.DLG)

FA_2.LIB contains 92 `.DLG` files. Each defines one dialog box in the FA menu system. All are **Win32 PE DLLs** (MZ stub + PE32 image) loaded at runtime; they import rendering functions from `main.dll` (= FA.EXE â€” see [architecture.md](../architecture.md#overlay-system--win32-pe-dlls)) and embed their label strings in the PE data section.

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

## Complete Filename â†’ Screen Mapping

Derived from embedded label strings in the `.DLG` PE data sections.

### Mission setup

| File | Screen |
|------|--------|
| CHOOSEAC.DLG | Main start screen â€” Play Single Mission / Create Quick Mission / Create Pro Mission / Replay Last Mission / Start New Campaign |
| BRIEFSCR.DLG | Mission briefing paper screen |
| SNGLMISS.DLG | Single mission list picker |
| QUIKMISS.DLG | Quick mission dialog |
| LOADORD.DLG | Arm plane / ordnance loadout â€” Select Plane + weapons dial |
| ACFTSLEC.DLG | Aircraft selection sub-screen (Arm Plane / Brief Map tabs) |
| ACFTRPAR.DLG | Aircraft parameters dialog |

### Quick Battle wizard (23 dialogs â€” one per wizard step)

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
| QUICKB14.DLG | Select number of enemy pilots â€” flight 1 |
| QUICKB15.DLG | Choose skill of enemy â€” flight 1 |
| QUICKB16.DLG | Choose plane type â€” enemy flight 1 |
| QUICKB17.DLG | Select number of enemy pilots â€” flight 2 |
| QUICKB18.DLG | Choose skill of enemy â€” flight 2 |
| QUICKB19.DLG | Choose plane type â€” enemy flight 2 |
| QUICKB20.DLG | Select number of enemy pilots â€” flight 3 |
| QUICKB21.DLG | Choose skill of enemy â€” flight 3 |
| QUICKB22.DLG | Choose plane type â€” enemy flight 3 |
| QUICKB23.DLG | Choose ground target |
| QUICKB24.DLG | Choose AAA defense strength |
| QUICKB25.DLG | Choose SAM defense strength |
| QUICK14.DLG | Quick mission theater / map selection list |

### Campaign and pilot

| File | Screen |
|------|--------|
| CAMPAIGN.DLG | Campaign list picker |
| SHWPILOT.DLG | Pilot roster screen â€” New Pilot / Delete / Copy Pilot / Select |
| CONTPLT.DLG | Continue with existing pilot â€” Delete / Copy Pilot / Select |
| VIEWPLT.DLG | View pilot record â€” Delete / Copy Pilot |
| AR_DLG.DLG | After-action report â€” General / Details / Videos / Photo Album / Parts List tabs |
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
| MC_NATF.DLG | Assign nationalities â€” full version (all sides) |
| MC_NAME.DLG | Enter pilot name (MC context) |
| PICKOBJ.DLG | Choose an object (mission editor object picker) |
| FORTAIRB.DLG | Multiplayer airbase â€” Deploy / Evacuate |
| FORTOPT.DLG | Multiplayer / fortification options |

### Preferences and configuration

| File | Screen |
|------|--------|
| GRAFPREF.DLG | Graphics preferences (640Ã—480) |
| GRAF320.DLG | Graphics preferences (320Ã—200) |
| SNDPREF.DLG | Sound preferences |
| SOUND320.DLG | Sound preferences (320Ã—200) |
| AUDIOD.DLG | Audio device options |
| UCONFIGD.DLG | User configuration dialog |

### Multiplayer â€” network

| File | Screen |
|------|--------|
| NEWNET.DLG | New network session options |
| NETJOIN.DLG | Join network game â€” game list |
| NETNEW.DLG | Host new game â€” wait for players |
| NETDIR.DLG | Network directory â€” player name / address |
| NETIPX.DLG | IPX/SPX connection â€” answer / status |
| NETIPX2.DLG | IPX/SPX settings (default / custom) |
| NETTCP.DLG | TCP/IP settings |
| NETEDT.DLG | Network player entry edit |
| NETBEDT.DLG | NetBEUI / transport-B player edit |
| NETCEDT.DLG | Transport-C player edit |

### Multiplayer â€” modem / serial

| File | Screen |
|------|--------|
| MODEM.DLG | Modem connection â€” Answer / player name / phone |
| MODEMCOM.DLG | Modem AT command strings (init / dial / listen) |
| MODEMSTS.DLG | Modem connection status |
| MODLIST.DLG | Modem selection list |
| SERIAL.DLG | Serial / null-modem connection |
| COM.DLG | Communications dialog |
| COMLIST.DLG | Communications transport list |

### Generic system dialogs

| File | Screen |
|------|--------|
| INFO320.DLG | Generic info box â€” OK only (320Ã—200) |
| INFO640.DLG | Generic info box â€” OK only (640Ã—480) |
| INFO0320.DLG | Info variant 0 â€” OK only (320Ã—200) |
| INFO0640.DLG | Info variant 0 â€” OK only (640Ã—480) |
| INFO2320.DLG | Info variant 2 â€” OK + Cancel (320Ã—200) |
| INFO2640.DLG | Info variant 2 â€” OK + Cancel (640Ã—480) |
| INFO2642.DLG | Info variant 2 alternate â€” OK + Cancel (640Ã—480) |
| INFOY320.DLG | Yes / No confirmation dialog (320Ã—200) |
| MDIAG.DLG | Generic message dialog (references GrafPrefPreload) |
| CDIAG.DLG | Continue / Cancel dialog |
| DDIAG.DLG | Disconnect confirmation dialog |
| LISTTST.DLG | Developer test dialog (placeholder/lorem ipsum text) |

The `CHOOSEAC.DLG` labels are the top-level game start menu items â€” displayed before any campaign or mission is active.

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 92 |

## Dispatch Table Layout (Confirmed)

DLG files use a **Phar Lap PE format** (signature `PL\0\0` instead of `PE\0\0`). There is no compiled x86 code â€” the CODE section is a **dispatch table** of variable-size records followed by packed label strings.

### Common record header â€” 10 bytes (all types)

**Confirmed** via `_DialogSetup` (DialogSetup), draw dispatcher (DialogDraw), and event dispatcher (DialogUpdate).

| Offset | Type | Field |
|--------|------|-------|
| +0x00 | u16 | `type_flags` â€” bits 0â€“14 = record type (0â€“9); bit 15 = disabled/dim flag. Set at runtime by `DisableActionButton`; cleared by `DialogDeselectItem`. Draw functions read bit 15 (via byte +0x01 bit 7) to choose dim vs. normal colour variant. |
| +0x02 | u32 | `next_record_ptr` â€” **engine-written** during `_DialogSetup`; zero in DLG file. Linked-list pointer to the next record; traversed by draw pass and event dispatcher via `*(record+2)`. |
| +0x06 | u32 | `draw_fn_ptr` â€” VA of the JMP thunk for this record's draw function, stored in the DLG file. The draw dispatcher (DialogDraw) calls `(**(draw_fn_ptr))(record_ptr)` for each record. |
| +0x0A | i16 | `x` â€” horizontal position relative to dialog origin |
| +0x0C | i16 | `y` â€” vertical position relative to dialog origin |

Record types and sizes (from `_DialogSetup` switch):

| Type | Draw fn | Record size | Notes |
|------|---------|-------------|-------|
| 0 | `_DrawAction` | 0x26 (38) | Clickable button |
| 1 | â€” | 0x1F (31) | Button variant |
| 2 | `_DrawEditBox` | 0x18 (24) | Edit box; first type-2 record tracked as focused edit box in dialog state |
| 3 | â€” | 0x17 (23) | Checkbox / toggle |
| 4 | â€” | 0x26 (38) | Scrollable list container; anchor record ptr stored in dialog state +0x22 |
| 5 | â€” | 0x19 (25) | |
| 6 | `_DrawRocker` | 0x27 (39) | Rocker switch; two independent hit zones (up and down halves) |
| 7 | â€” | 0x30 (48) | Scrollbar; `FUN_004891a0` called on show for thumb-position init |
| 8 | â€” | 0x1F (31) | Two-state button (selected / deselected, each has its own hit zone) |
| 9 | `_DrawText` / `_DrawFormattedText` | 0x16 (22) | Static label / formatted text |
| 10 | â€” | â€” | End-of-list sentinel |

**Note on the former +0x02..+0x09 gap:** Earlier analysis reported these bytes as unused by draw functions. They are now fully explained:
- +0x02..+0x05: `next_record_ptr` â€” zeroed in the DLG file; engine-written during `_DialogSetup`
- +0x06..+0x09: `draw_fn_ptr` â€” the thunk VA stored in the DLG file; draw functions do not read their own address, hence appearing unused in draw-function traces

### Hit-test and event dispatch (confirmed â€” DialogUpdate)

The event dispatcher iterates records via `next_record_ptr` and calls `PointInBox(mouse_coords_ptr, bounding_box_ptr)` for each record. The bounding-box pointer offset differs by type because each type stores its dimensions at a different place:

| Type | Hit-zone pointer |
|------|-----------------|
| 0, 1, 3, 6 (up half), 8 (offâ†’on zone) | `record + 0x0E` |
| 2 (edit box) | `record + 0x0C` |
| 4 (list container) | `record + 0x0A` |
| 6 (down half), 8 (onâ†’off zone) | `record + 0x16` |
| 7 (scrollbar) | `FUN_00488fd0` (custom handler) |

`_DialogWhatItem@0` (DialogWhatItem) returns the current value of `dialogItemPtr` â€” a global pointer set to the last record that passed the hit test during event dispatch. `_TopCenterDialog` (TopCenterDialog) positions the dialog: `screen_x = (screen_w âˆ’ dialog_w) / 2`, `screen_y = (screen_h âˆ’ dialog_h) / 3`.

### Per-type record fields

All offsets below are from the record base; the common 10-byte header (+0x00..+0x09) is not repeated.

#### _DrawAction â€” type 0, 38 bytes

| Offset | Type | Field |
|--------|------|-------|
| +0x0A | i16 | `x` |
| +0x0C | i16 | `y` |
| +0x0E | i16 | `screen_x` â€” engine-managed; lazily written as dialog_x + x on first render |
| +0x10 | i16 | `screen_y` â€” engine-managed; lazily written as dialog_y + y on first render |
| +0x12 | i16 | `render_width` â€” engine-managed; written from `width_px` on first render |
| +0x14 | i16 | `render_y_offset` â€” engine-managed; constant 20 (0x14) written on first render |
| +0x16 | u32 | (engine-managed â€” viewport handle slot) |
| +0x1A | u32 | `label_ptr` â€” ptr to label string or icon resource |
| +0x1C | u32 | `last_rendered_label` â€” engine-managed; written after each render |
| +0x1E | u32 | `icon_ptr` â€” engine-managed; icon viewport ptr (set from `actionBlueFont`/`4fca28`/etc. on first render) |
| +0x22 | i16 | `text_x` â€” text x offset within button |
| +0x24 | i16 | `text_y` â€” text y offset within button |

Lazily initialized fields (+0x0Eâ€“+0x15, +0x1C) are zero in the DLG file; the engine writes them on the first render pass.

#### _DrawText â€” type 9, 22 bytes

| Offset | Type | Field |
|--------|------|-------|
| +0x0A | u32 | `text_ptr` â€” `char*` to label string |
| +0x0E | u32 | `font_ptr` â€” font override (`0` â†’ default `PANELFNT`/`PANELFND`, chosen from disabled flag) |
| +0x12 | i16 | `x` |
| +0x14 | i16 | `y` |

#### _DrawFormattedText â€” type 9 variant, 36 bytes

| Offset | Type | Field |
|--------|------|-------|
| +0x0A | i16 | `x` |
| +0x0C | i16 | `y` |
| +0x0E | i16 | `width` |
| +0x10 | i16 | `height` |
| +0x12 | i16 | `secondary_display_x` â€” x offset for secondary item display; âˆ’1 = disabled |
| +0x14 | i16 | `secondary_display_y` |
| +0x16 | i16 | `visible_rows` â€” items per page; written to 1 when edit-mode activates |
| +0x18 | i16 | `result_val` â€” engine-managed; stores text-scroll result ptr at render time |
| +0x1A | i16 | `current_item` |
| +0x1C | i16 | `last_rendered` |
| +0x1E | i16 | `scroll_base` â€” starting offset for secondary display |
| +0x20 | u32 | `text_ptr` â€” `char**` string array |

#### _DrawCampaignList â€” 36 bytes

Same field layout as `_DrawFormattedText`. Render logic differs: rows are campaign entries with a highlight sprite from `CAMPHI.PIC`; row height is 0x4B px.

#### _DrawRocker â€” type 6, 39 bytes

| Offset | Type | Field |
|--------|------|-------|
| +0x0A | i16 | `x` |
| +0x0C | i16 | `y` |
| +0x0E | i16 | `render_x` â€” engine-managed; written as copy of `x` on first render |
| +0x10 | i16 | `render_y` â€” engine-managed; written as copy of `y` on first render |
| +0x12 | i16 | engine-managed â€” up-arrow icon offset A (18 or 16, based on `size_flag`) |
| +0x14 | i16 | engine-managed â€” up-arrow icon offset B (16 or 18) |
| +0x16 | i16 | engine-managed â€” left-state indicator x (= `x` on first render) |
| +0x18 | i16 | engine-managed â€” right-state indicator x (= `x + offset`) |
| +0x1A | i16 | engine-managed â€” down-arrow icon offset A (16 or 18) |
| +0x1C | i16 | engine-managed â€” down-arrow icon offset B (18 or 16) |
| +0x1E | i16 | `current_value` â€” 1 = up/left, else = down/right; updated from `click_state` after render |
| +0x20 | i16 | `click_state` â€” engine input: 0 = idle, 1 = click-up, else = click-down |
| +0x22 | u32 | `parent_ref` â€” ptr to linked parent control for auto-positioning |
| +0x26 | u8 | `size_flag` â€” non-zero = tall/large variant |

#### _DrawEditBox â€” type 2, 24 bytes

| Offset | Type | Field |
|--------|------|-------|
| +0x0A | i16 | `char_count` â€” field width in characters |
| +0x0C | i16 | `y` |
| +0x0E | i16 | `x` |
| +0x10 | i16 | `pixel_width` â€” engine-managed: `char_count Ã— 10 + 16`; written at render time |
| +0x12 | i16 | `height` â€” always written as 24 (0x18) at render time |
| +0x14 | u32 | `text_buffer` â€” `char*` to editable text |

### JMP thunks and state dispatch table

At the end of the CODE section, each imported function has a 6-byte JMP thunk:

```
FF 25 [iat_va LE]    ; JMP DWORD PTR [IAT slot]
```

Immediately before the thunk block there is a fixed 9-byte **state machine dispatch table**:

```
01 02 03 02 01 02 03 02 01
```

This same sequence appears in MUS CODE sections (after the `FC` opcode), confirming it is a shared engine construct â€” not DLG-specific. The `draw_fn_ptr` field (+0x06) in each dispatch record points to one of the JMP thunks, identifying which draw function the record invokes.

### _cancelString and _okString (button label indirection)

Records whose `draw_fn_ptr` points to the `_cancelString` or `_okString` thunk do **not** embed a string directly. The `label_ptr` field holds the VA of the thunk itself, which the engine dereferences at runtime to call the localized label function.

### CHOOSEAC.DLG decoded (main start screen)

Action-button records (type 0) in CHOOSEAC.DLG. Record addresses are PE virtual addresses within the CODE section.

| Record VA | x | y | width | Label |
|-----------|---|---|-------|-------|
| 0x1015 | 44 | 24 | 144 | Play Single Mission |
| 0x103B | 44 | 56 | 144 | Create Quick Mission |
| 0x1061 | 44 | 88 | 144 | Create Pro Mission |
| 0x1087 | 44 | 120 | 144 | Replay Last Mission |
| 0x10AD | 44 | 251 | 144 | Start New Campaign |
| 0x10D3 | 37 | 283 | 158 | Continue Old Campaign |
| 0x10F9 | 44 | 315 | 144 | View Pilot Records |
| 0x111F | 32 | 174 | 170 | Reference |

Records are spaced 0x26 (38) bytes apart, confirming type-0 record size. Y gap between 120 and 251 (131 px) separates mission-start options from management/info buttons.

### `_ChoosePreload` header record

Every DLG begins with one `_ChoosePreload` record that initialises assets and sets dialog state. In CHOOSEAC.DLG this record appears at PE VA 0x1000 with params `(379, 80, 238, 361)`.

**Params decoded** (Ghidra â€” `MMAccessE` DLG module descriptor getter, called from `_DialogSetup`):

The four i16 values are `(default_x, default_y, dialog_width, dialog_height)` â€” loaded from the DLG module's exported descriptor and stored in the dialog state frame:
- dialog_state +0x08/+0x0A = default screen position (x, y) â€” may be overridden by `_TopCenterDialog`
- dialog_state +0x0C/+0x0E = dialog dimensions (width, height) â€” used by `_TopCenterDialog` to compute centred position

For CHOOSEAC.DLG: `x=379, y=80, w=238, h=361`. `_TopCenterDialog` overrides position to `((screen_w âˆ’ 238) / 2, (screen_h âˆ’ 361) / 3)`.

**`_ChoosePreload` (`ChoosePreload`) confirmed behaviour** (Ghidra):
1. Calls `PushShellAlloc` â€” pushes current screen state onto dialog stack, sets state to `6`
2. Calls `FUN_00489840` (`__fastcall char param_1`) â€” loads action-button PIC and font assets keyed by `action_type`:
   - type 1: `ACTDFDxx.PIC` / `ACTDFNxx.PIC` (default, dim/normal); font `LMR`
   - type 3: `ACTI2Nxx.PIC` / `ACTI2Dxx.PIC`; fonts `fontact`/`fontacd`
   - type 4: `ACTI3Nxx.PIC` / `ACTI3Dxx.PIC`; same fonts
   - else:   `ACTIONxx.PIC` / `ACTIODxx.PIC`; same fonts
3. Decrements stack depth, pops screen state

`_ChoosePreload` is dispatched via computed indirect call â€” no direct CALL references (confirmed by Ghidra reference scan).

## Toolkit Roadmap

- New `lib/src/dlg.cpp` + `lib/include/fx/dlg.h` â€” parse dispatch table from PE CODE section
- New `cli/cmd_dlg.cpp` â€” `fx dlg dump <file.DLG>` prints control table as JSON `[{func, x, y, width, label}]`
- GUI: `dlg_editor.h/cpp` â€” visual dialog layout editor that lets modders reposition controls

## Related

- [MNU.md](MNU.md) â€” top-level menu files that surface DLG dialogs
