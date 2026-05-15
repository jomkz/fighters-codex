# RATVID -- Video Format (.VDO)

Video frames for mission briefing sequences. Each `.VDO` file is paired with a
`.FBC` index file and a `.11K` audio file of the same stem name.

Found in: `FA_7.LIB`

---

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
48     768    quantization / VQ codebook tables
```

The 768 bytes at offset 48 contain non-zero table data used by the video codec.
Their exact structure has not been fully reversed.

## Frame Data

Frame data begins at offset 816. Frames are packed back-to-back with no
delimiters. Frame N starts at offset `816 + sum(FBC[0..N-1])`.

Each frame encodes one video frame; the internal encoding uses the codebook
from the header. The exact per-frame bitstream format has not been reversed.

## Audio

Audio is stored separately in the paired `.11K` file (raw PCM, 8000 Hz mono
8-bit). It is not embedded in the `.VDO`.

## Observed Values

| File | Frames | Width | Height | FPS | Audio Hz |
|------|--------|-------|--------|-----|----------|
| AACA.VDO | 123 | 320 | 200 | 15 | 8000 |
| AACB.VDO | 260 | 320 | 200 | 15 | 8000 |
| IPCA.VDO | 1685 | 320 | 200 | 15 | 8000 |
