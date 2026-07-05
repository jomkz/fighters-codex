---
format: FBC
name: Video Frame Index
extensions: [".FBC"]
category: video
endianness: little
spec:
  status: complete
codec:
  direction: none
  issue: 107
  gui: [gui/src/editors/vdo_editor.cpp]
  fixtures:
    synthetic: false
    real_manifest: true
related: [VDO, 11K]
---

# FBC — Video Frame Index (.FBC)

Companion index file for a `.VDO` video. Provides the byte size of each frame
so that a decoder can seek directly to any frame without scanning the video
data. Found in `FA_7.LIB`, paired with every `.VDO` and `.11K` of the same stem.

## File Layout

All multi-byte integers are little-endian.

No magic or header. The file is a flat array of N u32 values, where N equals
the frame count stored in the paired `.VDO` header (offset 16). File size is
always `4 × N` bytes.

| Offset | Size  | Type     | Description |
|--------|-------|----------|-------------|
| `0x00` | 4 × N | u32[N]   | Byte size of each frame in the paired .VDO |

### Relationship to .VDO

```
frame_data_offset(n) = 816 + sum(FBC[0 .. n-1])
invariant: sum(FBC[0 .. N-1]) == VDO_file_size - 816
```

### Observed Values

| File | Entries (N) | FBC size | VDO size | VDO size − 816 |
|------|-------------|----------|----------|----------------|
| AACA.FBC | 123 | 492 | 156,301 | 155,485 |
| AACB.FBC | 260 | 1040 | 362,853 | 362,045 |
| IPCA.FBC | 1685 | 6740 | 13,141,769 | 13,140,953 |

## Related

**Formats:** [VDO](VDO.md) — the video stream this file indexes; [11K](11K.md)
— the audio track sharing the same stem.

**Engine:** the fxs VDO editor renders this index (frame table); the
fx_lib codec is tracked in #107.
