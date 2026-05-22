# RATVID -- Video Format (.VDO)

Video frames for mission briefing sequences. Each `.VDO` file is paired with a
`.FBC` index file and a `.11K` audio file of the same stem name.

Found in: `FA_7.LIB`

## File Layout

```
Offset  Size   Description
------  ----   -----------
0       816    Header (see below)
816+    var    Frame data blocks, one per frame (sizes given by paired .FBC)
```

## Header (816 bytes)

```
Offset  Size  Description
------  ----  -----------
0        6    Magic: "RATVID" (ASCII, no null terminator)
6        1    Major version = 1
7        1    Minor version = 2
8        4    uint32 LE: frame rate (observed: 15 fps)
12       4    uint32 LE: unknown (observed: 0)
16       2    uint16 LE: frame count (N)
18       2    uint16 LE: width in pixels (observed: 320)
20       2    uint16 LE: height in pixels (observed: 200)
22       2    uint16 LE: palette entry count (always 256)
24       2    uint16 LE: audio channel count (always 1 = mono)
26       2    uint16 LE: audio sample rate in Hz (observed: 8000)
28       4    uint32 LE: unknown
32      16    zeroed
48     768    VGA palette: 256 × 3 bytes (R, G, B each 6-bit, range 0–63)
```

The 768-byte palette at offset 48 is a standard VGA DAC palette — 256 entries of
3 bytes each (red, green, blue), all values in 0–63 (6-bit per channel, matching
VGA register format). Entry 0 is always black (00 00 00). The palette is
per-video; each `.VDO` file carries its own.

Field at offset 22 is confirmed to be the palette entry count (always 256,
matching the 768-byte palette block at +48). Field at offset 24 is confirmed
to be the audio channel count (always 1 = mono).

## Frame Data

Frame data begins at offset 816. Frames are packed back-to-back with no
delimiters. Frame N starts at offset `816 + sum(FBC[0..N-1])`.

Frame data is palettized (8-bit palette indices into the header palette at +48).
Each frame is Cobra-compressed. Partial RE of `DecodeFrame` (VA `0x442370` in
FA.EXE) reveals the following structure:

### Cobra codec — known structure

`DecodeFrame(MovieContext*, arg1, arg2)` dispatches on two fields of its
`MovieContext` struct:

| `context[8]` | Meaning |
|---|---|
| `0` | Delta frame (P-frame) — only changed regions are encoded |
| `1` | Key frame (I-frame) — full frame is encoded |

For delta frames (`context[8] == 0`), a second field `context[9]` selects a
sub-mode via an 8-entry jump table (VA `0x44260C`). Sub-modes 5 and 6 are the
only values observed in practice. Each sub-mode calls a dedicated leaf decoder
in the range `0x456300–0x45D090` (~45 functions); those leaf functions have not
yet been reversed.

`InitMovieContext` (VA `0x442360`) initialises the context; it only sets
`context[0x14] = 0`. The MovieContext struct is large — a pointer at offset
`0xC14E` points to the output/canvas buffer.

`VDOSetMode` (VA `0x4AED50`) selects between 320×200 and 640×480 render paths
and stores the result in `VDO.field_0x38`; this feeds `context[8]` during
decode.

The per-frame compression algorithm (the actual pixel-decompression loops inside
the leaf functions) has **not yet been fully reversed**. Implementing a decoder
requires completing the RE of those leaf functions and acquiring `.VDO` test
files (FA_7.LIB, not present in a typical FA install).

## Audio

Audio is stored separately in the paired `.11K` file (raw PCM, 8000 Hz mono
8-bit). It is not embedded in the `.VDO`.

## Confirmed Loader Functions (FA.SMS)

| Symbol | Demangled | Role |
|--------|-----------|------|
| `?ReadVDOHeader@@YADPAUVDO@@PAE@Z` | `char ReadVDOHeader(VDO *, unsigned char *)` | Parse the 816-byte file header into a `VDO` struct |
| `?ReadFrameSizesFile@@...` | `ReadFrameSizesFile(...)` | Read the paired `.FBC` frame-size index file |
| `?AllocVDO@@YADPAUVDO@@@Z` | `char AllocVDO(VDO *)` | Allocate frame decode buffers for a loaded VDO |
| `?OpenVDOFile@@...` | `OpenVDOFile(...)` | Open the `.VDO` file and begin streaming |

These four symbols are confirmed present in FA.SMS. All are member-like free functions operating on a `VDO` struct; the struct fields correspond directly to the header layout documented above.

## Observed Values

| File | Frames | Width | Height | FPS | Audio Hz |
|------|--------|-------|--------|-----|----------|
| AACA.VDO | 123 | 320 | 200 | 15 | 8000 |
| AACB.VDO | 260 | 320 | 200 | 15 | 8000 |
| IPCA.VDO | 1685 | 320 | 200 | 15 | 8000 |
