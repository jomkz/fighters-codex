---
format: SH
name: Shape 3D Model
extensions: [".SH"]
category: 3d
endianness: little
spec:
  status: partial
  gaps:
    - kind: re-static
      issue: 52
      note: "render-state/attribute opcode semantics (sizes known, behavior untraced; animation, LOD/damage, dispatch, and X86Unknown families traced)"
codec:
  direction: read
  rationale: "OBJ export only; OBJ→SH is intentionally out of scope (animation/LOD/damage states make the inverse impractical — roadmap 1.0 definition)"
  lib: [lib/src/sh.cpp]
  commands: [sh]
  tests: [tests/test_sh.cpp]
  fuzz: []
  gui: [gui/src/editors/sh_editor.cpp]
  fixtures:
    synthetic: true
    real_manifest: true
related: [PIC, LIB, PT]
---

# SH — Shape 3D Model (.SH)

`.SH` files hold the 3D geometry for every aircraft, vehicle, weapon, and scene
object — 1275 of them in FA_2.LIB. Each is a **Phar Lap PE/LE executable**
whose CODE section carries a shape *bytecode* stream: vertex buffers, faces,
texture references, and conditional jumps the engine interprets at render time
(some models embed raw x86 code as well).

## Tools

### fx

```
fx sh info   <file.SH>               # scale, bounding box, vertex/face count, textures
fx sh unpack <file.SH> [-o out.obj]  # export Wavefront OBJ (with usemtl directives)
```

The exported OBJ uses `mtllib shape.mtl` and `usemtl <texture_name>` directives
when textures are present. A `.mtl` file is not written automatically — create
one manually if needed for rendering. The library API behind these commands
(`sh_parse_info` / `sh_parse_mesh` / `sh_to_obj`) is documented in
[api.md](../../api.md).

### Other Tools

- **Blender** — free, cross-platform; best option for inspecting and measuring exported OBJ geometry
- **MeshLab** — free, cross-platform; lightweight viewer with basic mesh statistics
- **FASHion** — free, FA-specific; vertex repositioning only (see workflow below)
- **SketchUp 8** — free (legacy version required by FASHion plugin); use alongside FASHion
- **3ds Max** `$` — paid; full mesh editing if a pack command is added in future

**Community editing workflow (FASHion + SketchUp 8):**

- **FASHion** can only reposition individual vertices; it cannot add or remove
  vertices, change face topology, or alter the overall mesh structure. The
  rebuild operation overwrites the original file in place — always back up
  before editing.
- **SketchUp 8** is used as the 3D viewport. FASHion exports a vertex
  coordinate file that SketchUp loads via a plugin; after adjustments the
  modified coordinates are exported back and FASHion rebuilds the shape.

1. Unpack the target `.SH` from its `.LIB` with `fx lib unpack`.
2. Open the `.SH` in FASHion; export the vertex file.
3. Load into SketchUp 8 (install outside `Program Files` to avoid permissions issues).
4. Reposition vertices as needed.
5. Export from SketchUp; rebuild in FASHion → overwrites the `.SH`.
6. Repack with `fx lib patch`.

For bulk vertex edits (e.g. scaling an entire section), the community workflow
converts the vertex file to a spreadsheet, applies transformations numerically,
then reconverts before importing back into FASHion.

SH files with x86-only geometry (65/1275 in FA — see Round-Trip Notes) cannot
be edited with FASHion and require direct x86 disassembly for modification.

## File Layout

All multi-byte integers are little-endian.

### Container: Phar Lap PE/LE Executable

SH files are **Phar Lap PE/LE executables**. The shape bytecode lives in the
CODE section. Parse via the standard MZ/PE header chain:

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

Take the **first section** with `PointerToRawData > 0` — that is the code
section. Always named `CODE`. `PointerToRawData` is the file offset;
`SizeOfRawData` is its length.

### Code Section Structure

```
[FF FF]              2-byte signature
[radius_world i16]   authored bounding radius, world units — engine-unused
[radius i16]         approximate bounding-sphere radius, shape units
[scale i16]          coordinate scale (bytes 6..8): see table below
[ext[0] i16]         bounding extent X (half-width)
[ext[1] i16]         bounding extent Y (half-depth)
[ext[2] i16]         bounding extent Z (half-height)
[instruction stream] variable-length opcodes
```

