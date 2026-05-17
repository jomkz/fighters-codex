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

## Observed Dialogs (sample)

| File | Purpose | Labels |
|------|---------|--------|
| ACFTSLEC.DLG | Aircraft selection sub-screen | Arm Plane, Brief Map |
| AUDIOD.DLG | Audio options dialog | (edit box, OK) |
| BRIEFSCR.DLG | Mission briefing text | (formatted text, OK/Cancel) |
| CAMPAIGN.DLG | Campaign list picker | (campaign list, OK/Cancel) |
| CHOOSEAC.DLG | Main start screen | Play Single Mission, Create Quick Mission, Create Pro Mission, Replay Last Mission, Start New Campaign, Continue Old Campaign, Configure Hardware, Other Vehicle Info, View Pilot Records, Reference |
| SHWPILOT.DLG | Pilot service record | New Pilot, Delete, Copy Pilot, Select |

The `CHOOSEAC.DLG` labels are the top-level game start menu items — displayed before any campaign or mission is active.

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 92 |

## Dispatch Table Layout (Confirmed)

DLG files use a **Phar Lap PE format** (signature `PL\0\0` instead of `PE\0\0`). There is no compiled x86 code — the CODE section is a **dispatch table** of fixed-size records.

### Record structure (38 bytes each, empirically confirmed from CHOOSEAC.DLG)

| Offset | Size | Field |
|--------|------|-------|
| +0 | u32 | `thunk_va` — VA of the JMP thunk for the draw function (e.g. `_DrawAction`) |
| +4 | u16 | `x` — screen x in pixels |
| +6 | u16 | `y` — screen y in pixels |
| +8 | u8[10] | padding (zeros) |
| +18 | u16 | `width` — control width in pixels |
| +20 | u32 | `str_va` — VA of null-terminated label string in CODE section |
| +24 | u8[14] | trailing padding (zeros) |

Label strings are packed consecutively after the dispatch table in the same CODE section.

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

Records whose `thunk_va` points to the `_cancelString` or `_okString` thunk do **not** embed a string directly. The `str_va` field in those records holds the VA of the thunk itself, which the engine dereferences at runtime to call the engine's localized label function. Hex-dumping these records shows garbage if the `str_va` is treated as a code-section string pointer — it must be followed as an indirect call.

### _DrawEditBox (confirmed different record size)

`EDITNAME.DLG` hex dump confirms `_DrawEditBox` records use a different layout than `_DrawAction`:
- `x=31`, `y=20`, `w=9` (width in characters, not pixels)
- The record is shorter than 38 bytes; exact size not yet measured

### _DrawText

`str_va` appears at offset 0 of the record (rather than +20 as in `_DrawAction`). The remaining field layout differs — width and position fields are at different offsets.

### Other control types

`_DrawRocker` and `_DrawCampaignList` are confirmed present in `CALLSIGN.DLG`. Record sizes for these types are not yet measured.

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

Every DLG begins with one `_ChoosePreload` record (also 4-byte thunk + params) that contains the dialog bounding box or initialisation args. In CHOOSEAC.DLG this record appears at VA 0x1000 with params `(379, 80, 238, 361)` — likely `(x1=?, y1=80, width=238, height=361)` or a similar bounding-box encoding.

## Toolkit Roadmap

- New `lib/src/dlg.cpp` + `lib/include/ft/dlg.h` — parse dispatch table from PE CODE section
- New `cli/cmd_dlg.cpp` — `ft dlg dump <file.DLG>` prints control table as JSON `[{func, x, y, width, label}]`
- GUI: `dlg_editor.h/cpp` — visual dialog layout editor that lets modders reposition controls

## TODO — Deep Dive

- Measure exact record sizes for `_DrawRocker`, `_DrawEditBox`, `_DrawText` (confirmed different from `_DrawAction`'s 38 bytes)
- Confirm `_DrawEditBox` width field units (characters vs pixels — currently reads as 9, likely characters)
- Map `_DrawText` field offsets (str_va confirmed at offset 0; x/y/width positions unknown)
- Map all 92 DLG filenames to their in-game screens
- Decode `_ChoosePreload` params (bounding box vs dialog-type ID)

## Related

- [MNU.md](MNU.md) — top-level menu files that surface DLG dialogs
