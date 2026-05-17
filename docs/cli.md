# CLI Reference

All commands follow the pattern `ft <subsystem> <subcommand> [args]`.

## Quick Reference

```
ft lib     ls / unpack / pack / patch   # .LIB archive management
ft pic     info / unpack / pack         # .PIC images (dense, sparse, JPEG)
ft seq     dump / unpack / pack         # .SEQ cutscene timelines
ft audio   info / unpack / pack         # .11K / .5K / .8K raw PCM audio
ft ot      info / unpack / pack         # object type definitions
ft pt      info / unpack / pack         # aircraft type definitions
ft nt / jt / see / ecm / gas …          # other type definitions
ft mission info / unpack / pack         # .M / .MM mission and map files
ft sh      info / unpack                # .SH 3D shapes → Wavefront OBJ
ft sms     dump                         # FA.SMS symbol map → CSV
ft t2      info                         # .T2 terrain map grid info
ft plt     info                         # .P pilot save file
```

## lib — Archive

```
ft lib ls     <file.LIB>
ft lib unpack <file.LIB> [output_dir]
ft lib pack   <dir>      <output.LIB>
ft lib patch  <src.LIB>  <name> <file> <output.LIB>
```

#### `ft lib ls <file.LIB>`

List the contents of a `.LIB` archive.

```
> ft lib ls FA_2.LIB
Name           Flags      Size
-------------  -----  --------
_AFTB2.11K     dcl       26488
BALTIC.TXT     dcl        3421
PALETTE.PAL    dcl        2310
...
5405 file(s)
```

Flags: `raw` = uncompressed, `lzss` = LZSS, `pxpk` = PxPk, `dcl` = PKWare DCL.

#### `ft lib unpack <file.LIB> [output_dir]`

Extract and decompress all files. Output defaults to the archive stem.

```powershell
ft lib unpack FA_2.LIB out\FA_2
ft lib unpack FA_3.LIB          # extracts to .\FA_3\
```

Audio files whose names begin with `&` in the archive extract as `_` (e.g. `&AFTB2.11K` →
`_AFTB2.11K`) because Windows rejects `&` in filenames.

#### `ft lib pack <dir> <output.LIB>`

Pack all files in `dir` into a new `.LIB`. Files are stored uncompressed (flags=0).
The game accepts both raw and compressed entries.

#### `ft lib patch <src.LIB> <name> <file> <output.LIB>`

Replace one named entry without touching the rest of the archive.

```powershell
ft lib patch FA_2.LIB BALTIC.TXT edits\BALTIC.TXT FA_2_mod.LIB
ft lib patch FA_3.LIB F16C_0.PIC F16C_mod.PIC FA_3_mod.LIB
```

## pic — Images

```
ft pic info   <file.PIC>
ft pic unpack <file.PIC> [-p PALETTE.PAL] [-o output.png]
ft pic pack   <file.png> [-p PALETTE.PAL] [-o output.PIC]
```

#### `ft pic info <file.PIC>`

Print the PIC header: format, dimensions, palette and span offsets.

#### `ft pic unpack <file.PIC> [-p PALETTE.PAL] [-o output.png]`

Decode to PNG. Handles all three sub-formats: JPEG, dense (format 0), and sparse (format 1).
`-p` is required for paletted PICs; omit for JPEG.

#### `ft pic pack <file.png> [-p PALETTE.PAL] [-o output.PIC]`

Encode to a dense PIC (format 0) with a full 256-color inline palette. Pixels with
alpha < 128 map to transparent (index 0xFF). Always provide the same `PALETTE.PAL`
used during unpack.

## seq — Cutscene timelines

```
ft seq dump   <file.SEQ>
ft seq unpack <file.SEQ> [-o out.txt]
ft seq pack   <in.txt>   -o <out.SEQ>
```

#### `ft seq dump <file.SEQ>`

Pretty-print all events to stdout.

#### `ft seq unpack / pack`

Round-trip editable text. Output is byte-identical to originals.

## audio — PCM audio

```
ft audio info   <file.11K|.5K>
ft audio unpack <file.11K|.5K> [-o out.wav] [-r hz]
ft audio pack   <in.wav>       -o <out.11K|.5K> [-r hz]
```

Sample rate is inferred from the file extension (`.11K` = 11025 Hz, `.5K` = 5512 Hz).
Override with `-r`. Input WAV for packing must be mono and 8-bit.

