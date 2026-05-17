# Aircraft Screen Assets (.PTS)

FA_2.LIB contains 37 `.PTS` files (e.g. `A4E.PTS`, `F22.PTS`). Each supplies the screen asset references for one aircraft type — primarily the aircraft icon shown in the hangar and selection screens. Each is a **Win32 PE DLL** loaded at runtime via `LoadLibrary`.

## Content

String analysis shows each `.PTS` file references a single PIC asset:

| File | PIC reference |
|------|---------------|
| A4E.PTS | `ICONA4E.PIC` |
| F22.PTS | `ICONF22.PIC` |
| … | `ICON<AC>.PIC` |

The naming pattern `ICON<aircraft>.PIC` is consistent — the `.PTS` file is the lookup bridge between an aircraft type (identified by the `.PTS` base name) and its icon image.

## Coverage

37 `.PTS` files vs 145 `.PT` aircraft flight model files — most aircraft share a generic icon, and only those 37 with unique cockpit or selection screen art have a dedicated `.PTS` entry.

## Format

Win32 PE DLL. All observed `.PTS` files decompressed to **4608 bytes**.

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 37 |

## TODO — Deep Dive

- Extract all 37 filenames and cross-reference with `.PT` aircraft to identify which aircraft have custom icons
- Disassemble a `.PTS` to determine if additional asset references exist beyond the icon PIC (e.g. cockpit PIC, sound file)
- Determine whether `.PTS` also links to `.HUD` or `.FNT` for the aircraft's cockpit display

## Related

- [BRF.md](BRF.md) — `.PT` aircraft flight model records (one per aircraft)
- [PIC.md](PIC.md) — `ICON<AC>.PIC` aircraft icon images
- [HUD.md](HUD.md) — cockpit HUD definitions (may be referenced via `.PTS`)
