# Video Decode (Cobra: .CB8 + .VDO)

The **Cobra** full-motion-video framework — EA's in-house movie player, not licensed
middleware. A per-frame vector-quantization scheme: 2×2 codebooks rendered to 8/15/16/24
bpp with optional 2× pixel doubling. `0x456300–0x45CDA0`.

**Attribution corrected (#95):** Cobra is first and foremost **the [CB8](formats/CB8.md)
player** — `InitCobra` (`0x46ae10`) validates the `DRBC` container (rejecting the older
`ARBC`/`BRBC`/`CRBC` generations), streams it through the engine's own LIB layer, and the
8-bit paletted path (`DecodeFrame` submode 5 → `DecodeSVGA8Frame`/`DecodeDSVGA8Frame`,
`ExpandDB`/`EDB`, `CopySB8`/`CopyDB8`) is exactly the CB8 keyframe codec, with a 768-byte
palette embedded per frame. The 15/16/24-bit submode-6 paths are the hi-color
generalization. Note the **shipped `.VDO` corpus does *not* run through this
`DecodeFrame` dispatcher** — those 320×200 8bpp movies decode via a separate,
much smaller cluster (`GetVDOFrame` → `UnRLE` → `DecompressVideo`, `0x4C8Axx`),
documented in [VDO.md](formats/VDO.md) (#138). `DecodeFrame` here is the CB8
player. The compiled-in engine structs, the LIB-layer I/O, and the private generation
lineage mark the whole framework as homegrown (confirmed); the CB8 side is validated by
`fx_lib`'s pixel-exact codec (tests/test_cb8.cpp) and the on-disk layout is specified in
[CB8.md](formats/CB8.md).

> **Provenance:** Ghidra static analysis of the game executable with [FA.SMS](formats/SMS.md) symbols
> applied; recorded in the
> [symbol database](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/video.csv)
> and applied to the Ghidra project. Progress: [reconstruction matrix](reconstruction.md).
> Markers follow [spec-authoring.md](../spec-authoring.md): confirmed · inferred · unknown.
> Coordinates with the [VDO format spec](formats/VDO.md) (epic #55).

## Vector-quantized YUV, four output depths

Each frame carries a **codebook** of 256 entries, each `4 luma + 2 chroma` decoded to four
RGB pixels (a 2×2 block). `DecodeDBook` builds the book at 15/16-bit (`5:5:5` or `5:6:5`
chosen per frame), `Decode24Book` at 24-bit; the YUV→RGB conversion uses the per-movie
scale/offset constants and saturates each channel through `ClampU8` (the one recovered leaf
in the range — the color clamp called from every book decoder). The 8-bit path
(`DecodeSVGA8Frame` / `DecodeDSVGA8Frame`) renders paletted output with an interpolated 2×2
dither built by `EDB` (expand-book), optionally 2× doubled.

Frames are **key (intra)** or **delta (inter)** — the dispatch keys on the frame header,
and the inter decoders are gated on the keyframe latch. `.VDO` audio is a separate `.11K`
track, not interleaved in this cluster.

![Cobra decode: per-frame codebook (DBook/24Book/EDB) decoded to RGB via ClampU8, rendered at 8/15/16/24 bpp with optional 2x doubling.](diagrams/video-decode.svg)

## VDO container & Cobra lifecycle

Around the frame decoders sit two support layers the game already names. The **Cobra
lifecycle** brings the movie framework up and down against the `GlobalData` block —
`InitVideo`/`SetupCobra` on entry, `CleanVideo`/`CleanCobra`/`CleanupCobra` on teardown —
and `StartCobraSound`/`StopCobraSound` gate its audio. The **`.VDO` container layer** is
the 320×200 8bpp path (the shipped `.VDO` corpus, distinct from the `DecodeFrame` CB8
dispatcher): a `VDOLinkedList` of nodes read from disk, then decoded frame by frame.

| VA | Symbol | Role |
|----|--------|------|
| `0x46B120` | `InitVideo` | bring the video/Cobra framework up (`GlobalData`) |
| `0x46B4E0` | `SetupCobra` | configure the Cobra player |
| `0x46B0F0` | `CleanCobra` | release Cobra decode state |
| `0x46B4B0` | `CleanVideo` | tear the video framework down |
| `0x46B530` | `CleanupCobra` | final Cobra teardown |
| `0x4219D0` | `StartCobraSound` | start the movie's audio track |
| `0x4219B0` | `StopCobraSound` | stop the movie's audio track |
| `0x442360` | `InitMovieContext` | zero/size a `MovieContext` |
| `0x4AEE80` | `BuildVDOList` | build the `VDOLinkedList` for a `.VDO` file |
| `0x4AF100` | `NewVDOLinkNode` | append a node to the list |
| `0x4AF1B0` | `FreeVDOLinkNode` | free one list node |
| `0x4AF1E0` | `OpenVDOFile` | open the `.VDO` file |
| `0x4AF200` | `ReadVDOHeader` | read the `VDOHEADER` |
| `0x4AF230` | `ReadFrameSizesFile` | read the per-frame size table |
| `0x4AF2D0` | `ReadVDOPalette` | read the movie palette into `T_RGB[]` |
| `0x4AF3A0` | `AllocVDO` | allocate the `VDO` playback buffers |
| `0x4AF4B0` | `DeallocVDO` | free them |
| `0x4AF070` | `StartVDOAudio` | start the `.11K` audio track |
| `0x4C8CD8` | `DecompressVideoImage` | decompress one `.VDO` image (the 8bpp path) |

## Functions

Full record: [`db/symbols/video.csv`](https://github.com/jomkz/fighters-codex/blob/main/db/symbols/video.csv).

| VA | Symbol | Role |
|----|--------|------|
| `0x457230` | `DecodeDBook` | decode the 15/16-bit YUV codebook |
| `0x456300` | `DecodeDSVGA8Frame` | key frame → 8bpp paletted, 2× doubled |
| `0x456EC0` | `DecodeSVGA8Frame` | key frame → 8bpp paletted |
| `0x456AD0` | `EDB` | expand-book: interpolated 2×2 dither pattern |
| `0x4575E0` | `ClampU8` | saturate a channel to `[0,255]` (YUV→RGB leaf) |

## Open Questions

### 1. VDO.md corrections — resolved

The three [VDO.md](formats/VDO.md) inaccuracies are now reconciled ([#259](https://github.com/jomkz/fighters-codex/issues/259)):
the `DecodeFrame` dispatch keys are documented on the **FrameHeader** (`+8` kind, `+9` submode),
not the movie context; the **frame-kind polarity is corrected** (0 = key/intra, 1 = inter/delta);
and the `0xC14E` canvas pointer is attributed to `GlobalData`. The Cobra per-frame codec itself
(the ~45 leaf decoders) remains the long pole, tracked under epic #55.

*Status: resolved — re-static (#259; the codec long-pole stays under #55).*

## Related

- [formats/VDO.md](formats/VDO.md) — the `.VDO` container/codec spec.
- [formats/CB8.md](formats/CB8.md) — the related 8-bit codebook image format.
- [renderer.md](renderer.md) — `DrawAcrossBank`, the VGA-bank span helper the 8bpp path uses.
