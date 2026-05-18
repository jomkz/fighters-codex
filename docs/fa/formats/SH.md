# SH -- Shape / 3D Model Format (.SH)

## Container: Phar Lap PE/LE Executable

SH files are **Phar Lap PE/LE executables**. The shape bytecode lives in the CODE section.

Parse via the standard MZ/PE header chain:

```
data[0x00..0x02]  MZ signature: 'M' 'Z'
data[0x3C..0x40]  e_lfanew (u32 LE) -> offset of PE/LE header
pe[0..2]          Phar Lap signature: 'P' 'L' (same layout as standard 'P' 'E')
pe[4..6]          Machine (u16, ignored)
pe[6..8]          NumberOfSections (u16)
pe[20..22]        SizeOfOptionalHeader (u16)
pe[24 + SizeOfOptionalHeader ..]   Section table (40 bytes per entry)

Section entry layout:
  [0..8]    Name (8 bytes, null-padded) -- always "CODE" for the code section
  [8..12]   VirtualSize (u32)
  [12..16]  VirtualAddress (u32)
  [16..20]  SizeOfRawData (u32)   <-- use this
  [20..24]  PointerToRawData (u32) <-- use this as file offset
  [24..40]  (ignored)
```

Take the **first section** with `PointerToRawData > 0` — that is the code section.
Always named `CODE`. `PointerToRawData` is the file offset; `SizeOfRawData` is its length.

## Code Section Structure

```
[FF FF]              2-byte signature
[unk0 i16]           unknown
[unk1 i16]           unknown (sometimes encodes a file ID)
[scale i16]          coordinate scale (bytes 6..8): see table below
[ext[0] i16]         bounding extent X (half-width)
[ext[1] i16]         bounding extent Y (half-depth)
[ext[2] i16]         bounding extent Z (half-height)
[instruction stream] variable-length opcodes
```

**Scale table** (`world_coord_feet = raw_i16 * scale_factor`):

| scale field | scale_factor | Notes |
|-------------|--------------|-------|
| 7 | 0.5 | USNF97 and earlier only |
| 8 | 1.0 | Standard FA -- 1 unit = 1 foot |
| 9 | 2.0 | Large objects |
| 10 | 4.0 | Very large objects |
| 11 | 8.0 | Terrain features |
| 0 | 1.0 | Treated as 8 |

## Instruction Dispatch

Instructions are either **Byte-magic** (1-byte opcode) or **Word-magic** (2-byte opcode:
`[op_byte, 0x00]`). Dispatch is on the first byte.

### Key Instructions

| First byte | Name | Total size | Notes |
|------------|------|------------|-------|
| `0xFF` | Header | 14 | Always first; scale at `[6..8]` |
| `0x00` | EndObject | all remaining | Triggers X86Unknown skip if `obj_end_off` set |
| `0x01` | EndShape | all remaining | Terminates the shape |
| `0x1E` | Pad | 1+ | Run of `0x1E` NOP bytes |
| `0x38` | Unk38 | 3 | |
| `0xBC` | UnkBC | 2 | |
| `0xF0` | X86Code | variable | Bail out -- x86 machine code |
| `0xF6` | VertexInfo | 7 | `[F6][idx u16][color u8][normal i8[3]]` |
| `0xFC` | Face | variable | See face format section |

### Word-magic instructions (second byte = `0x00`)

| First byte | Name | Total size |
|------------|------|------------|
| `0x06` | Unk06 | `16 + u16@[14]` |
| `0x08` | Unk08 | 4 |
| `0x0C` | Unk0C | `12 + u16@[10]` |
| `0x0E` | Unk0E | `12 + u16@[10]` |
| `0x10` | Unk10 | `12 + u16@[10]` |
| `0x12` | Unmask | 4 |
| `0x2E` | Unk2E | 4 |
| `0x3A` | Unk3A | 6 |
| `0x40` | JumpToFrame | `4 + u16@[2]*2` |
| `0x42` | SourceName | `2 + strlen + 1` |
| `0x44` | Unk44 | 4 |
| `0x46` | Unk46 | 2 |
| `0x48` | Jump | 4 |
| `0x4E` | Unk4E | 2 |
| `0x50` | Unk50 | 6 |
| `0x66` | Unk66 | 10 |
| `0x68` | Unk68 | 8 |
| `0x6C` | Unk6C | 13/14/16 (flag@[10]: 0x38=13, 0x48=14, 0x50=16) |
| `0x6E` | UnmaskLong | 6 |
| `0x72` | Unk72 | 4 |
| `0x76` | Unk76 | 10 |
| `0x78` | Unk78 | 12 |
| `0x7A` | Unk7A | 10 |
| `0x82` | VertexBuffer | `6 + u16@[2]*6` |
| `0x96` | Unk96 | 6 |
| `0xA6` | JumpToDetail | 6 |
| `0xAC` | JumpToDamage | 4 |
| `0xB2` | UnkB2 | 2 |
| `0xB8` | UnkB8 | 4 |
| `0xC4` | XformUnmask | 16 |
| `0xC6` | XformUnmaskLong | 18 |
| `0xC8` | JumpToLOD | 8 |
| `0xCA` | UnkCA | 4 |
| `0xCE` | UnkCE | 40 |
| `0xD0` | UnkD0 | 4 |
| `0xD2` | UnkD2 | 8 |
| `0xDA` | UnkDA | 4 |
| `0xDC` | UnkDC | 12 |
| `0xE0` | TextureIndex | 4 |
| `0xE2` | TextureFile | 16 |
| `0xE4` | UnkE4 | 20 |
| `0xE6` | UnkE6 | 10 |
| `0xE8` | UnkE8 | 6 |
| `0xEA` | UnkEA | 8 |
| `0xEE` | UnkEE | 2 |
| `0xF2` | PtrToObjEnd | 4 |

