# CLI Reference

All commands follow the pattern `fx <subsystem> <subcommand> [args]`.

## Quick Reference

```
fx lib     ls / unpack / extract / pack / patch   # .LIB archive management
fx pic     info / unpack / pack         # .PIC images (dense, sparse, JPEG)
fx seq     dump / unpack / pack         # .SEQ cutscene timelines
fx audio   info / unpack / pack         # .11K / .5K / .8K raw PCM audio
fx ot      info / unpack / pack         # object type definitions
fx pt      info / unpack / pack         # aircraft type definitions
fx nt / jt / see / ecm / gas â€¦          # other type definitions
fx mission info / unpack / pack         # .M / .MM mission and map files
fx sh      info / unpack                # .SH 3D shapes → Wavefront OBJ
fx sms     dump                         # FA.SMS symbol map → CSV
fx t2      info                         # .T2 terrain map grid info
fx plt     info / dump                  # .P pilot save file
fx pal     info / dump                  # .PAL VGA palettes
fx inf     dump                         # .INF aircraft tech sheets
fx hud     dump                         # .HUD layout overlays
fx lay     dump / gradient              # .LAY sky/atmosphere layers
fx fnt     info / unpack                # .FNT bitmap fonts
fx mus     dump                         # .MUS music sequencer bytecode
fx bi      dump                         # .BI compiled AI disassembler
fx ai      compile                      # .AI → .BI compiler
```

## lib — Archive

```
fx lib ls      <file.LIB>
fx lib unpack  <file.LIB> [output_dir]
fx lib extract <file.LIB> <NAME> [NAME ...] [-o output_dir]
fx lib pack    <dir>      <output.LIB>
fx lib patch   <src.LIB>  <name> <file> <output.LIB>
```

#### `fx lib ls <file.LIB>`

List the contents of a `.LIB` archive.

```
> fx lib ls FA_2.LIB
Name           Flags      Size
-------------  -----  --------
_AFTB2.11K     dcl       26488
BALTIC.TXT     dcl        3421
PALETTE.PAL    dcl        2310
...
5405 file(s)
```

Flags: `raw` = uncompressed, `lzss` = LZSS, `pxpk` = PxPk, `dcl` = PKWare DCL.

#### `fx lib unpack <file.LIB> [output_dir]`

Extract and decompress all files. Output defaults to the archive stem.

```powershell
fx lib unpack FA_2.LIB out\FA_2
fx lib unpack FA_3.LIB          # extracts to .\FA_3\
```

Audio files whose names begin with `&` in the archive extract as `_` (e.g. `&AFTB2.11K` â†’
`_AFTB2.11K`) because Windows rejects `&` in filenames.

#### `fx lib extract <file.LIB> <NAME> [NAME ...] [-o output_dir]`

Extract one or more named entries. Output defaults to the current directory;
use `-o` to redirect. Name matching is case-insensitive.

```powershell
fx lib extract FA_2.LIB BALTIC.TXT
fx lib extract FA_2.LIB F16C_0.PIC F15C_0.PIC -o pics\
```

#### `fx lib pack <dir> <output.LIB>`

Pack all files in `dir` into a new `.LIB`. Files are stored uncompressed (flags=0).
The game accepts both raw and compressed entries.

#### `fx lib patch <src.LIB> <name> <file> <output.LIB>`

Replace one named entry without touching the rest of the archive.

```powershell
fx lib patch FA_2.LIB BALTIC.TXT edits\BALTIC.TXT FA_2_mod.LIB
fx lib patch FA_3.LIB F16C_0.PIC F16C_mod.PIC FA_3_mod.LIB
```

*See also: [fa/formats/LIB.md](fa/formats/LIB.md)*

## pic — Images

```
fx pic info   <file.PIC>
fx pic unpack <file.PIC> [-p PALETTE.PAL] [-o output.png]
fx pic pack   <file.png> [-p PALETTE.PAL] [-o output.PIC]
```

#### `fx pic info <file.PIC>`

Print the PIC header: format, dimensions, palette and span offsets.

#### `fx pic unpack <file.PIC> [-p PALETTE.PAL] [-o output.png]`

Decode to PNG. Handles all three sub-formats: JPEG, dense (format 0), and sparse (format 1).
`-p` is required for paletted PICs; omit for JPEG.

#### `fx pic pack <file.png> [-p PALETTE.PAL] [-o output.PIC]`

Encode to a dense PIC (format 0) with a full 256-color inline palette. Pixels with
alpha < 128 map to transparent (index 0xFF). Always provide the same `PALETTE.PAL`
used during unpack.

*See also: [fa/formats/PIC.md](fa/formats/PIC.md) Â· [fa/formats/PAL.md](fa/formats/PAL.md)*

## seq — Cutscene timelines

```
fx seq dump   <file.SEQ>
fx seq unpack <file.SEQ> [-o out.txt]
fx seq pack   <in.txt>   -o <out.SEQ>
```

#### `fx seq dump <file.SEQ>`

Pretty-print all events to stdout.

#### `fx seq unpack / pack`

Round-trip editable text. Output is byte-identical to originals.

