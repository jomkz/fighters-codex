# Aircraft Screen Assets (.PTS)

FA_2.LIB contains 37 `.PTS` files (e.g. `A4E.PTS`, `F22.PTS`). Each supplies the screen asset references for one aircraft type — primarily the aircraft icon shown in the hangar and selection screens. Each is a **Win32 PE DLL** loaded at runtime via `LoadLibrary`.

## Content

String analysis confirms each `.PTS` file references exactly **one** PIC asset — the aircraft icon. No `.HUD`, `.FNT`, or `.5K`/`.11K` references are present in any `.PTS` file; those assets are loaded directly by the cockpit and HUD subsystems at flight time.

The naming pattern `ICON<AC>.PIC` is consistent; some aircraft share an icon (e.g. `F22N.PTS` reuses `ICONF22.PIC`, all ASTOVL variants share `ICONAST.PIC`).

## Full Inventory (37 files)

| PTS file | Icon PIC | Notes |
|----------|----------|-------|
| A4E.PTS | ICONA4E.PIC | |
| A7.PTS | ICONA7.PIC | |
| A7V.PTS | ICONA7.PIC | shares A7 icon |
| AC130.PTS | ICONC130.PIC | |
| ASTOVL.PTS | ICONAST.PIC | |
| ASTOVLE.PTS | ICONAST.PIC | shares ASTOVL icon |
| ASTOVLF.PTS | ICONAST.PIC | shares ASTOVL icon |
| ASTOVLV.PTS | ICONAST.PIC | shares ASTOVL icon |
| AV8.PTS | ICONAV8.PIC | |
| B2.PTS | ICONB2.PIC | |
| E2000.PTS | ICONE20.PIC | |
| F104.PTS | ICONF104.PIC | |
| F117.PTS | ICONF117.PIC | |
| F14.PTS | ICONF14.PIC | |
| F16C.PTS | ICONF16.PIC | |
| F18.PTS | ICONF18.PIC | |
| F22.PTS | ICONF22.PIC | |
| F22N.PTS | ICONF22.PIC | shares F22 icon |
| F29.PTS | ICONF29.PIC | |
| F31.PTS | ICONF31.PIC | |
| F31E.PTS | ICONF31.PIC | shares F31 icon |
| F31F.PTS | ICONF31.PIC | shares F31 icon |
| F31V.PTS | ICONF31.PIC | shares F31 icon |
| F4B.PTS | ICONF4B.PIC | |
| F4J.PTS | ICONF4J.PIC | |
| F8J.PTS | ICONF8J.PIC | |
| GRIPEN.PTS | ICONGRI.PIC | |
| MIG17F.PTS | ICONM17F.PIC | |
| MIG21.PTS | ICONM21.PIC | |
| MIG21F.PTS | ICONM21F.PIC | |
| RAFALE.PTS | ICONRAF.PIC | |
| RAFALEE.PTS | ICONRAF.PIC | shares RAFALE icon |
| SEAHAR.PTS | ICONSEA.PIC | |
| SU33.PTS | ICONSU33.PIC | |
| SU35.PTS | ICONSU35.PIC | |
| YAK141.PTS | ICONY141.PIC | |
| ~MOTH.PTS | II~MOTH.PIC | campaign variant moth icon |

## Coverage

37 `.PTS` files vs 145+ `.PT` aircraft flight model files — most aircraft share a generic icon, and only those 37 have a dedicated `.PTS` entry. Variants of the same aircraft (ASTOVLE/F/V, F31E/F/V, RAFALEE, A7V) typically share their base aircraft's icon.

## Format

Win32 PE DLL. All observed `.PTS` files decompressed to **4608 bytes**.

## Location

| LIB | Count |
|-----|-------|
| FA_2.LIB | 37 |


## Related

- [BRF.md](BRF.md) — `.PT` aircraft flight model records (one per aircraft)
- [PIC.md](PIC.md) — `ICON<AC>.PIC` aircraft icon images
- [HUD.md](HUD.md) — cockpit HUD definitions (may be referenced via `.PTS`)
