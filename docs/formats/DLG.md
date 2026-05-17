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

## TODO — Deep Dive

- Disassemble one DLG to determine the binary layout of control position tables (screen coordinates, sizes, z-order)
- Map all 92 DLG names to their in-game screens

## Related

- [MNU.md](MNU.md) — top-level menu files that surface DLG dialogs
