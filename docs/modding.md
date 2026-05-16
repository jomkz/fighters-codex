# Modding Guide

Quick recipes for common FA modding tasks using `ft`.

Extract `PALETTE.PAL` from `FA_2.LIB` once before any paletted image work:

```powershell
ft lib unpack FA_2.LIB out\FA_2
# PALETTE.PAL is now at out\FA_2\PALETTE.PAL
```

---

## Texture mod (FA_3.LIB aircraft skins)

FA_3.LIB lives on the CD (typically `F:\`). All 822 textures are JPEG-format PICs —
no palette needed to decode them, but you do need the palette to re-encode.

```powershell
# Extract textures from the CD
ft lib unpack F:\FA_3.LIB out\FA_3

# Decode one texture to PNG
ft pic unpack out\FA_3\F16C_0.PIC -o F16C_0.png

# Edit F16C_0.png in Photoshop, GIMP, etc. -- keep the original dimensions.

# Re-encode to PIC (uses the system palette)
ft pic pack F16C_0.png -p out\FA_2\PALETTE.PAL -o F16C_mod.PIC

# Patch the modified texture back into a copy of the LIB
ft lib patch F:\FA_3.LIB F16C_0.PIC F16C_mod.PIC FA_3_mod.LIB

# Place FA_3_mod.LIB in the install directory -- the game prefers it over the CD copy
```

The re-encoded PIC is format 0 (dense) with an inline 256-color palette. The engine
accepts this in place of the original JPEG format.

---

## Text / data mod (mission text, pilot bios)

```powershell
ft lib unpack FA_2.LIB out\FA_2
notepad out\FA_2\BALTIC.TXT
ft lib patch FA_2.LIB BALTIC.TXT out\FA_2\BALTIC.TXT FA_2_mod.LIB
```

---

## Aircraft stats mod (.PT)

```powershell
ft lib unpack FA_2.LIB out\FA_2

# Export to editable text
ft pt unpack out\FA_2\F16C.PT -o F16C.pt.txt

# Edit F16C.pt.txt -- thrust, max_speed, fuel_capacity, etc.

# Re-encode and patch
ft pt pack F16C.pt.txt -o F16C_mod.PT
ft lib patch FA_2.LIB F16C.PT F16C_mod.PT FA_2_mod.LIB
```

---

## 3D model inspection (.SH)

```powershell
ft lib unpack FA_2.LIB out\FA_2

# Quick stats
ft sh info out\FA_2\F16C.SH

# Export to Wavefront OBJ and open in Blender / MeshLab
ft sh unpack out\FA_2\F16C.SH -o F16C.obj
```

---

## Mission edit (.M)

```powershell
ft lib unpack FA_2.LIB out\FA_2
ft mission unpack out\FA_2\BALTIC.M -o BALTIC.m.txt

# Edit object positions, weather, side assignments...

ft mission pack BALTIC.m.txt -o BALTIC_mod.M
ft lib patch FA_2.LIB BALTIC.M BALTIC_mod.M FA_2_mod.LIB
```

---

## Cutscene edit (.SEQ)

```powershell
ft lib unpack FA_2.LIB out\FA_2
ft seq unpack out\FA_2\KDEAD.SEQ -o KDEAD.seq.txt

# Edit timings, bitmap references, sound names...

ft seq pack KDEAD.seq.txt -o KDEAD_mod.SEQ
ft lib patch FA_2.LIB KDEAD.SEQ KDEAD_mod.SEQ FA_2_mod.LIB
```

---

## Mission briefing text (.MT)

`.MT` files are plain-text companions to `.M` files and can be edited directly — no
`ft` command needed. They live alongside the `.M` in the `.LIB`.

```powershell
ft lib unpack FA_2.LIB out\FA_2

# Open the briefing text for any mission
notepad out\FA_2\BALTIC.MT

# Edit section 2 (briefing) and sections 3/4 (debrief success/failure), then patch back
ft lib patch FA_2.LIB BALTIC.MT out\FA_2\BALTIC.MT FA_2_mod.LIB
```

See [formats/MISSION.md](formats/MISSION.md) for the section and directive syntax.

---

## Aircraft flight model reference data

The community has produced G-envelope spreadsheets for 70+ real aircraft
(F-4, F-14, F-15, F-16, F/A-18, F-22, MiG-25, Rafale, Typhoon, and many more),
measuring stall and max speeds in KTAS at altitude breakpoints from sea level to 65,000 ft
across −4 G to +9 G. These map directly to the `env` section in `.PT` files.

The [Fighters Anthology Resource Center](http://jkpeterson.net/fa/) and [USNRaptor](http://myplace.frontier.com/~usnraptor/) community archives include spreadsheets
covering dozens of airframes. They use KTAS at altitude breakpoints — convert to `.PT`
`env` units as follows:

```
speed_ft_per_sec = ktas * 1.6878         # 1 knot = 1.6878 ft/s
altitude_ft      = altitude_as_read      # already in feet
```

---

## Tips

- The game loads flags=0 (uncompressed) LIB entries just as well as flags=4 (compressed).
  `ft lib patch` always writes uncompressed — no need to re-compress.
- Keep image dimensions unchanged. The engine does not resize at load time.
- Pixels are quantized to the nearest palette color on PIC re-encode. Keep source art
  at 256 colors or less for best fidelity.
- Test mods by placing the modified `.LIB` in the install directory. The engine searches
  there before the CD, so you can override without burning a disc.