## Geometry Instructions in Detail

### VertexBuffer (0x82 0x00)

Pushes a batch of vertices into the global vertex pool.

```
[82 00]           opcode (2 bytes)
[nverts u16]      number of vertices in this buffer
[push_at u16]     byte offset into the global pool where this buffer starts
[x y z i16 ...]   nverts * 3 signed 16-bit coordinates (LE)
```

**Pool index** = `push_at / 8`. Vertex slot size is 8 bytes in the engine's pool
(6 bytes of coords + 2 bytes alignment padding), so `push_at` is always a multiple of 8.
Face indices reference global pool indices.

### TextureFile (0xE2 0x00)

Sets the current texture for subsequent faces.

```
[E2 00]           opcode
[name 14 bytes]   null-padded ASCII filename (e.g. "_A10.PIC")
```

### PtrToObjEnd (0xF2 0x00)

Records the absolute code-section byte offset of the EndObject instruction.

```
[F2 00]           opcode
[offset u16]      absolute byte offset within the code section
```

### Face (0xFC)

Variable-length polygon face instruction.

```
[FC]
[content_flags u8]   see FaceContentFlags below
[layout_flags u8]    see FaceLayoutFlags below
[color u8]           palette color index for untextured rendering
[is_shadow u8]       non-zero if this face is a shadow polygon

[if HAVE_FACE_NORMAL (content_flags & 0x40):]
    [face_normal i16[3]]    face normal vector, scale by 1/32765.0 to get float
    [if USE_BYTE_FACE_CENTER (layout_flags & 0x02):]
        [face_center i8[3]]
    [else:]
        [face_center i16[3]]

[nindices u8]           number of vertex indices (= number of polygon corners)

[if USE_SHORT_INDICES (layout_flags & 0x04):]
    [indices u16[nindices]]  2-byte pool indices
[else:]
    [indices u8[nindices]]   1-byte pool indices

[if HAVE_TEXCOORDS (content_flags & 0x04):]
    [if USE_BYTE_TEXCOORDS (layout_flags & 0x01):]
        [(s u8, t u8) * nindices]    8-bit texcoords
    [else:]
        [(s u16, t u16) * nindices]  16-bit texcoords
```

**FaceContentFlags:**

| Bit | Mask | Meaning |
|-----|------|---------|
| 7 | 0x80 | Unk1 |
| 6 | 0x40 | HAVE_FACE_NORMAL |
| 5 | 0x20 | Unk2 (brighter shading) |
| 4 | 0x10 | Unk3 (perspective-correct mapping) |
| 3 | 0x08 | Unk4 |
| 2 | 0x04 | HAVE_TEXCOORDS |
| 1 | 0x02 | FILL_BACKGROUND |
| 0 | 0x01 | Unk5 |

**FaceLayoutFlags:**

| Bit | Mask | Meaning |
|-----|------|---------|
| 3 | 0x08 | Unk0 |
| 2 | 0x04 | USE_SHORT_INDICES (u16 instead of u8) |
| 1 | 0x02 | USE_BYTE_FACE_CENTER (i8[3] instead of i16[3]) |
| 0 | 0x01 | USE_BYTE_TEXCOORDS (u8[2] instead of u16[2]) |

## X86Unknown Region

Some models (main aircraft like A10.SH, AC130.SH) use x86 machine code to drive
face rendering. These regions are detected and skipped:

1. **PtrToObjEnd (0xF2)** seen: record `obj_end_off = offset_field`.
2. **EndObject (0x00)** seen while `current_pos < obj_end_off`: the range
   `[current_pos .. obj_end_off)` is x86 machine code mixed with embedded SH face
   instructions. Skip to `obj_end_off` and continue.
3. **EndObject (0x00)** seen while `current_pos >= obj_end_off`: real end of object;
   stop parsing.

Models with x86-only geometry cannot be exported to OBJ without x86 disassembly.

## Geometry Extraction Results

Tested against all 1275 `.SH` files from `FA_2.LIB`:

| Result | Count | % |
|--------|-------|---|
| Vertices + faces extracted | 1210 | 94.9% |
| x86-only geometry (no OBJ output) | 65 | 5.1% |
| Parser crash / error | 0 | 0% |

