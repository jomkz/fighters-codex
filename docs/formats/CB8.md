# CB8 -- FMV Video Format (.CB8)

Full-motion video used for intros, cutscenes, and per-aircraft presentation
clips. Each `.CB8` is paired with a `.11K` audio file of the same stem.

Found in: `FA_4C.LIB`, `FA_4D.LIB`, `FA_10.LIB`, `FA_10B.LIB`, `FA_11.LIB`, `FA_11B.LIB`

---

## File Layout

```
Offset  Size   Description
------  ----   -----------
0        64    DRBC outer header
64+      var   Inner video chunk (MRFA or VooM — see below)
```

---

## DRBC Outer Header (64 bytes)

```
Offset  Size  Description
------  ----  -----------
0        4    Magic: "DRBC" (ASCII)
4        4    uint32 LE: flags (observed: 0x00000000 or 0x00000001)
8        8    Unknown constant (observed: 96 00 22 56 65 00 00 00 across all files)
16       2    Unknown (observed: 0x0000 or 0x0080)
18      46    0xFF padding
```

The inner chunk type is determined by the magic at offset 64, not by any field
in the DRBC header.

---

## Inner Chunk: VooM (intro / cutscene FMV, 320×240)

Used for full-screen intro and cutscene videos (e.g. `ATF.CB8`, `C_INTRO.CB8`).

```
Offset  Size  Description
------  ----  -----------
0        4    Magic: "VooM" (ASCII)
4        4    uint32 LE: unknown
8        4    uint32 LE: width in pixels (observed: 320)
12       4    uint32 LE: height in pixels (observed: 240)
16+      var  Frame index and frame data (format not yet reversed)
```

---

## Inner Chunk: MRFA (per-aircraft clips, 128-wide)

Used for per-aircraft presentation videos in `FA_10.LIB`–`FA_11B.LIB` and
debriefing clips in `FA_4C.LIB`.

```
Offset  Size  Description
------  ----  -----------
0        4    Magic: "MRFA" (ASCII)
4        4    uint32 LE: unknown constant (observed: 0x00001CCE = 7374)
8        4    uint32 LE: width in pixels (observed: 128)
12       4    uint32 LE: height (observed: 0 — encoding unknown)
16       4    uint32 LE: unknown (observed: 8)
20       4    uint32 LE: unknown (observed: 1)
24+      var  Codebook + frame data (format not yet reversed)
```

Height is stored as 0 in all observed MRFA files; actual display height is
unknown without further decoding.

---

## Audio

Audio is stored separately in the paired `.11K` file (raw PCM). It is not
embedded in the `.CB8`.

## Observed Files

| File | Inner format | Width | Height | Source LIB |
|------|-------------|-------|--------|------------|
| ATF.CB8 | VooM | 320 | 240 | FA_4C.LIB |
| C_INTRO.CB8 | VooM | 320 | 240 | FA_4C.LIB |
| JANELOGO.CB8 | MRFA | 128 | ? | FA_4C.LIB |
| B2_D.CB8 | MRFA | 128 | ? | FA_10.LIB |
| 117_D.CB8 | MRFA | 128 | ? | FA_10.LIB |