*See also: [fa/formats/SEQ.md](fa/formats/SEQ.md)*

## audio — PCM audio

```
fx audio info   <file.11K|.5K>
fx audio unpack <file.11K|.5K> [-o out.wav] [-r hz]
fx audio pack   <in.wav>       -o <out.11K|.5K> [-r hz]
```

Sample rate is inferred from the file extension (`.11K` = 11025 Hz, `.5K` = 5512 Hz).
Override with `-r`. Input WAV for packing must be mono and 8-bit.

*See also: [fa/formats/11K.md](fa/formats/11K.md)*

## ot / nt / pt / jt / see / ecm / gas — Type definitions

All seven type definition formats share the same subcommand pattern:

```
ft <type> info   <file>
ft <type> unpack <file> [-o out.txt]
ft <type> pack   <in.txt> -o <out>
```

| Command | Format | Contents |
|---------|--------|----------|
| `fx ot` | `.OT` | Generic object type |
| `fx nt` | `.NT` | NPC / crew type |
| `fx pt` | `.PT` | Plane type (aircraft aerodynamics + avionics) |
| `fx jt` | `.JT` | Jettison / weapon type |
| `fx see` | `.SEE` | Seeker (missile guidance) type |
| `fx ecm` | `.ECM` | ECM pod type |
| `fx gas` | `.GAS` | Gas / smoke type |

```
> fx pt info F16C.PT
Name:        F-16C Fighting Falcon
Thrust:      28000 lbf (AB) / 17000 lbf (dry)
Max speed:   1327 mph
Ceiling:     50000 ft
Fuel:        6972 lb
```

*See also: [fa/formats/BRF.md](fa/formats/BRF.md) Â· [fa/formats/OT.md](fa/formats/OT.md) Â· [fa/formats/NT.md](fa/formats/NT.md) Â· [fa/formats/PT.md](fa/formats/PT.md) Â· [fa/formats/JT.md](fa/formats/JT.md) Â· [fa/formats/SEE.md](fa/formats/SEE.md) Â· [fa/formats/ECM.md](fa/formats/ECM.md) Â· [fa/formats/GAS.md](fa/formats/GAS.md)*

## mission / mm — Mission and map files

```
fx mission info   <file.M|.MM>
fx mission unpack <file.M|.MM> [-o out.txt]
fx mission pack   <in.txt>     -o <out.M|.MM>
```

`fx mm` is an alias for `.MM` map files. Round-trips byte-identically for all 592
mission files in FA_2.LIB.

*See also: [fa/formats/M.md](fa/formats/M.md) Â· [fa/formats/MM.md](fa/formats/MM.md)*

## sh — 3D shapes

```
fx sh info   <file.SH>
fx sh unpack <file.SH> [-o out.obj]
```

#### `fx sh info <file.SH>`

Print scale factor, bounding box (feet), vertex count, face count, and texture names.

#### `fx sh unpack <file.SH> [-o out.obj]`

Export geometry to Wavefront OBJ with `usemtl` directives for texture references.
Open in Blender, MeshLab, or any 3D viewer.

65 of 1275 FA shape files use x86 machine code for rendering (particle effects,
AC130, etc.) and produce no OBJ output. All others extract cleanly.

*See also: [fa/formats/SH.md](fa/formats/SH.md)*

## cb8 — FMV video

```
fx cb8 info   <file.CB8>
fx cb8 frames <file.CB8> [-o output_dir]
```

#### `fx cb8 info <file.CB8>`

Print video dimensions, frame count, frame rate, and total duration.

```
> fx cb8 info JANELOGO.CB8
video: 320 x 240, 466 frames, 15.0 fps, 31.07 s
audio: 11025 Hz PCM, 400 sync ticks/frame
```

#### `fx cb8 frames <file.CB8> [-o output_dir]`

Decode every frame to a PGM image (8-bit palette indices as greyscale) in
`output_dir` (default: current directory). Files are named `frame0000.pgm`,
`frame0001.pgm`, etc.

The decoder maintains a persistent canvas across frames; each MRFI chunk
applies a delta to the previous state. Output values are raw palette indices —
apply the appropriate PAL file separately to get RGB colours.

*See also: [fa/formats/CB8.md](fa/formats/CB8.md)*

## sms — Symbol map

```
fx sms dump <FA.SMS> [-o out.csv]
```

#### `fx sms dump <FA.SMS> [-o out.csv]`

Export all 3,829 MSVC C++ mangled symbols from `FA.SMS` to a two-column CSV
(`va,name`), sorted by virtual address. Without `-o`, prints to stdout. The CSV
uses LF line endings on every platform (previously CRLF on Windows).

```
> fx sms dump FA.SMS -o symbols.csv
FA.SMS -> symbols.csv (3829 symbols)
```

The CSV can be imported directly into Ghidra (Script Manager â†’ ImportSymbolsScript)
or IDA Pro to auto-label all known functions and data symbols.

*See also: [fa/formats/SMS.md](fa/formats/SMS.md)*

## t2 — Terrain map

```
fx t2 info <file.T2>
```

#### `fx t2 info <file.T2>`