**x86-only files** are all procedural effects or complex models:
`FIRE.SH`, `FLARE.SH`, `BULLET.SH`, `CHAFF.SH`, `CLOUD*.SH`, `CRATER.SH`,
`DEBRIS.SH`, `EXP.SH`, `EJECT.SH`, `AC130.SH` (and variants), `CATGUY.SH`, etc.

**Shadow models** (`*_S.SH`): flat ground silhouettes, Z=0, typically 6-20 faces.

**Sample results:**

| File | Scale | Verts | Faces | Textures |
|------|-------|-------|-------|----------|
| A10.SH | 8 (1x) | 361 | 81 | _a10.PIC |
| A10_B.SH | 8 (1x) | 71 | 98 | _a10_b.PIC |
| A10_S.SH | 8 (1x) | 21 | 6 | (none) |
| F22.SH | 8 (1x) | 290 | 89 | |
| F15E.SH | 8 (1x) | 387 | 42 | |
| AC130.SH | 9 (2x) | 0 | 0 | (x86-only) |

## Library API

```cpp
// lib/include/ft/sh.h
namespace ft {
struct ShVertex { float x, y, z; };
struct ShFace {
    uint8_t  color;
    std::string texture;           // from last TextureFile before this face
    std::vector<uint32_t> indices; // 0-based pool indices
};
struct ShInfo {
    int   scale_raw;    // raw scale field (8 = 1 ft/unit)
    float scale;        // multiplier: raw_coord * scale = feet
    int   vert_count;
    int   face_count;
    float bbox[6];      // min_x min_y min_z max_x max_y max_z (feet)
    std::vector<std::string> textures;
};
struct ShMesh {
    float scale;
    std::vector<ShVertex>    vertices;
    std::vector<ShFace>      faces;
    std::vector<std::string> textures;
};
ShInfo      sh_parse_info(const uint8_t* data, size_t size);
ShMesh      sh_parse_mesh(const uint8_t* data, size_t size);
std::string sh_to_obj(const ShMesh& mesh);
}
```

## ft commands

```
ft sh info   <file.SH>               # scale, bounding box, vertex/face count, textures
ft sh unpack <file.SH> [-o out.obj]  # export Wavefront OBJ (with usemtl directives)
```

The exported OBJ uses `mtllib shape.mtl` and `usemtl <texture_name>` directives when
textures are present. A `.mtl` file is not written automatically -- create one manually
if needed for rendering.

## Known Limitations

- Faces embedded in x86 code blocks (65/1275 files) cannot be exported.
- Animation frames, LOD variants, and damage states are not distinguished -- all
  geometry from the main sequential stream is emitted into a single OBJ.
- OBJ -> SH is not implemented (inverse is too complex given animation/LOD/damage states).

## .PTS Extension

Community mod archives sometimes distribute aircraft shadow/crash shapes as `.PTS` files
(e.g. `A10.PTS`) rather than the in-LIB convention of `A10_S.SH`. The binary format is
identical — parse with the same SH parser. The `shadow_shape` ptr in the corresponding
`.PT` file points to the `_S.SH` name; the `.PTS` rename is a distribution artifact only.

## External Shape Editing (Community Tools)

The community uses two tools in combination to edit `.SH` vertex geometry:

- **FASHion** — a dedicated FA shape editor. It can only reposition individual vertices;
  it cannot add or remove vertices, change face topology, or alter the overall mesh
  structure. The rebuild operation overwrites the original file in place — always back up
  before editing.
- **SketchUp 8** — used as the 3D viewport. FASHion exports a vertex coordinate file
  that SketchUp loads via a plugin; after adjustments the modified coordinates are
  exported back and FASHion rebuilds the shape.

**Typical workflow:**

1. Unpack the target `.SH` from its `.LIB` with `ft lib unpack`.
2. Open the `.SH` in FASHion; export the vertex file.
3. Load into SketchUp 8 (install outside `Program Files` to avoid permissions issues).
4. Reposition vertices as needed.
5. Export from SketchUp; rebuild in FASHion → overwrites the `.SH`.
6. Repack with `ft lib patch`.

For bulk vertex edits (e.g. scaling an entire section), the community workflow converts
the vertex file to a spreadsheet, applies transformations numerically, then reconverts
before importing back into FASHion.

SH files with x86-only geometry (65/1275 in FA — see extraction results above) cannot
be edited with FASHion and require direct x86 disassembly for modification.

## Applications

Use `ft sh unpack` to export to Wavefront OBJ. There is no pack command — OBJ export
is for inspection and reference. For geometry edits, use the FASHion + SketchUp 8
community workflow described above.

- **Blender** — free, cross-platform; best option for inspecting and measuring exported OBJ geometry
- **MeshLab** — free, cross-platform; lightweight viewer with basic mesh statistics
- **FASHion** — free, FA-specific; vertex repositioning only (see workflow above)
- **SketchUp 8** — free (legacy version required by FASHion plugin); use alongside FASHion
- **3ds Max** `$` — paid; full mesh editing if a pack command is added in future
