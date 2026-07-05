---
format: DLG
name: Menu Dialog Layout
extensions: [".DLG"]
category: ui-overlay
endianness: little
spec:
  status: partial
  gaps:
    - kind: re-static
      issue: 54
      note: "type-4 list / type-7 scrollbar: a few engine-managed interior bytes unmapped"
codec:
  direction: read
  rationale: "engine-code container (dialog DLL, PL family like CAM/MNU/MC): fx_lib validates the container and extracts the control label strings; the structural per-record decode is a larger RE task (the on-disk record layout differs from the documented in-memory layout) tracked under #54"
  lib: [lib/src/dlg.cpp]
  commands: [dlg]
  tests: [tests/test_dlg.cpp]
  fuzz: []
  fixtures:
    synthetic: true
    real_manifest: true
related: [MNU]
---

# DLG — Menu Dialog Layout (.DLG)

FA_2.LIB contains 92 `.DLG` files. Each defines one dialog box in the FA menu
system. All are **Win32 PE DLLs** (MZ stub + Phar Lap PE image) loaded at
runtime; they import rendering functions from `main.dll` (= the game executable — see
[architecture.md](../architecture.md#overlay-system--win32-pe-dlls)) and embed
their label strings in the PE data section. The engine associates dialogs with
their parent MNU file; the DLG is loaded when the corresponding menu item is
selected.

## Tools

### fx

```
fx dlg info    <file.DLG>            # container check + CODE section geometry
fx dlg strings <file.DLG> [-n MIN]   # embedded control label strings
```

Container-level surface for now: same MZ + Phar Lap `PL` family as
[CAM](CAM.md)/[MNU](MNU.md); all 92 shipped dialogs validate and surface
their label strings. The structural record-table decode below is a larger
reverse-engineering task — the on-disk record layout differs from the
in-memory layout documented here (records begin at the `draw_fn_ptr` thunk
VA; the `type_flags`/`next_record_ptr` header fields are engine-written) —
and is tracked under #54.

## File Layout

All multi-byte integers are little-endian.

Win32 PE DLL. All DLG files import from `main.dll`. The set of imported
drawing functions varies per dialog and reveals the control types used:

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

### Dispatch Table Layout — confirmed

DLG files use **Phar Lap PE format** (signature `PL\0\0` instead of `PE\0\0`).
There is no compiled x86 code — the CODE section is a **dispatch table** of
variable-size records followed by packed label strings.

**Common record header — 10 bytes (all types)**, confirmed via `_DialogSetup`
(DialogSetup), the draw dispatcher (DialogDraw), and the event dispatcher
(DialogUpdate):

| Offset | Type | Field |
|--------|------|-------|
| `+0x00` | u16 | `type_flags` — bits 0–14 = record type (0–9); bit 15 = disabled/dim flag. Set at runtime by `DisableActionButton`; cleared by `DialogDeselectItem`. Draw functions read bit 15 (via byte +0x01 bit 7) to choose dim vs. normal colour variant. |
| `+0x02` | u32 | `next_record_ptr` — **engine-written** during `_DialogSetup`; zero in DLG file. Linked-list pointer to the next record; traversed by draw pass and event dispatcher via `*(record+2)`. |
| `+0x06` | u32 | `draw_fn_ptr` — VA of the JMP thunk for this record's draw function, stored in the DLG file. The draw dispatcher (DialogDraw) calls `(**(draw_fn_ptr))(record_ptr)` for each record. |
| `+0x0A` | i16 | `x` — horizontal position relative to dialog origin |
| `+0x0C` | i16 | `y` — vertical position relative to dialog origin |

Record types and sizes (from `_DialogSetup` switch):

| Type | Draw fn | Record size | Notes |
|------|---------|-------------|-------|
| 0 | `_DrawAction` | 0x26 (38) | Clickable button |
| 1 | — | 0x1F (31) | Button variant |
| 2 | `_DrawEditBox` | 0x18 (24) | Edit box; first type-2 record tracked as focused edit box in dialog state |
| 3 | — | 0x17 (23) | Checkbox / toggle |
| 4 | `_DrawListBox` | 0x26 (38) | Scrollable list container; anchor record ptr stored in dialog state +0x22 (`_DrawMissList` is the mission-list variant) |
| 5 | — | 0x19 (25) | Radio button (toggleable) — same field layout as type 1 + `type_flags` bit 0x8000 = selected |
| 6 | `_DrawRocker` | 0x27 (39) | Rocker switch; two independent hit zones (up and down halves) |
| 7 | — | 0x30 (48) | Scrollbar; `DialogScrollThumbInit` (`0x4891A0`) called on show for thumb-position init |
| 8 | — | 0x1F (31) | Two-state button (selected / deselected, each has its own hit zone) |
| 9 | `_DrawText` / `_DrawFormattedText` | 0x16 (22) | Static label / formatted text |
| 10 | — | — | End-of-list sentinel |

**Note on the former +0x02..+0x09 gap:** earlier analysis reported these bytes
as unused by draw functions. They are now fully explained:
- +0x02..+0x05: `next_record_ptr` — zeroed in the DLG file; engine-written
  during `_DialogSetup`
- +0x06..+0x09: `draw_fn_ptr` — the thunk VA stored in the DLG file; draw
  functions do not read their own address, hence appearing unused in
  draw-function traces

### Hit-test and event dispatch — confirmed (DialogUpdate)

The event dispatcher iterates records via `next_record_ptr` and calls
`PointInBox(mouse_coords_ptr, bounding_box_ptr)` for each record. The
bounding-box pointer offset differs by type because each type stores its
dimensions at a different place:

| Type | Hit-zone pointer |
|------|-----------------|
| 0, 1, 3, 6 (up half), 8 (off→on zone) | `record + 0x0E` |
| 2 (edit box) | `record + 0x0C` |
| 4 (list container) | `record + 0x0A` |
| 6 (down half), 8 (on→off zone) | `record + 0x16` |
| 7 (scrollbar) | `FUN_00488fd0` (custom handler) |

`_DialogWhatItem@0` (DialogWhatItem) returns the current value of
`dialogItemPtr` — a global pointer set to the last record that passed the hit
test during event dispatch. `_TopCenterDialog` (TopCenterDialog) positions the
dialog: `screen_x = (screen_w − dialog_w) / 2`,
`screen_y = (screen_h − dialog_h) / 3`.

### Per-type record fields

All offsets below are from the record base; the common 10-byte header
(+0x00..+0x09) is not repeated.

**_DrawAction — type 0, 38 bytes:**

| Offset | Type | Field |
|--------|------|-------|
| `+0x0A` | i16 | `x` |
| `+0x0C` | i16 | `y` |
| `+0x0E` | i16 | `screen_x` — engine-managed; lazily written as dialog_x + x on first render |
| `+0x10` | i16 | `screen_y` — engine-managed; lazily written as dialog_y + y on first render |
| `+0x12` | i16 | `render_width` — engine-managed; written from `width_px` on first render |
| `+0x14` | i16 | `render_y_offset` — engine-managed; constant 20 (0x14) written on first render |
| `+0x16` | u32 | (engine-managed — viewport handle slot) |
| `+0x1A` | u32 | `label_ptr` — ptr to label string or icon resource |
| `+0x1C` | u32 | `last_rendered_label` — engine-managed; written after each render |
| `+0x1E` | u32 | `icon_ptr` — engine-managed; icon viewport ptr (set from `actionBlueFont`/`4fca28`/etc. on first render) |
| `+0x22` | i16 | `text_x` — text x offset within button |
| `+0x24` | i16 | `text_y` — text y offset within button |

Lazily initialized fields (+0x0E–+0x15, +0x1C) are zero in the DLG file; the
engine writes them on the first render pass.

**_DrawText — type 9, 22 bytes:**

| Offset | Type | Field |
|--------|------|-------|
| `+0x0A` | u32 | `text_ptr` — `char*` to label string |
| `+0x0E` | u32 | `font_ptr` — font override (`0` → default `PANELFNT`/`PANELFND`, chosen from disabled flag) |
| `+0x12` | i16 | `x` |
| `+0x14` | i16 | `y` |

**_DrawFormattedText — type 9 variant, 36 bytes:**

| Offset | Type | Field |
|--------|------|-------|
| `+0x0A` | i16 | `x` |
| `+0x0C` | i16 | `y` |
| `+0x0E` | i16 | `width` |
| `+0x10` | i16 | `height` |
| `+0x12` | i16 | `secondary_display_x` — x offset for secondary item display; −1 = disabled |
| `+0x14` | i16 | `secondary_display_y` |
| `+0x16` | i16 | `visible_rows` — items per page; written to 1 when edit-mode activates |
| `+0x18` | i16 | `result_val` — engine-managed; stores text-scroll result ptr at render time |
| `+0x1A` | i16 | `current_item` |
| `+0x1C` | i16 | `last_rendered` |
| `+0x1E` | i16 | `scroll_base` — starting offset for secondary display |
| `+0x20` | u32 | `text_ptr` — `char**` string array |

**_DrawCampaignList — 36 bytes:** same field layout as `_DrawFormattedText`.
Render logic differs: rows are campaign entries with a highlight sprite from
`CAMPHI.PIC`; row height is 0x4B px.

**_DrawRocker — type 6, 39 bytes:**

| Offset | Type | Field |
|--------|------|-------|
| `+0x0A` | i16 | `x` |
| `+0x0C` | i16 | `y` |
| `+0x0E` | i16 | `render_x` — engine-managed; written as copy of `x` on first render |
| `+0x10` | i16 | `render_y` — engine-managed; written as copy of `y` on first render |
| `+0x12` | i16 | engine-managed — up-arrow icon offset A (18 or 16, based on `size_flag`) |
| `+0x14` | i16 | engine-managed — up-arrow icon offset B (16 or 18) |
| `+0x16` | i16 | engine-managed — left-state indicator x (= `x` on first render) |
| `+0x18` | i16 | engine-managed — right-state indicator x (= `x + offset`) |
| `+0x1A` | i16 | engine-managed — down-arrow icon offset A (16 or 18) |
| `+0x1C` | i16 | engine-managed — down-arrow icon offset B (18 or 16) |
| `+0x1E` | i16 | `current_value` — 1 = up/left, else = down/right; updated from `click_state` after render |
| `+0x20` | i16 | `click_state` — engine input: 0 = idle, 1 = click-up, else = click-down |
| `+0x22` | u32 | `parent_ref` — ptr to linked parent control for auto-positioning |
| `+0x26` | u8 | `size_flag` — non-zero = tall/large variant |

**_DrawEditBox — type 2, 24 bytes:**

| Offset | Type | Field |
|--------|------|-------|
| `+0x0A` | i16 | `char_count` — field width in characters |
| `+0x0C` | i16 | `y` |
| `+0x0E` | i16 | `x` |
| `+0x10` | i16 | `pixel_width` — engine-managed: `char_count × 10 + 16`; written at render time |
| `+0x12` | i16 | `height` — always written as 24 (0x18) at render time |
| `+0x14` | u32 | `text_buffer` — `char*` to editable text |

The six interactive types below were mapped from the `DialogUpdate` event switch
(`case 1/3/4/5/7/8`, keyed on `type_flags & 0x7FFF`), the `_DrawListBox` renderer, and the
scrollbar helpers (`DialogScrollThumbInit` `0x4891A0`, `DialogClampThumb` `0x489220`,
`DialogScrollbarHit` `0x488FD0`). `x`/`y` at `+0x0A`/`+0x0C` follow the common pattern; the
hit-box fields are the render-coordinate rectangles passed to `_PointInBox`.

**Type 1 — button variant (radio group member), 31 bytes:**

| Offset | Type | Field |
|--------|------|-------|
| `+0x0A` | i16 | `x` |
| `+0x0C` | i16 | `y` |
| `+0x0E` | i16×4 | hit box (`_PointInBox`) |
| `+0x16` | u8 | `pressed` — set to 1 on click; cleared by `DialogRadioGroupClear` |
| `+0x17` | i16 | `radio_group` — group id; peers with the same id are cleared on select |

**Type 3 — checkbox / toggle, 23 bytes:**

| Offset | Type | Field |
|--------|------|-------|
| `+0x0A` | i16 | `x` |
| `+0x0C` | i16 | `y` |
| `+0x0E` | i16×4 | hit box (`_PointInBox`) |
| `+0x16` | u8 | `state` — XOR-toggled (`state ^= 1`) on each click |

**Type 4 — scrollable list container, 38 bytes:**

| Offset | Type | Field |
|--------|------|-------|
| `+0x0A` | i16×4 | hit box (`_PointInBox`); a click computes the row from the y within it |
| `+0x0C` | i16 | `x` |
| `+0x0E` | i16 | `y` |
| `+0x10` | i16 | `pixel_height` — engine-managed; `visible_rows × 0x12` |
| `+0x16` | i16 | `visible_rows` — rows per page (row height `0x12` = 18 px) |
| `+0x18` | i16 | engine scroll/selection state |
| `+0x1A` | i16 | `total_items` |
| `+0x1C` | i16 | `top_index` — top/selected row; `1000` sentinel = none |
| `+0x20` | u32 | `rows_ptr` — `char**` row-string array |

**Type 5 — radio button (toggleable), 25 bytes:** same as type 1 (`pressed` `+0x16`,
`radio_group` `+0x17`), plus the `type_flags` bit `0x8000` carries the *selected* state — when
already selected, clicking calls `DialogRadioGroupClear(radio_group)` to release the group.

**Type 7 — scrollbar, 48 bytes:**

| Offset | Type | Field |
|--------|------|-------|
| `+0x0B` | u8 | `orientation` — 0 = vertical, non-0 = horizontal (flips the value axis) |
| `+0x0C` | i16 | `value` — current scroll value (engine-written from thumb position) |
| `+0x0E` | i16 | `value_max` — value range (thumb-to-value divisor) |
| `+0x10` | i16 | `thumb_x` — engine-computed thumb pixel X |
| `+0x12` | i16 | `thumb_y` — engine-computed thumb pixel Y |
| `+0x14` | i16 | track X reference (paired with `+0x1C`) |
| `+0x16` | i16 | track Y reference (paired with `+0x1E`) |
| `+0x18` | i16×4 | track rectangle = the hit box (`x0=+0x18, y0=+0x1A, x1=+0x1C, y1=+0x1E`) |
| `+0x24` | u32 | `on_scroll` — callback invoked on a track hit (`DialogScrollbarHit`) |

**Type 8 — two-state button, 31 bytes:**

| Offset | Type | Field |
|--------|------|-------|
| `+0x0A` | i16 | `x` |
| `+0x0C` | i16 | `y` |
| `+0x0E` | i16×4 | hit box A — pressed while `state == 0` → sets `state = 1` |
| `+0x16` | i16×4 | hit box B — pressed while `state == 1` → sets `state = 0` |
| `+0x1E` | u8 | `state` — 0/1; selects which hit zone is live |

### JMP thunks and state dispatch table

At the end of the CODE section, each imported function has a 6-byte JMP thunk:

```
FF 25 [iat_va LE]    ; JMP DWORD PTR [IAT slot]
```

Immediately before the thunk block there is a fixed 9-byte **state machine
dispatch table**:

```
01 02 03 02 01 02 03 02 01
```

This same sequence appears in MUS CODE sections (after the `FC` opcode),
confirming it is a shared engine construct — not DLG-specific. The
`draw_fn_ptr` field (+0x06) in each dispatch record points to one of the JMP
thunks, identifying which draw function the record invokes.

### _cancelString and _okString (button label indirection)

Records whose `draw_fn_ptr` points to the `_cancelString` or `_okString` thunk
do **not** embed a string directly. The `label_ptr` field holds the VA of the
thunk itself, which the engine dereferences at runtime to call the localized
label function.

### CHOOSEAC.DLG decoded (main start screen)

Action-button records (type 0) in CHOOSEAC.DLG. Record addresses are PE
virtual addresses within the CODE section.

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

Records are spaced 0x26 (38) bytes apart, confirming type-0 record size. Y gap
between 120 and 251 (131 px) separates mission-start options from
management/info buttons.

### `_ChoosePreload` header record

Every DLG begins with one `_ChoosePreload` record that initialises assets and
sets dialog state. In CHOOSEAC.DLG this record appears at PE VA 0x1000 with
params `(379, 80, 238, 361)`.

**Params decoded** (Ghidra — `MMAccessE` DLG module descriptor getter, called
from `_DialogSetup`): the four i16 values are
`(default_x, default_y, dialog_width, dialog_height)` — loaded from the DLG
module's exported descriptor and stored in the dialog state frame:
- dialog_state +0x08/+0x0A = default screen position (x, y) — may be
  overridden by `_TopCenterDialog`
- dialog_state +0x0C/+0x0E = dialog dimensions (width, height) — used by
  `_TopCenterDialog` to compute centred position

For CHOOSEAC.DLG: `x=379, y=80, w=238, h=361`. `_TopCenterDialog` overrides
position to `((screen_w − 238) / 2, (screen_h − 361) / 3)`.

**`_ChoosePreload` (`ChoosePreload`) confirmed behaviour** (Ghidra):
1. Calls `PushShellAlloc` — pushes current screen state onto dialog stack,
   sets state to `6`
2. Calls `FUN_00489840` (`__fastcall char param_1`) — loads action-button PIC
   and font assets keyed by `action_type`:
   - type 1: `ACTDFDxx.PIC` / `ACTDFNxx.PIC` (default, dim/normal); font `LMR`
   - type 3: `ACTI2Nxx.PIC` / `ACTI2Dxx.PIC`; fonts `fontact`/`fontacd`
   - type 4: `ACTI3Nxx.PIC` / `ACTI3Dxx.PIC`; same fonts
   - else:   `ACTIONxx.PIC` / `ACTIODxx.PIC`; same fonts
3. Decrements stack depth, pops screen state

`_ChoosePreload` is dispatched via computed indirect call — no direct CALL
references (confirmed by Ghidra reference scan).

## File Inventory

Complete filename → screen mapping, derived from embedded label strings in the
`.DLG` PE data sections. All 92 live in FA_2.LIB.

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

### Quick Battle wizard (24 dialogs — one per wizard step)

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

The `CHOOSEAC.DLG` labels are the top-level game start menu items — displayed
before any campaign or mission is active.

## Open Questions

### 1. Record types 1, 3, 4, 5, 7, 8 field layouts

**Mapped ([#258](https://github.com/jomkz/fighters-codex/issues/258)).** All ten record types
now have per-field layouts (see § Per-type record fields), recovered from the `DialogUpdate`
event switch, `_DrawListBox`, and the scrollbar helpers: types 1/5 are radio group members
(`pressed` `+0x16`, `radio_group` `+0x17`; type 5 also carries the *selected* state in
`type_flags` bit `0x8000`), type 3 is a toggle checkbox (`state` `+0x16`), type 4 the scrollable
list container, type 7 the scrollbar (value/range/thumb/track + `on_scroll` callback), and type 8
a two-state button (two hit zones + `state` `+0x1E`).

**Residual:** a few engine-managed interior bytes of the two largest records (type-4 list,
type-7 scrollbar) are not individually named — the same scratch-field level left unmapped in the
already-documented types. Behaviourally complete.

*Status: open — re-static (#54; interactive layouts mapped, minor scratch fields remain)*

## Related

**Formats:** [MNU](MNU.md) — top-level menu files that surface DLG dialogs.

**Engine:** [architecture.md](../architecture.md#overlay-system--win32-pe-dlls)
— the overlay DLL loading architecture; the fx_lib codec is tracked in #105.
