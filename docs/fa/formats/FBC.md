# Frame Byte Count -- Frame Index (.FBC)

Companion index file for a `.VDO` video. Provides the byte size of each frame
so that a decoder can seek directly to any frame without scanning the video data.

Found in: `FA_7.LIB` (paired with every `.VDO` and `.11K` of the same stem)

## File Layout

No magic or header. The file is a flat array of N uint32 LE values, where N
equals the frame count stored in the paired `.VDO` header (offset 16).

```
Offset  Size   Description
------  ----   -----------
0       4 × N  N × uint32 LE: byte size of each frame in the paired .VDO
```

File size is always `4 × N` bytes.

## Relationship to .VDO

```
frame_data_offset(n) = 816 + sum(FBC[0 .. n-1])
invariant: sum(FBC[0 .. N-1]) == VDO_file_size - 816
```

## Observed Values

| File | Entries (N) | FBC size | VDO size | VDO size − 816 |
|------|-------------|----------|----------|----------------|
| AACA.FBC | 123 | 492 | 156,301 | 155,485 |
| AACB.FBC | 260 | 1040 | 362,853 | 362,045 |
| IPCA.FBC | 1685 | 6740 | 13,141,769 | 13,140,953 |