Print the terrain grid dimensions, tile count, and surface class distribution
(water vs land tiles, top land classes by count).

```
> fx t2 info UKR.T2
Theater:    UKR
Grid:       25 x 26 (650 tiles)
Surface:    water 176 (27.1%)  land 474 (72.9%)
Land classes:
  0xD4  315 tiles (48.5%)
  0xD6   46 tiles (7.1%)
  ...
```

T2 files are stored in `FA_2.LIB`; unpack the archive first.

*See also: [fa/formats/T2.md](fa/formats/T2.md)*

## plt — Pilot save

```
fx plt info <file.P>
fx plt dump <file.P>
```

#### `fx plt info <file.P>`

Print pilot identity fields and active campaign state from a `.P` pilot save file.

```
> fx plt info PLT441.P
File:       PLT441.P  (9696 bytes)
Name:       Maverick
Callsign:   MAVERICK
Rank:       Captain
Voice:      ^ACID.5K
Nose art:   NOSE01
Left decal: LEFT03
Right decal:RIGHT03
Portrait:   PILOT02

Campaign:   UKRAINE.CAM  (Ukraine Crisis)
Aircraft:   F16C.PT
Pool:       F16C.PT, F15C.PT
Ordnance:
  AIM9M.JT         x4
  AIM120.JT        x2
  MK82.JT          x6
Sensors:    F16CSEE.SEE
```

#### `fx plt dump <file.P>`

Print the confirmed stats block (kill tallies, mission counters, weapon accuracy
at `0x1F80`–`0x21F7`). Decoding the remaining gap regions is tracked in
[#29](https://github.com/jomkz/fighters-codex/issues/29).

Pilot save files (`.P`) are stored in the FA install directory alongside `FA.EXE`.
The stats block (offsets 0xB0–0x0D7E) is not yet decoded; only the identity and
campaign blocks are read.

*See also: [fa/formats/P.md](fa/formats/P.md)*

## pal — VGA palettes

```
fx pal info <file.PAL>
fx pal dump <file.PAL> [-o out.png]
```

#### `fx pal info <file.PAL>`

Print the entry count and header details of a 256-color 6-bit VGA palette.

#### `fx pal dump <file.PAL> [-o out.png]`

Render the palette as a swatch-grid PNG for visual inspection.

*See also: [fa/formats/PAL.md](fa/formats/PAL.md)*

## inf — Aircraft tech sheets

```
fx inf dump <file.INF>
```

#### `fx inf dump <file.INF>`

Print the technical info sheet: aircraft metadata and the briefing-room scene
data (RTF text section and scene parameters).

*See also: [fa/formats/INF.md](fa/formats/INF.md)*

## hud — HUD layout overlays

```
fx hud dump <file.HUD>
```

#### `fx hud dump <file.HUD>`

Print the HUD overlay DLL's element table — positions, flags, and resource
references for each HUD element.

*See also: [fa/formats/HUD.md](fa/formats/HUD.md)*

## lay — Sky/atmosphere layers

```
fx lay dump     <file.LAY>
fx lay gradient <file.LAY> [-o output.png]
```

#### `fx lay dump <file.LAY>`

Print the sky and atmosphere lookup-table structure of a `.LAY` overlay DLL.

#### `fx lay gradient <file.LAY> [-o output.png]`

Render the atmosphere gradient tables to a PNG.

*See also: [fa/formats/LAY.md](fa/formats/LAY.md)*

## fnt — Bitmap fonts

```
fx fnt info   <file.FNT>
fx fnt unpack <file.FNT> [-o output_dir]
```

#### `fx fnt info <file.FNT>`

Print glyph count and font metrics from a font overlay DLL.

#### `fx fnt unpack <file.FNT> [-o output_dir]`

Extract every glyph as an image into the output directory, plus a `metrics.csv`
(`ascii,char,width,height`). The CSV uses LF line endings on every platform
(previously CRLF on Windows).

*See also: [fa/formats/FNT.md](fa/formats/FNT.md)*

## mus — Music sequencer bytecode

```
fx mus dump <file.MUS>
```

#### `fx mus dump <file.MUS>`

Disassemble the in-flight music sequencer bytecode: playlists, transitions,
and opcode listing.

*See also: [fa/formats/MUS.md](fa/formats/MUS.md)*

## bi — Compiled AI bytecode

```
fx bi dump <file.BI>
```

#### `fx bi dump <file.BI>`

Disassemble compiled `.BI` AI bytecode to readable mnemonics with
cross-referenced label annotations and resolved `CALL_BY_NAME` targets.

*See also: [fa/formats/BI.md](fa/formats/BI.md)*

## ai — AI script compiler

```
fx ai compile <file.AI> -o <file.BI>
```

#### `fx ai compile <file.AI> -o <file.BI>`

Compile a plain-text `.AI` script to the Phar Lap PE `.BI` bytecode format the
game's AI interpreter loads. All nine stock flight AIs compile to valid
bytecode.

*See also: [fa/formats/AI.md](fa/formats/AI.md), [fa/formats/BI.md](fa/formats/BI.md)*
