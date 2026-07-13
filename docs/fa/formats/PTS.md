---
format: PTS
name: Aircraft Screen Assets
extensions: [".PTS"]
category: ui-overlay
endianness: little
spec:
  status: complete
codec:
  direction: read
  rationale: "engine-code container (screen-assets DLL, same PL family as CAM/MNU): fx_lib surfaces the container geometry and the single documented icon reference; writing compiled DLLs is fighters-legacy territory"
  lib: [lib/src/pts.cpp]
  commands: [pts]
  tests: [tests/test_pts.cpp]
  fuzz: [fuzz/fuzz_pts.cpp]
  fixtures:
    synthetic: true
    real_manifest: true
    real_install: false
related: [BRF, PIC, HUD]
---

# PTS — Aircraft Screen Assets (.PTS)

FA_2.LIB contains 37 `.PTS` files (e.g. `A4E.PTS`, `F22.PTS`). Each supplies
the screen asset references for one aircraft type — primarily the aircraft
icon shown in the hangar and selection screens. Each is a **Win32 PE DLL**
loaded at runtime; all observed files decompress to 4608 bytes.

(Unrelated to the community `.PTS` distribution rename of SH shadow shapes —
see the note in [SH.md](SH.md).)

## Tools

### fx

```
fx pts info <file.PTS>     # container check + referenced icon PIC
```

Same MZ + Phar Lap `PL` container family as [CAM](CAM.md)/[MNU](MNU.md)
(verified against F22.PTS); `pts_info` extracts the one icon reference.
All 37 shipped files resolve to the inventory below.

## File Layout

All multi-byte integers are little-endian.

Win32 PE DLL. String analysis confirms each `.PTS` file references exactly
**one** PIC asset — the aircraft icon. No `.HUD`, `.FNT`, or `.5K`/`.11K`
references are present in any `.PTS` file; those assets are loaded directly by
the cockpit and HUD subsystems at flight time.

The naming pattern `ICON<AC>.PIC` is consistent; some aircraft share an icon
(e.g. `F22N.PTS` reuses `ICONF22.PIC`, all ASTOVL variants share
`ICONAST.PIC`).

## File Inventory

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

All 37 live in FA_2.LIB.

**Coverage:** 37 `.PTS` files vs 145+ `.PT` aircraft flight model files — most
aircraft share a generic icon, and only those 37 have a dedicated `.PTS`
entry. Variants of the same aircraft (ASTOVLE/F/V, F31E/F/V, RAFALEE, A7V)
typically share their base aircraft's icon.

## Related

**Formats:** [BRF](BRF.md) — the `.PT` aircraft flight model records (one per
aircraft); [PIC](PIC.md) — the `ICON<AC>.PIC` aircraft icon images;
[HUD](HUD.md) — cockpit HUD definitions, loaded separately by the HUD
subsystem (not referenced from `.PTS`).