**radius** (bytes 4..6) is an authored, approximate bounding-sphere radius in
shape units: across all 1275 FA_2.LIB shapes it is always positive (min 10) and
correlates 0.996 with `‖ext‖₂`, sitting anywhere between the true max vertex
norm and the (loosely padded) header extents. The engine folds it into the
projection-shift computation, where only its magnitude (leading bit) matters —
see [Engine Notes](#engine-notes).

**radius_world** (bytes 2..4) is the same radius expressed in world units:
where nonzero it equals `radius * 2^(scale-8)` exactly in 93/134 files, with
authoring drift in the rest (ratios 1.4–4.8). It is zero in 1141/1275 shapes —
every aircraft and weapon — and nonzero only for ground/naval scenery (ships,
runways, cities, rocks, SAM sites). No the game executable code reads it: it is authoring
residue — see [Engine Notes](#engine-notes) for the tracing evidence.

**Scale table** (`world_coord_feet = raw_i16 * scale_factor`):

| scale field | scale_factor | Notes |
|-------------|--------------|-------|
| 7 | 0.5 | USNF97 and earlier only |
| 8 | 1.0 | Standard FA -- 1 unit = 1 foot |
| 9 | 2.0 | Large objects |
| 10 | 4.0 | Very large objects |
| 11 | 8.0 | Terrain features |
| 0 | 1.0 | Treated as 8 |

### Instruction Dispatch

Instructions are either **Byte-magic** (1-byte opcode) or **Word-magic**
(2-byte opcode: `[op_byte, 0x00]`). Dispatch is on the first byte.

**Key instructions:**

| First byte | Name | Total size | Notes |
|------------|------|------------|-------|
| `0xFF` | Header | 14 | Always first; scale at `[6..8]` |
| `0x00` | EndObject | all remaining | Triggers X86Unknown skip if `obj_end_off` set |
| `0x01` | EndShape | all remaining | Terminates the shape |
| `0x1E` | Pad | 1+ | Run of `0x1E` NOP bytes |
| `0x38` | ShortJump | 3 | `[38][rel16]`; see [Engine Notes](#animation-opcodes-traced) |
| `0xBC` | UnkBC | 2 | |
| `0xF0` | X86Code | variable | Bail out -- x86 machine code |
| `0xF6` | VertexInfo | 7 | `[F6][idx u16][color u8][normal i8[3]]` |
| `0xFC` | Face | variable | See face format section |

**Word-magic instructions (second byte = `0x00`):**

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
| `0x50` | LongJump | 6 |
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

### VertexBuffer (0x82 0x00)

Pushes a batch of vertices into the global vertex pool.

```
[82 00]           opcode (2 bytes)
[nverts u16]      number of vertices in this buffer
[push_at u16]     byte offset into the global pool where this buffer starts
[x y z i16 ...]   nverts * 3 signed 16-bit coordinates (LE)
```

**Pool index** = `push_at / 8`. Vertex slot size is 8 bytes in the engine's
pool (6 bytes of coords + 2 bytes alignment padding), so `push_at` is always a
multiple of 8. Face indices reference global pool indices.

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
| 7 | 0x80 | **Unknown** (Unk1) |
| 6 | 0x40 | HAVE_FACE_NORMAL |
| 5 | 0x20 | **Unknown** (Unk2 — brighter shading) |
| 4 | 0x10 | **Unknown** (Unk3 — perspective-correct mapping) |
| 3 | 0x08 | **Unknown** (Unk4) |
| 2 | 0x04 | HAVE_TEXCOORDS |
| 1 | 0x02 | FILL_BACKGROUND |
| 0 | 0x01 | **Unknown** (Unk5) |

**FaceLayoutFlags:**

| Bit | Mask | Meaning |
|-----|------|---------|
| 3 | 0x08 | **Unknown** (Unk0) |
| 2 | 0x04 | USE_SHORT_INDICES (u16 instead of u8) |
| 1 | 0x02 | USE_BYTE_FACE_CENTER (i8[3] instead of i16[3]) |
| 0 | 0x01 | USE_BYTE_TEXCOORDS (u8[2] instead of u16[2]) |

### X86Unknown Region

Some shapes embed native **x86 machine code** in the instruction stream, entered
by the `0xF0` X86Code opcode. These blocks are not procedural geometry
generators — they are **conditional selectors**: each reads a piece of live game
state and re-enters the bytecode interpreter on the sub-stream that matches. They
exist because the bytecode's own conditionals cannot read arbitrary engine
globals; the authoring tool emitted a small x86 `switch` instead. 208 of the
1275 FA_2.LIB shapes carry them.

The `fx` read codec bounds these regions (skip protocol below) and **recovers
the guarded sub-stream geometry via the PE relocation table** (sub-stream
harvest below) so articulated models render more completely; this section
specifies the **runtime contract** so the game-accurate, state-selected effect
can be reimplemented — see [Round-Trip Notes](#round-trip-notes) and fa-bridge#21
for the interpreter that reads live state, and
[#297](https://github.com/jomkz/fighters-codex/issues/297) for the codec-side
work remaining.

#### Skip protocol (read codec)

The parser bounds each region:

1. **PtrToObjEnd (0xF2)** seen: record `obj_end_off = offset_field`.
2. **EndObject (0x00)** seen while `current_pos < obj_end_off`: the range
   `[current_pos .. obj_end_off)` is the x86 region (native code plus the
   sub-stream geometry it guards). Skip to `obj_end_off` and continue.
3. **EndObject (0x00)** seen while `current_pos >= obj_end_off`: real end of
   object; stop parsing.

#### Sub-stream harvest (read codec, reloc-based)

Rather than dropping the guarded geometry, the codec recovers it without
executing the x86. Each x86 selector sets `esi` to a sub-stream via an internal
pointer that the **Phar Lap PE base-relocation table** fixes up, so the reloc
entries whose target lands inside the code section are the **exact** sub-stream
entry offsets (a byte scan would false-positive on x86 bytes — the relocations do
not). The codec (`lib/src/sh.cpp`, `collect_reloc_targets` + `harvest_target`):

1. Parses the `.reloc` table; keeps targets that point into the code section and
   are **not** `FF 25` trampolines (those reach the game executable's exports).
2. From each target, walks the geometry (VertexBuffer / Face / TextureFile, plus
   Pad/VertexInfo) and **follows `Unmask`/`UnmaskLong` sub-model calls** into
   their referenced sub-streams (bounded by a visited-set + recursion depth).
   Other control/attribute opcodes (LOD/detail jumps, `sh_op_78` bbox culls,
   render-state) are **skipped by size and the walk continues**, rather than
   stopping — so the full facet set of a sub-stream region is recovered, not just
   the run before its first jump. This is what makes a complete airframe appear
   (e.g. the A-10's left wing and the F-16's full planform); halting at the first
   jump left half the model's facets — which reference already-loaded pool
   vertices — uncollected.
3. Writes vertices **append-only** (never below the base pool count) so
   state-variant sub-streams that reuse low pool slots cannot corrupt the base
   mesh; faces reference the shared pool.

This yields the base mesh **plus every reloc-reachable sub-stream** — i.e. all
articulation states merged, not the single game-accurate state (default landing
gear, flap position, …). Selecting one state per selector from live `_PL*` values
is the remaining work ([#297](https://github.com/jomkz/fighters-codex/issues/297),
with [#295](https://github.com/jomkz/fighters-codex/issues/295) exposing the
state inputs); the case→sub-stream values are attributed to OpenFA, never
transcribed.

#### Entry contract (`0xF0` → native)

`do_start_asm` (`0x4D4254`), the `0xF0` handler, is two instructions:

```asm
push esi        ; esi = the bytecode pointer, now just past the F0 00 opcode —
ret             ; i.e. the address of the embedded x86 itself. RET jumps to it.
```

So the interpreter transfers to the payload by `push esi; ret`, entering native
execution **at the current stream position with `esi` pointing at the payload**.
The x86 inherits the interpreter's register and memory state (the shared vertex
pool, view matrix, and viewer-relative position globals used elsewhere in
[Engine Notes](#engine-notes)); it uses `esi` for position-independent access to
the sub-streams that follow it.

#### External references (trampolines)

The payload reaches the game executable globals and functions through **trampolines** —
6-byte indirect jumps `[FF 25][target u32]` whose `target` the shape's Phar Lap
PE relocations bind to a the game executable export **by name** at load time. Two kinds:

- **Inputs** — a global the block reads. Across the corpus these are dominated by
  the `_PL*` articulation-state block that
  [`ShapeSetup`](#engine-notes) initializes (independently confirmed: the same
  symbols appear as writers there and as trampoline reads here):

  | Trampoline | Shapes | Selects |
  |------------|-------:|---------|
  | `_PLgearDown` / `_PLgearPos` | 126 / 103 | landing-gear geometry |
  | `_PLrightFlap` / `_PLleftFlap` | 104 / 104 | flap geometry per side |
  | `_PLafterBurner` | 85 | exhaust/afterburner geometry |
  | `_PLbrake` | 75 | airbrake geometry |
  | `_PLrudder` | 73 | rudder deflection geometry |
  | `_PLhook` | 17 | arrestor-hook geometry |
  | `_PLswingWing` | 15 | variable-sweep wing geometry |
  | `_PLcanardPos` | 14 | canard geometry |
  | `_PLbayOpen` / `_PLbayDoorPos` | 10 / 6 | weapons-bay doors |
  | `_PLvtOn` / `_PLvtAngle` | 4 / 1 | thrust-vectoring nozzles |
  | `_PLslats` | 3 | leading-edge slats |
  | `_effects` / `_effectsAllowed` | 23 / 31 | render-effect gating |
  | `brentObjId`, `_SAMcount`, `@HARDNumLoaded@8` | 12, — , 4 | effect shapes (e.g. `FIRE.SH`): draw driven by object id / live counts |

- **Callback** — `do_start_interp` (`0x4D4240`), the bytecode entry. **All 208
  blocks reference it.** The payload sets `esi` to the selected sub-stream and
  jumps here to resume interpreting that geometry.

#### Runtime behavior (switch → re-enter)

Each block is therefore:

```
read  <input global>                 ; via an FF25 trampoline
switch on its value:
  case k0: esi = &substream_0
  case k1: esi = &substream_1
  ...
goto do_start_interp                 ; interpret the selected sub-stream
```

The value→variant mapping is per-shape (e.g. `_PLgearDown`: `0` = retracted,
`1` = extended, with `4` = strut on `F117.SH`). The exhaustive decode of those
case values is documented by the [OpenFA](https://gitlab.com/openfa/openfa)
project's `sh` crate (GPLv3) as a symbol→state table
(`_PLgearDown`/`_PLrightFlap`/`_PLslats`/`_PLbayOpen`/`_PLbrake`/`_PLhook`/…);
the *mechanism, trampoline inventory, and entry contract here are independently
derived* (Ghidra on the game executable plus a structural parse of all 1275 shapes) and the
per-shape case values are attributed to OpenFA per the license boundary — never
transcribed.

*Implementing this (fa-bridge#21, clean-room):* for each block, read the named
global, map its value to a sub-stream via the state tables, and interpret that
sub-stream. The original x86 need never execute — the switch is what matters.

#### Inventory

| Metric | Count | Source |
|--------|------:|--------|
| Shapes embedding x86 blocks (reference `do_start_interp`) | 208 | structural parse |
| …reading `_PL*` articulation state | 134 | structural parse |
| …reading effect state (`brentObjId`/`_SAMcount`/`@HARDNumLoaded@8`) | 12 | structural parse |
| Shapes producing **no** static OBJ geometry (x86 gates everything) | 65 | `fx` codec ([Round-Trip Notes](#round-trip-notes)) |

The 65 fully-gated shapes are procedural effects (`FIRE.SH`, `FLARE.SH`,
`DEBRIS.SH`, `EXP.SH`, `CLOUD*.SH`, …) and a few complex models (`AC130.SH`);
the rest are articulated aircraft whose base mesh extracts normally but whose
moving-part variants sit behind these switches. The evidence script is
[`AnalyzeSHX86.java`](https://github.com/jomkz/fighters-codex/blob/main/scripts/ghidra/AnalyzeSHX86.java).

### .PTS distribution files

Community mod archives sometimes distribute aircraft shadow/crash shapes as
`.PTS` files (e.g. `A10.PTS`) rather than the in-LIB convention of `A10_S.SH`.
The binary format is identical — parse with the same SH parser. The
`shadow_shape` ptr in the corresponding [.PT](PT.md) file points to the `_S.SH`
name; the `.PTS` rename is a distribution artifact only. (Unrelated to the
in-LIB **PTS overlay DLL** format documented in [PTS.md](PTS.md).)

### Cross-validation against OpenFA — audited

The instruction inventory above was audited against the
[OpenFA](https://gitlab.com/openfa/openfa) `sh` crate (GPLv3; commit
`7507fef5`, 2024-10-29). **Attribution:** the mnemonic names used here
(Header, Pad, Unmask, XformUnmask, JumpToFrame/Detail/Damage/LOD,
VertexBuffer, VertexInfo, SourceName, TextureFile/Index, PtrToObjEnd,
EndObject/EndShape, X86Code, and the `Unk*` scheme) originate from OpenFA's
reverse engineering. Facts are documented here with attribution; no code
crosses the license boundary — sizes and formulas were independently
re-validated by `fx_lib`'s parser walking all 1275 FA_2.LIB shapes with zero
errors.

**Agreement** — the two inventories are identical on every checkable fact:

- Same 55 opcodes; neither project knows an opcode the other lacks.
- All fixed sizes match, including the full `Unk*` set (0x08…0xEE).
- All variable-size formulas match: `0x06 = 16 + u16@[14]`,
  `0x0C/0x0E/0x10 = 12 + u16@[10]`, `0x40 = 4 + u16@[2]*2`,
  `0x42 = 2 + strlen + 1`, `0x82 = 6 + u16@[2]*6`, and the 0x6C flag arms
  (`0x38→13, 0x48→14, 0x50→16`).
- Face content/layout flag bits, the header layout, the scale table
  (including USNF97-only `7 → 0.5` and `0 → 1.0`), and the
  `push_at % 8 == 0` vertex-pool constraint all match.

**Recorded differences** (both inert for our export-only parser):

| Topic | This spec | OpenFA | Note |
|---|---|---|---|
| `0xF0` magic class | byte-magic | word-magic (`F0 00`) | both immediately delegate to x86 handling, so no parse divergence; adjudicated: the engine has no magic-class concept at all — dispatch is uniform `vector_table[opcode*2]` (see [Engine Notes](#engine-notes)) |
| `EndObject` extent | consumes all remaining input, with the [PtrToObjEnd/EndObject skip protocol](#x86unknown-region) for x86 regions | 18-byte errata heuristic, because OpenFA parses *through* x86 regions via trampolines instead of skipping them | different models of the same stream behavior; ours is validated by the 1275/1275 walk |

Where this spec now exceeds OpenFA: the two header fields OpenFA marks
unknown ("probably super important, but I don't know what they mean") are
resolved as `radius` / `radius_world` by engine tracing — see
[Engine Notes](#engine-notes). OpenFA's in-source hypotheses for still-open
opcodes (`0x6C`/`0xC8` as low-detail fast-forwards; the Unmask family as
manual backface culling) are recorded as leads for the #52 tracing work, not
as facts.

## Engine Notes

Shape opcodes that branch on entity state are handled by the game executable functions.
Confirmed from FA.SMS:

| VA | Symbol | Description |
|----|--------|-------------|
| `0x4D22D4` | `do_ifdestroyed` | Tests whether the entity is in destroyed state; skips or follows a conditional branch in the shape bytecode stream |
| `0x4D057C` | `GRAddBrentObj` | Registers a shape instance for rendering: consumes the header (`radius`, `scale`), culls, computes the projection shift, and queues a render-sort record whose stream pointer starts at header+0xe |

### Header field consumption (traced)

`GRAddBrentObj(shape, x, y, z)` is the only the game executable code that touches the raw
14-byte header:

- **`scale` (+6)** shifts the viewer-relative Δx/Δy/Δz into shape units.
- **`radius` (+4)** is OR-ed with those shifted magnitudes before the
  `_shift_table` leading-bit lookup that selects the projection/precision
  shift — flooring the shift so the whole shape stays in 16-bit range — and is
  copied into the render-sort record (+0x10). Only the leading bit matters,
  which is why authored values need not be exact radii.
- **`radius_world` (+2) is never read.** Evidence: the interpreter receives
  the stream pointer already advanced to header+0xe; a scan of every
  decompiled the game executable function found no 16-bit or 32-bit read of header+2
  co-occurring with other header-field access; and a raw-listing scan of the
  hand-written interpreter region (0x4CD000–0x4D7000, including 5342
  instructions outside Ghidra functions) shows all 467 `[reg+0x2]` accesses
  are word-magic operand fetches and zero negative-displacement reads that
  could reach back from the stream start
  ([`AnalyzeSHHeader.java`](https://github.com/jomkz/fighters-codex/blob/main/scripts/ghidra/AnalyzeSHHeader.java)).

### Interpreter dispatch — vector_table (traced)

The shape interpreter is hand-written **threaded code**: there is no central
decode loop. Every handler ends by fetching and dispatching the next
instruction inline (150+ dispatch sites across 0x4C9581–0x4D6F42):

```asm
mov   ax, [esi]                  ; AL = opcode, AH = first operand byte
lea   esi, [esi+2]               ; consume 2 bytes
movzx ebx, al
jmp   [vector_table + ebx*2]     ; 4-byte entries at opcode*2
```

`vector_table` (the FA.SMS name; `.data`, `0x5183A0`) holds 128 dword handler
pointers addressed at `opcode*2` — so **only even opcodes dispatch cleanly**,
and the parser-side byte-magic/word-magic distinction dissolves at engine
level: every fetch consumes `[op][first-operand-byte]`, and handlers whose
operands start at byte 1 back up themselves (the 3-byte `0x38` handler begins
with `DEC ESI`). `0xFF`/`0x01` never reach the table in well-formed streams
(the header is consumed at queue time by `GRAddBrentObj`).

Handler symbols recovered from FA.SMS (the full 128-entry map is reproduced by
[`AnalyzeSHDispatch.java`](https://github.com/jomkz/fighters-codex/blob/main/scripts/ghidra/AnalyzeSHDispatch.java),
which also materializes the handlers as Ghidra functions):

| Op | Spec name | Handler | FA.SMS symbol |
|----|-----------|---------|---------------|
| `0x1E` | Pad | `0x4D17F4` | `do_short_eof` |
| `0x38` | ShortJump | `0x4D30E4` | `do_short_ijmp` — `DEC ESI`, then falls into the `0x48` Jump handler |
| `0x40` | JumpToFrame | `0x4D3134` | `do_anim_jmp` |
| `0x42` | SourceName | `0x4D17BC` | `do_shape_name` |
| `0x44` | Unk44 | `0x4D42EC` | `do_setcoarse` |
| `0x46` | Unk46 | `0x4D478C` | `do_force_no_pmap` |
| `0x50` | LongJump | `0x4D3100` | `do_ijmp_long` — `[50 00][rel32]`, unconditional `esi += rel32` |
| `0x6E` | UnmaskLong | `0x4D22A8` | `do_sfcal_long` |
| `0xA6` | JumpToDetail | `0x4D2318` | (unnamed) — skips `rel16` when detail global `0x515EEE` ≥ operand threshold |
| `0xAC` | JumpToDamage | `0x4D22D4` | `do_ifdestroyed` — `esi += rel16` when `_destroyed` (`0x50C39C`, set per entity by `ShapeSetup`) is non-zero |
| `0xB2` | UnkB2 | `0x4D2344` | `do_use_terrain_detail` |
| `0xB8` | UnkB8 | `0x4D22FC` | `do_no_overlap` |
| `0xC6` | XformUnmaskLong | `0x4D33D8` | `do_icall_long` |
| `0xC8` | JumpToLOD | `0x4D416C` | `do_jumpfar4` — skips its 6-byte operand when flag `0x515EF0 & 0x20000`; otherwise computes a view-space depth dot product for the distance test |
| `0xCE` | UnkCE | `0x4D47A4` | `do_streamer_def` |
| `0xD0` | UnkD0 | `0x4D47B8` | `do_streamer_draw` |
| `0xD2` | UnkD2 | `0x4D4894` | `do_screen_coords` |
| `0xDA` | UnkDA | `0x4D42C8` | `do_setlight` |
| `0xE4` | UnkE4 | `0x4D4A47` | `do_brush_area` |
| `0xE6` | UnkE6 | `0x4D4A6D` | `do_brush_area_full` |
| `0xE8` | UnkE8 | `0x4D5475` | `do_new_smap` |
| `0xEA` | UnkEA | `0x4D5644` | `do_new_rmap` |
| `0xEE` | UnkEE | `0x4D4A30` | `do_brush_trans` |
| `0xF0` | X86Code | `0x4D4254` | `do_start_asm` |
| `0xF2` | PtrToObjEnd | `0x4D4258` | `do_collision_info` |
| `0xF6` | VertexInfo | `0x4D4308` | `do_set_point_color` |
| `0xFC` | Face | `0x4D43DC` | `do_new_poly` |

Engine-only opcodes (handlers exist; absent from the 1275-file FA corpus):
`0x34` → `do_nop`, `0x36`/`0x3E` → `do_new_pmap_or_tmap`, `0x5C` →
`do_setcolor2`, `0xEC` → `do_brush_solid`, `0xF4` → `do_set_gouraud`,
`0xF8` → `do_drawobj000`, `0xFA` → `do_fullpntg16`, `0xFE` → `do_nt`.
Six slots (`0x14`, `0x16`, `0x3C`, `0xA8`, `0xAA`, `0xC0`) share
`do_if_not_effect` — a conditional skip keyed on the effects setting — and
ten unassigned slots point at a common stub (`0x4D17E0`).

These handler names are FA.SMS facts. The animation and LOD/damage families
are specified below.

### Animation opcodes (traced)

Shapes animate by **free-running frame selection** against a single global
counter — there is no per-entity animation clock, start trigger, or authored
playback rate.

**`0x40` JumpToFrame — `do_anim_jmp` (0x4D3134).** Layout
`[40 00][nframes u16][rel16 × nframes]` (total `4 + nframes*2`, matching the
skip-table formula). Runtime:

```
idx    = _frameCounter mod nframes
target = &frame_table[idx] + (int16)frame_table[idx]   ; rel16 is relative
                                                        ; to its own slot
```

`_frameCounter` (`0x4EB738`) is a global incremented once per rendered frame in
the main flight loop (`FlyingLoop`, `0x404C70`) as
`_frameCounter = (_frameCounter + 1) & 0x7FFF`, and reset to 0 on screen
transitions. Because selection is `_frameCounter mod nframes`, every animated
model is **phase-locked to the same counter**: a shape with `nframes = 8`
advances one frame per rendered frame and repeats every 8 frames, independent
of which entity draws it. Used by 510/1275 shapes (5610 opcodes).

*Playback (for fa-bridge#19):* keep a render-tick counter masked to 15 bits; at
each JumpToFrame compute `counter % nframes`, read that slot's signed `rel16`,
and branch to `slot_address + rel16`.

**Control-flow primitives** (shared by the animation and LOD/damage families —
each advances the instruction pointer `esi`; `rel` is signed and, except where
noted, relative to the end of its own operand):

| Op | Name | Layout | Behavior | Corpus |
|----|------|--------|----------|--------|
| `0x48` | Jump (`0x4D30E5`) | `[48 00][rel16]` | `esi += rel16` | 533 files |
| `0x38` | ShortJump — `do_short_ijmp` (`0x4D30E4`) | `[38][rel16]` (3 bytes) | identical to Jump; handler does `DEC ESI` then shares the `0x48` body | 724 files |
| `0x50` | LongJump — `do_ijmp_long` (`0x4D3100`) | `[50 00][rel32]` | `esi += rel32` | **0** (engine-only) |

*Sub-model draw (articulated parts).* `Unmask` (`0x12`, 728 files) and
`UnmaskLong` (`0x6E`, unused) invoke a referenced sub-stream via the dispatch
table's **call** form (`FF 14`, versus `FF 24` for the normal tail jump) with
`esi` pushed, so the sub-stream renders and control resumes after the opcode.
`XformUnmask` (`0xC4`, 2 files) / `XformUnmaskLong` (`0xC6`, unused) first save
the view matrix and object-position globals and apply the operand offsets to the
object position, so the sub-stream renders at a relative transform — the
attached-part mechanism. Their per-operand layout is only partially traced and
stays in [Open Questions](#1-remaining-unk-opcode-semantics); they are not part
of frame playback.

### LOD and damage-state opcodes (traced)

Three conditional branches choose which geometry block renders. Each jumps
**forward** to an alternative block when its condition selects it; the
fall-through (default) block is the primary geometry. All `rel16` are signed and
relative to the end of the operand.

**`0xAC` JumpToDamage — `do_ifdestroyed` (0x4D22D4).** Layout `[AC 00][rel16]`.

```
if (_destroyed != 0) esi += rel16     ; branch to the damaged sub-model
                                       ; else fall through to intact geometry
```

`_destroyed` (`0x50C39C`) is set per entity by `ShapeSetup` (`0x4AB450`) as the
shape is queued: it is `(health_word == 0)` read from the object record (`+0xE`),
with a `_forceDestroyed` override forcing 1. So the damaged geometry is authored
inline after the opcode and reached only for destroyed entities. Rare — 62/1275
shapes (65 opcodes), almost all ground/naval targets (e.g. `BARKSA.SH`).

**`0xA6` JumpToDetail (0x4D2318).** Layout `[A6 00][rel16][threshold u16]`.

```
if (_detail < threshold) esi += rel16  ; branch to the lower-detail block
                                        ; else fall through to full detail
```

A **static quality switch** on the user's detail preference `_detail`
(`0x515EEE`), independent of distance. Near-universal: 1094/1275 shapes.

**`0xC8` JumpToLOD — `do_jumpfar4` (0x4D416C).** Layout
`[C8 00][size u16][pixel_threshold u16][rel16]` (6-byte operand → total 8).
**Distance-based.** When the force-max-detail flag `_effects & 0x20000` is set,
the operand is skipped and the block always renders; otherwise the handler
compares the object's projected on-screen size against the threshold:

```
depth     = -(xv·m3 + yv·m6 + zv·m9)              ; view-space Z, clamped >= 20
projected = (size * aspecty * scrh) / depth / 2   ; ~ vertical pixels on screen
threshold = (pixel_threshold * sizeAdjust) >> 8
if (projected < threshold) esi += rel16           ; too small -> next (coarser) LOD
                                                   ; else fall through to this LOD
```

with `xv/yv/zv` (`0x51CDAA/AC/AE`) the object's viewer-relative position,
`m3/m6/m9` (`0x515F48/4E/54`) the view-matrix depth row, `aspecty`
(`0x51837C`), `scrh` (`0x5182F4`) the screen height, and `sizeAdjust`
(`0x51D125`) a global tuning factor. LOD blocks chain: each JumpToLOD renders
its block when the object is large enough on screen, else jumps past it to the
next, coarser block. Near-universal: 1098/1275 shapes (2064 opcodes).

### State-selected rendering (read codec)

`sh_parse_mesh(data, size, ShState{})` interprets the animation and damage
branches to emit **one** state's geometry, so the viewer can show a specific
frame or the wreck rather than every block merged. `ShState` selects the state:

- **`destroyed`** — follows `0xAC` JumpToDamage: `false` (default) falls through
  to the intact geometry; `true` takes the branch into the damaged sub-model.
- **`frame`** — the `0x40` JumpToFrame index. The codec computes
  `idx = frame mod nframes` per opcode and branches to that slot (the same
  `slot_address + rel16` the interpreter uses, with `frame` standing in for
  `_frameCounter`), and reports the model's animation length as
  `ShMesh::frame_count` (the max `nframes` seen; `0` = static).

Frame selection is applied both to the **base sequential stream** (where most
aircraft's control-surface/rotor animation lives — 511/1275 FA_2.LIB shapes) and
inside the **x86-gated sub-streams** recovered by the reloc harvest: `0x40` in a
sub-stream selects the frame's block and the harvester keeps walking there (the
frame blocks are only reachable through that jump, never through a reloc), so a
handful more shapes animate and previously-dropped frame-0 geometry is recovered
(5 FA_2.LIB shapes gain a face at frame 0; vertices unchanged, none lost). Some
`0x40` tables remain gated behind control flow the harvester doesn't follow
(e.g. the F-16's), and those shapes stay static (`frame_count = 0`). The `fxs`
preview exposes both a **Destroyed** toggle and, when `frame_count > 1`, a
**Frame** slider (docs/gui.md).

## Round-Trip Notes

The codec is deliberately export-only: OBJ→SH is out of scope because a shape
is a bytecode *program* (animation frames, LOD variants, damage states,
embedded x86), not a plain mesh — regenerating one from a static OBJ would
discard the behavioral stream.

Extraction coverage, tested against all 1275 `.SH` files from FA_2.LIB:

| Result | Count | % |
|--------|-------|---|
| Vertices + faces extracted | 1257 | 98.6% |
| No geometry (no OBJ output) | 18 | 1.4% |
| Parser crash / error | 0 | 0% |

The remaining **no-geometry files** are pure procedural effects that emit their
geometry entirely from x86 (no static VertexBuffer/Face at all): `FIRE.SH`,
`FLARE.SH`, `BULLET.SH`, `CHAFF.SH`, `CLOUD*.SH`, `CRATER.SH`, `DEBRIS.SH`,
`EXP.SH`, `EJECT.SH`, etc. (Complex airframes that were previously x86-only, such
as `AC130.SH`, now recover their facets via the walk-through harvest above.)

**Shadow models** (`*_S.SH`): flat ground silhouettes, Z=0, typically 6-20 faces.

**Sample results:**

| File | Scale | Verts | Faces | Textures |
|------|-------|-------|-------|----------|
| A10.SH | 8 (1x) | 425 | 418 | _a10.PIC |
| A10_B.SH | 8 (1x) | 71 | 98 | _a10_b.PIC |
| A10_S.SH | 8 (1x) | 21 | 6 | (none) |
| F22.SH | 8 (1x) | 476 | 422 | |
| F15E.SH | 8 (1x) | 483 | 647 | |
| AC130.SH | 9 (2x) | 283 | 488 | |

**Texture coordinates.** Faces with `HAVE_TEXCOORDS` carry one `(s, t)` per
corner (`ShFace::texcoords`, parallel to `indices`); the codec extracts them in
**texel space** (origin top-left, pixels of the referenced PIC) since the shape
does not record its texture's dimensions. `sh_to_obj` emits them as `vt` lines
with `f v/vt` faces; a consumer normalizes by the PIC's width/height (and flips
V) for a 0..1 sampler. Example: `A10.SH` — 40/122 faces textured, `s ∈ [0,251]`,
`t ∈ [24,284]` against `_a10.PIC`.

Further limitations: the OBJ export merges every block — animation frames, LOD
variants, and damage states are not distinguished. The **in-memory** parse can
select a single animation frame or the damaged sub-model via `ShState` (see
[State-selected rendering](#state-selected-rendering-read-codec)); that
selection is not plumbed through `sh_to_obj`.

## Open Questions

### 1. Remaining Unk* opcode semantics

All opcodes have confirmed sizes (the parser walks every FA shape without
error). The control-flow families are now traced — animation (`0x40`, `0x48`,
`0x38`, `0x50`; see [Animation opcodes](#animation-opcodes-traced)) and
LOD/damage (`0xA6`, `0xAC`, `0xC8`; see
[LOD and damage-state opcodes](#lod-and-damage-state-opcodes-traced)) — as is
the [dispatch mechanism](#interpreter-dispatch--vector_table-traced) that named
a dozen render-state handlers. What remains untraced is the **render-state /
attribute** set (`do_setcoarse`, `do_setlight`, the `do_brush_*` and
`do_new_smap`/`do_new_rmap` family, the streamer pair, and the still-unnamed
`Unk06…UnkEE` word-magic entries), the exact per-operand layout of the
`Xform`/`Unmask` sub-model calls (`0xC4`/`0xC6`/`0x12`/`0x6E`), and the unknown
Face flag bits. These affect surface appearance, not geometry or the
animation/LOD/damage playback contract.

*Status: open — re-static (#52)*

### 2. x86-embedded geometry regions

The runtime contract of these regions is specified in
[X86Unknown Region](#x86unknown-region): the `0xF0 → push esi; ret` entry, the
`FF25` trampoline reads of `_PL*`/effect globals, and the `do_start_interp`
re-entry that selects a geometry sub-stream. What remains is not a spec gap but
a **codec limitation**: the static `fx` read path skips these regions, so the
65 fully-gated shapes still export no OBJ (see [Round-Trip Notes](#round-trip-notes)),
and the exhaustive per-shape case-value tables are carried with attribution to
OpenFA rather than re-derived here.

*Status: specified — implementable by fa-bridge#21 (closed #125)*

## Related

**Formats:** [PIC](PIC.md) — `_`-prefixed skin textures referenced by
`TextureFile`; [LIB](LIB.md) — container (FA_2.LIB ×1275); [PT](PT.md) —
flight-model records whose `shadow_shape` field names the `_S.SH` shadow
shapes.

**Engine:** [shape-selection.md](../shape-selection.md) — how the engine picks
*which* `.SH` to draw (the whole-model damage swap and per-class `_A`…`_D`
variant set), the inter-shape counterpart to this file's intra-shape LOD/damage
opcodes; [renderer.md](../renderer.md) — the shape interpreter and
rasterizer pipeline; [architecture.md](../architecture.md) — Phar Lap overlay
loading.
