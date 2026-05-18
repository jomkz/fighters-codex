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
22       2    uint16 LE: unknown (observed: 0x0100 = 256)
24       2    uint16 LE: unknown (observed: 1)
26       2    uint16 LE: audio sample rate in Hz (observed: 8000)
28       4    uint32 LE: unknown
32      16    zeroed
48     768    VGA palette: 256 × 3 bytes (R, G, B each 6-bit, range 0–63)
```

The 768-byte palette at offset 48 is a standard VGA DAC palette — 256 entries of
3 bytes each (red, green, blue), all values in 0–63 (6-bit per channel, matching
VGA register format). Entry 0 is always black (00 00 00). The palette is
per-video; each `.VDO` file carries its own.

Field at offset 22 (0x0100 = 256) likely reflects the palette size; field at
offset 24 (1) is likely audio channel count (mono).

## Frame Data

Frame data begins at offset 816. Frames are packed back-to-back with no
delimiters. Frame N starts at offset `816 + sum(FBC[0..N-1])`.

Frame data is palettized (8-bit palette indices into the header palette). The
per-frame encoding has not been fully reversed.

## Audio

Audio is stored separately in the paired `.11K` file (raw PCM, 8000 Hz mono
8-bit). It is not embedded in the `.VDO`.

## Observed Values

| File | Frames | Width | Height | FPS | Audio Hz |
|------|--------|-------|--------|-----|----------|
| AACA.VDO | 123 | 320 | 200 | 15 | 8000 |
| AACB.VDO | 260 | 320 | 200 | 15 | 8000 |
| IPCA.VDO | 1685 | 320 | 200 | 15 | 8000 |