## ot / nt / pt / jt / see / ecm / gas — Type definitions

All seven type definition formats share the same subcommand pattern:

```
ft <type> info   <file>
ft <type> unpack <file> [-o out.txt]
ft <type> pack   <in.txt> -o <out>
```

| Command | Format | Contents |
|---------|--------|----------|
| `ft ot` | `.OT` | Generic object type |
| `ft nt` | `.NT` | NPC / crew type |
| `ft pt` | `.PT` | Plane type (aircraft aerodynamics + avionics) |
| `ft jt` | `.JT` | Jettison / weapon type |
| `ft see` | `.SEE` | Seeker (missile guidance) type |
| `ft ecm` | `.ECM` | ECM pod type |
| `ft gas` | `.GAS` | Gas / smoke type |

```
> ft pt info F16C.PT
Name:        F-16C Fighting Falcon
Thrust:      28000 lbf (AB) / 17000 lbf (dry)
Max speed:   1327 mph
Ceiling:     50000 ft
Fuel:        6972 lb
```

## mission / mm — Mission and map files

```
ft mission info   <file.M|.MM>
ft mission unpack <file.M|.MM> [-o out.txt]
ft mission pack   <in.txt>     -o <out.M|.MM>
```

`ft mm` is an alias for `.MM` map files. Round-trips byte-identically for all 592
mission files in FA_2.LIB.

## sh — 3D shapes

```
ft sh info   <file.SH>
ft sh unpack <file.SH> [-o out.obj]
```

#### `ft sh info <file.SH>`

Print scale factor, bounding box (feet), vertex count, face count, and texture names.

#### `ft sh unpack <file.SH> [-o out.obj]`

Export geometry to Wavefront OBJ with `usemtl` directives for texture references.
Open in Blender, MeshLab, or any 3D viewer.

65 of 1275 FA shape files use x86 machine code for rendering (particle effects,
AC130, etc.) and produce no OBJ output. All others extract cleanly.

## cb8 — FMV video

```
ft cb8 info   <file.CB8>
ft cb8 frames <file.CB8> [-o output_dir]
```

#### `ft cb8 info <file.CB8>`

Print video dimensions, frame count, frame rate, and total duration.

```
> ft cb8 info JANELOGO.CB8
video: 320 x 240, 466 frames, 15.0 fps, 31.07 s
audio: 11025 Hz PCM, 400 sync ticks/frame
```

#### `ft cb8 frames <file.CB8> [-o output_dir]`

Decode every frame to a PGM image (8-bit palette indices as greyscale) in
`output_dir` (default: current directory). Files are named `frame0000.pgm`,
`frame0001.pgm`, etc.

The decoder maintains a persistent canvas across frames; each MRFI chunk
applies a delta to the previous state. Output values are raw palette indices —
apply the appropriate PAL file separately to get RGB colours.

## sms — Symbol map

```
ft sms dump <FA.SMS> [-o out.csv]
```

#### `ft sms dump <FA.SMS> [-o out.csv]`

Export all 3,829 MSVC C++ mangled symbols from `FA.SMS` to a two-column CSV
(`va,name`), sorted by virtual address. Without `-o`, prints to stdout.

```
> ft sms dump FA.SMS -o symbols.csv
FA.SMS -> symbols.csv (3829 symbols)
```

The CSV can be imported directly into Ghidra (Script Manager → ImportSymbolsScript)
or IDA Pro to auto-label all known functions and data symbols.

## t2 — Terrain map

```
ft t2 info <file.T2>
```

#### `ft t2 info <file.T2>`

Print the terrain grid dimensions, tile count, and surface class distribution
(water vs land tiles, top land classes by count).

```
> ft t2 info UKR.T2
Theater:    UKR
Grid:       25 x 26 (650 tiles)
Surface:    water 176 (27.1%)  land 474 (72.9%)
Land classes:
  0xD4  315 tiles (48.5%)
  0xD6   46 tiles (7.1%)
  ...
```

T2 files are stored in `FA_2.LIB`; unpack the archive first.

## plt — Pilot save

```
ft plt info <file.P>
```

#### `ft plt info <file.P>`

Print pilot identity fields and active campaign state from a `.P` pilot save file.

```
> ft plt info PLT441.P
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

Pilot save files (`.P`) are stored in the FA install directory alongside `FA.EXE`.
The stats block (offsets 0xB0–0x0D7E) is not yet decoded; only the identity and
campaign blocks are read.
