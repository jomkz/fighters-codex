# EALIB -- Archive Format (.LIB)

All game assets are packed into `.LIB` files using the EALIB container.

---

## File Layout

```
Offset  Size    Description
------  ------  -----------
0       5       Magic: "EALIB" (ASCII, no null terminator)
5       2       uint16 LE: number of directory entries (N)
7+      18 x N  Directory entries (see below)
[data]          File data blocks, immediately after the directory
```

File sizes are **not stored**. Each entry's size = next entry's offset - this entry's offset.
Last entry's size = file_size - last entry's offset.

## Directory Entry (18 bytes)

```
Offset  Size  Description
------  ----  -----------
0       13    Filename, null-padded, 8.3 DOS format (max 12 chars + null)
13       1    Flags byte (see table)
14       4    uint32 LE: absolute byte offset of file data in the .LIB
```

## Flags

| Value | Name | Description |
|-------|------|-------------|
| 0 | raw | Uncompressed -- data stored verbatim |
| 1 | lzss | LZSS compressed (4-byte decompressed-size prefix) |
| 3 | pxpk | Raw with a 4-byte `PXPK` inline header |
| 4 | dcl | PKWare DCL ("Blast") with 4-byte EA size prefix |

`ft lib unpack` decompresses flags=0 and flags=4 automatically. Flags=1 and flags=3
are rare and passed through without decompression.

## Filename Notes

Audio files (`*.11K`, `*.5K`) whose names start with `&` are looping sounds (engine
convention). Windows rejects `&` in filenames, so `ft lib unpack` maps `&` to `_` on
extraction. The original name is preserved in memory for patching operations.

Example: `&AFTB2.11K` in the archive extracts to `_AFTB2.11K` on disk.

---

## EA Compression Wrapper (flags=4)

Compressed entries prepend a 6-byte header to a standard PKWare DCL stream:

```
Offset  Size  Description
------  ----  -----------
0        4    uint32 LE: decompressed size
4        1    litmode: 0x00 = binary (only mode used in FA)
5        1    dictbits: 4=1024-byte window, 5=2048, 6=4096
6+            PKWare DCL bitstream (LSB-first)
```

---

## PKWare DCL Algorithm

Based on `blast.c` by Mark Adler (zlib project). See `lib/src/blast.cpp`.

**Bit reading:** bits are consumed LSB-first from a 32-bit buffer loaded one byte at a time.

**Fixed Huffman trees (not stored in stream):**

Length symbols (16 symbols):
```
RLE table:  { 0x12, 0x23, 0x24, 0x35, 0x26, 0x17 }
Code lengths: [2,3,3,3,4,4,4,5,5,5,5,6,6,6,7,7]
```

Distance symbols (64 symbols):
```
RLE table:  { 0x12, 0x14, 0x35, 0xE6, 0xF7, 0x97, 0xF8 }
```

Each RLE byte: high nibble = `reps-1`, low nibble = `code_length`.

**Decode loop:**
```c
flag = read_bits(1);
if (flag == 0) {
    emit read_bits(8);  // literal byte
} else {
    len  = lenbase[decode(len_tree)] + read_bits(lenextra[sym]);
    if (len == 519) break;  // END marker
    dsym = decode(dist_tree);
    dist = (len == 2) ? (dsym << 2) + read_bits(2) + 1
                      : (dsym << dictbits) + read_bits(dictbits) + 1;
    copy(len, from = output_tail - dist);  // overlapping ok; out-of-window = 0
}
```

**Critical: bit inversion.** The Huffman decode loop inverts each raw bit:
```c
code |= (bitbuf & 1) ^ 1;  // ^1 is required -- matches blast.c exactly
```
Without this inversion many files silently truncate to a wrong shorter output.

**Tables:**
```c
static const int lenbase[]  = {3,2,4,5,6,7,8,9,10,12,16,24,40,72,136,264};
static const int lenextra[] = {0,0,0,0,0,0,0,0, 1, 2, 3, 4,  5,  6,  7,  8};
```

---

## Known .LIB Files (Fighters Anthology)

| File | Location | Key Contents |
|------|----------|--------------|
| FA_1.LIB | Install dir | Fonts (.FNT), UI graphics |
| FA_2.LIB | Install dir | Shapes, missions, audio, type defs, cutscenes, text, palette -- 5405 files total |
| FA_3.LIB | Disk 2 (Red) | 822 JPEG-format textures, 269 aircraft data files |
| FA_4B.LIB | Install dir | Additional assets |
| FA_4C.LIB | Disk 1 (Blue) | — |
| FA_4D.LIB | Install dir | — |
| FA_7.LIB | Disk 1 (Blue) | — |
| FA_10.LIB | Disk 2 (Red) | — |
| FA_10B.LIB | Disk 2 (Red) | — |
| FA_11.LIB | Disk 2 (Red) | — |
| FA_11B.LIB | Disk 2 (Red) | — |
