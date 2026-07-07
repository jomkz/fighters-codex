# Library API

`fx_lib` is a static C++17 library. Link it from CMake by adding the repo root:

```cmake
add_subdirectory(fighters-codex)   # e.g. the extern/fx_lib submodule in fa-bridge
target_link_libraries(your_target PRIVATE fx::lib)
```

When embedded this way, only the library target is built — no CLI/GUI/tools/tests
and no test-framework fetch. The library is built position-independent, so it can
be linked into a shared plugin.

All public headers are under `lib/include/fx/`. Include them with the `fx/` prefix:

```cpp
#include "fx/ealib.h"
#include "fx/pic.h"
// etc.
```

## ealib.h — Archive container

```cpp
namespace fx {

struct Entry {
    char     name[13];   // null-terminated 8.3 filename
    uint8_t  flags;      // 0=raw, 1=lzss, 3=pxpk, 4=dcl
    uint32_t offset;     // absolute byte offset in the .LIB
    uint32_t size;       // compressed/raw size in the .LIB
};

// Read the directory from a memory-mapped .LIB
std::vector<Entry>   ealib_read_dir(const uint8_t* data, size_t size);

// Find an entry by name (ASCII case-insensitive); nullptr if not found
const Entry*         ealib_find(const std::vector<Entry>& entries,
                                 const std::string& name);

// Map an entry name to an output filename that is legal and identical on
// every platform: & * ? " < > | / \ : each become '_'
std::string          ealib_safe_name(const char* name);

// Extract one entry. With decompress=true, flags=0 is verbatim and flags=4 is
// blast-decompressed; flags 1 (LZSS) / 3 (PXPK) / unknown are unsupported —
// returns empty and sets *unsupported=true (decoders tracked in #54) rather
// than handing back still-compressed bytes. decompress=false returns the stored
// bytes for any flags. Decompressed-size claims above 64 MiB are rejected as
// malformed (empty return) — the size prefix is attacker-controlled.
std::vector<uint8_t> ealib_extract(const uint8_t* data, size_t size,
                                    const Entry& entry, bool decompress = true,
                                    bool* unsupported = nullptr);

// Build a new .LIB from a list of (name, data) pairs (stored uncompressed)
std::vector<uint8_t> ealib_build(
    const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files);

// Return a new .LIB with one entry replaced
std::vector<uint8_t> ealib_patch(const uint8_t* data, size_t size,
                                  const std::string& name,
                                  const std::vector<uint8_t>& new_data);

// Rebuild the container from its own directory (raw payloads, verbatim
// entry metadata, recomputed offsets incl. the terminator entry) —
// byte-identical for well-formed archives
std::vector<uint8_t> ealib_repack(const uint8_t* data, size_t size);
} // namespace fx
```

## pal.h — VGA palette

```cpp
namespace fx {

struct Palette {
    uint8_t r[256], g[256], b[256]; // already scaled to 8-bit (0–255)
};

Palette pal_load(const uint8_t* data, size_t size); // load a .PAL file
void    pal_save(const Palette& pal, uint8_t out[768]);
} // namespace fx
```

## pic.h — PIC image codec

```cpp
namespace fx {

struct PicInfo {
    uint16_t format;          // 0=dense, 1=sparse, 0xD8FF=JPEG
    uint32_t width, height;
    uint32_t pixels_offset, pixels_size;
    uint32_t palette_offset, palette_size;
    uint32_t spans_offset,   spans_size;
    uint32_t rowheads_offset, rowheads_size;
};

bool                 pic_info(const uint8_t* data, size_t size, PicInfo* info);

// Decode any PIC sub-format to RGBA8 (width * height * 4 bytes).
// sys_pal may be nullptr for JPEG or when the PIC has a full inline palette.
std::vector<uint8_t> pic_decode(const uint8_t* data, size_t size,
                                 const Palette* sys_pal);

// Encode RGBA8 to a dense PIC (format 0) with a full inline palette.
// Pixels with alpha < 128 become transparent (index 0xFF).
std::vector<uint8_t> pic_encode(const uint8_t* rgba, int w, int h,
                                 const Palette& pal);

// Byte-identical structural repack: re-derive every region from the parsed
// header and re-emit by construction (whole-file passthrough for JPEG).
// Returns empty if any byte is unaccounted for; a non-empty result is
// always byte-identical to the input.
std::vector<uint8_t> pic_repack(const uint8_t* data, size_t size);
} // namespace fx
```

## blast.h — PKWare DCL decompressor

```cpp
// Decompress a raw PKWare DCL stream (litmode=0, dictbits=4–6).
// Returns bytes written, or -1 on error.
int blast_decompress(const uint8_t* in, size_t in_size,
                     uint8_t* out, size_t out_capacity);

// EA wrapper: strips the 6-byte EA header before decompressing.
int blast_decompress_ea(const uint8_t* in, size_t in_size,
                        uint8_t* out, size_t out_capacity);
```

## seq.h — Cutscene timeline

```cpp
namespace fx {

struct SeqEvent {
    bool        relative;        // true = time is +N ticks from previous event
    int         ticks;
    std::string command;
    std::vector<std::string> args;
};

struct SeqFile {
    std::vector<std::string> header_comments;
    std::vector<SeqEvent>    events;
};

SeqFile              seq_parse(const uint8_t* data, size_t size);
std::vector<uint8_t> seq_serialize(const SeqFile&);
} // namespace fx
```

## audio.h — Raw PCM audio

```cpp
namespace fx {

struct AudioInfo {
    int    sample_rate;   // Hz
    size_t sample_count;
    double duration_sec;
};

AudioInfo            audio_info(const uint8_t* data, size_t size, int sample_rate);
std::vector<uint8_t> audio_to_wav(const uint8_t* data, size_t size, int sample_rate);
std::vector<uint8_t> audio_from_wav(const uint8_t* wav, size_t size,
                                     int* sample_rate_out);
} // namespace fx
```

## brf.h / ot.h — Type definitions

```cpp
namespace fx {

// ObjectType covers OT, NT, JT, SEE, ECM, GAS.
// PlaneType extends ObjectType with aerodynamic fields.
// Full field lists are in lib/include/fx/ot.h.

ObjectType           ot_parse(const uint8_t* data, size_t size);
PlaneType            pt_parse(const uint8_t* data, size_t size);
std::vector<uint8_t> ot_serialize(const ObjectType&);
std::vector<uint8_t> pt_serialize(const PlaneType&);
// nt_parse, jt_parse, see_parse, ecm_parse, gas_parse follow the same pattern
} // namespace fx
```

## mission.h — Mission and map files

```cpp
namespace fx {

struct MissionFile { /* map name, time, wind, clouds, object list */ };

MissionFile          mission_parse(const uint8_t* data, size_t size);
std::vector<uint8_t> mission_serialize(const MissionFile&);
} // namespace fx
```

## ai.h / bi.h — Compiled AI script codec

```cpp
namespace fx {

struct AiCompileError { int line; std::string message; };

// AI source (text) → BI Phar Lap PE DLL bytes. Empty on failure (see errors).
std::vector<uint8_t> ai_compile(const std::string& source,
                                std::vector<AiCompileError>& errors);

// BI bytecode → AI source (the inverse of ai_compile). The recovered text
// recompiles byte-identically for any BI this toolchain produced, i.e.
// ai_compile(ai_decompile(bi)) == bi. Reads the fx CALL_BY_NAME dialect only;
// returns "" on failure (no CODE section, or a foreign CALL_DIRECT dialect).
std::string ai_decompile(const uint8_t* data, size_t size);

struct BiInstr { uint32_t offset; std::string text; };

// Disassemble BI bytecode to mnemonics, resolving CALL_DIRECT thunks via
// .idata. Handles both fx-compiled and stock game BIs. Empty on failure.
std::vector<BiInstr> bi_disasm(const uint8_t* data, size_t size);
} // namespace fx
```

## sh.h — 3D shape / model

```cpp
namespace fx {

struct ShVertex { float x, y, z; };   // world coordinates, feet

struct ShTexCoord { float s, t; };  // texel-space (pixels of the referenced PIC)

struct ShFace {
    uint8_t  color;          // palette index for untextured rendering
    std::string texture;     // filename from last TextureFile instruction
    std::vector<uint32_t>    indices;    // 0-based into ShMesh::vertices
    std::vector<ShTexCoord>  texcoords;  // per-corner (parallel to indices); empty if untextured
};

struct ShInfo {
    int   scale_raw;         // raw scale field (8 = 1 ft/unit)
    float scale;             // multiplier: raw_coord * scale = feet
    int   vert_count, face_count;
    int   frame_count;       // animation frames (max JumpToFrame nframes); 0 = static
    int   lod_count;         // selectable LOD levels (1 = no distance LODs)
    bool  has_detail;        // any JumpToDetail preference switch present
    bool  has_damage;        // any inline JumpToDamage (0xAC) branch present
    float bbox[6];           // min_x min_y min_z max_x max_y max_z (feet)
    std::vector<std::string> textures;
};

struct ShMesh {
    float scale;
    int   frame_count = 0;   // animation frames; 0 = static
    int   lod_count   = 1;   // selectable LOD levels (1 = no JumpToLOD sites)
    bool  has_detail  = false;
    bool  has_damage  = false;
    std::vector<ShVertex>    vertices;
    std::vector<ShFace>      faces;
    std::vector<std::string> textures;
};

struct ShState {                 // selects a conditional-geometry state
    bool destroyed = false;      // JumpToDamage: emit the wreck sub-model
    int  frame     = 0;          // JumpToFrame: animation frame index (mod nframes)
    int  lod       = 0;          // JumpToLOD level: 0 = finest .. lod_count-1 = coarsest
    int  detail    = 0xFFFF;     // JumpToDetail preference; max = full detail
};

// Engine-generated sibling name: "A10.SH" + 'a' -> "A10_A.SH" ('a'-'d' =
// wreck variants, 's' = shadow; docs/fa/shape-selection.md). Which slots the
// engine fills depends on the type record's obj_class - probe which exist.
std::string sh_variant_name(const std::string& base, char variant);

ShInfo      sh_parse_info(const uint8_t* data, size_t size);
ShMesh      sh_parse_mesh(const uint8_t* data, size_t size);                        // intact
ShMesh      sh_parse_mesh(const uint8_t* data, size_t size, const ShState& state);  // state-aware
std::string sh_to_obj(const ShMesh& mesh);   // returns Wavefront OBJ text
} // namespace fx
```

## fbc.h — Video frame index

```cpp
namespace fx {
// Parse the flat u32le frame-size array (*ok=false if size % 4 != 0)
std::vector<uint32_t> fbc_read(const uint8_t* data, size_t size,
                               bool* ok = nullptr);

// Serialize — byte-identical inverse of fbc_read
std::vector<uint8_t> fbc_write(const std::vector<uint32_t>& frame_sizes);

// Byte offset of frame n inside the paired .VDO (816-byte header + prefix
// sum); n == frame count yields the expected VDO file size
uint64_t fbc_frame_offset(const std::vector<uint32_t>& frame_sizes, size_t n);
} // namespace fx
```

## bin.h — Lookup-table identification

```cpp
namespace fx {
enum class BinKind { Insigmap, Mix2, Mix2L, Mix4, Mix4L, VFontPal, Unknown };

// Classify by entry name (case-insensitive, .BIN optional)
BinKind bin_classify(const std::string& entry_name);

// One-line description / documented table size (0 for Unknown)
const char* bin_kind_desc(BinKind kind);
size_t bin_expected_size(BinKind kind);
} // namespace fx
```

## cam.h — Campaign DLL reader

```cpp
namespace fx {
struct CamInfo {
    bool        valid;  // MZ + "PL" signature with a CODE section
    CodeSection code;   // section geometry (pe.h)
};

CamInfo cam_info(const uint8_t* data, size_t size);

// Printable-ASCII runs >= min_len — the embedded campaign string tables
std::vector<std::string> cam_strings(const uint8_t* data, size_t size,
                                     size_t min_len = 3);
} // namespace fx
```

## txt.h — In-game text / directive engine

```cpp
namespace fx {
struct TxtLine {
    std::string raw;        // line bytes without the terminator
    bool crlf, terminated;
    std::vector<std::string> directives;  // ".section", "..button", ...
};
struct TxtDoc { std::vector<TxtLine> lines; };
enum class TxtKind { CampaignDescription, UiTemplate, PlainText };

TxtDoc txt_read(const uint8_t* data, size_t size);   // never fails
std::vector<uint8_t> txt_write(const TxtDoc& doc);   // byte-identical inverse
TxtKind txt_classify(const TxtDoc& doc);
size_t  txt_count(const TxtDoc& doc, const std::string& directive);
} // namespace fx
```

## cfg.h — EA.CFG game configuration

```cpp
namespace fx {
constexpr size_t   EA_CFG_SIZE  = 347;
constexpr uint32_t EA_CFG_MAGIC = 0x24;
struct EaCfg { /* every documented CONFIG field; three untraced
                  pass-through fields (#54) — see cfg.h */ };

// Engine-faithful validation: exact size + magic
bool cfg_read(const uint8_t* data, size_t size, EaCfg& out);
std::vector<uint8_t> cfg_write(const EaCfg& cfg);  // byte-identical inverse
} // namespace fx
```

## dat.h — CN_INFO network configuration

```cpp
namespace fx {
constexpr size_t DAT_FILE_SIZE = 3552;  // checksum + 0xDDC CN_INFO (v3)
struct CnInfo { /* typed documented fields; checksum + unmapped regions
                   pass through verbatim — see dat.h */ };

bool dat_read(const uint8_t* data, size_t size, CnInfo& out);
std::vector<uint8_t> dat_write(const CnInfo& info); // byte-identical inverse
const char* dat_transport_name(uint32_t transport);
unsigned    dat_baud_rate(uint32_t baud_index);
} // namespace fx
```

## mnu.h — Menu DLL reader

```cpp
namespace fx {
struct MnuInfo {
    bool        valid;  // MZ + "PL" signature with a CODE section
    CodeSection code;   // section geometry (pe.h)
};
MnuInfo mnu_info(const uint8_t* data, size_t size);
std::vector<std::string> mnu_strings(const uint8_t* data, size_t size,
                                     size_t min_len = 3);
} // namespace fx
```

## mt.h — Mission briefing text

```cpp
namespace fx {
struct MtInfo {
    std::string mission_id, source_name, title, mission_type;
    size_t sections;
};
// Parsing rides txt.h (same directive engine); this adds MT semantics
MtInfo mt_info(const TxtDoc& doc);
} // namespace fx
```

## pts.h — Aircraft screen-assets DLL reader

```cpp
namespace fx {
struct PtsInfo {
    bool        valid;  // MZ + "PL" signature with a CODE section
    CodeSection code;
    std::string icon;   // the single ICON*.PIC reference; empty if absent
};
PtsInfo pts_info(const uint8_t* data, size_t size);
} // namespace fx
```

## rgn.h — Installer UI region maps

```cpp
namespace fx {
struct RgnRecord { uint8_t name[4]; uint32_t vertex_count; uint32_t xy[8]; };
struct RgnFile   { std::vector<RgnRecord> records; };

bool rgn_read(const uint8_t* data, size_t size, RgnFile& out);
std::vector<uint8_t> rgn_write(const RgnFile& rgn);  // byte-identical inverse
std::string rgn_name(const RgnRecord& rec);
} // namespace fx
```

## ssf.h — Installer scripts

```cpp
namespace fx {
struct SsfStatement {
    size_t line;                    // index into the TxtDoc
    std::string keyword;            // e.g. "INSTALL_FILES"
    std::vector<std::string> args;  // unquoted argument values
};
struct SsfDoc { TxtDoc text; std::vector<SsfStatement> statements; };

SsfDoc ssf_read(const uint8_t* data, size_t size);   // never fails
std::vector<uint8_t> ssf_write(const SsfDoc& doc);   // byte-identical inverse
} // namespace fx
```

## mc.h / hgr.h — Mission condition & hangar DLL readers

```cpp
namespace fx {
struct McInfo  { bool valid; CodeSection code; };
McInfo mc_info(const uint8_t* data, size_t size);
std::vector<std::string> mc_strings(const uint8_t* data, size_t size,
                                    size_t min_len = 3);

struct HgrInfo {
    bool valid;
    CodeSection code;
    std::vector<std::string> pics;  // referenced *.PIC assets, in order
};
HgrInfo hgr_info(const uint8_t* data, size_t size);
std::vector<std::string> hgr_strings(const uint8_t* data, size_t size,
                                     size_t min_len = 3);
} // namespace fx
```

## dlg.h — Menu dialog DLL reader

```cpp
namespace fx {
struct DlgInfo {
    bool        valid;  // MZ + "PL" signature with a CODE section
    CodeSection code;   // control dispatch table (pe.h geometry)
};
DlgInfo dlg_info(const uint8_t* data, size_t size);
std::vector<std::string> dlg_strings(const uint8_t* data, size_t size,
                                     size_t min_len = 3);
} // namespace fx
```

## xmi.h — Extended MIDI (XMI → MID)

```cpp
namespace fx {
struct XmiChunk    { std::string tag; uint32_t offset, size; };
struct XmiSequence { std::vector<XmiChunk> chunks; uint16_t timbres; };
struct XmiFile     { bool valid; uint16_t seq_count;
                     std::vector<XmiSequence> sequences; };

// Parse the IFF envelope (FORM/XDIR + INFO + CAT XMID + per-seq FORM XMID)
XmiFile xmi_parse(const uint8_t* data, size_t size);

// Export one sequence to a Standard MIDI File (format 0); empty on error.
// One-way translation: AIL delays -> SMF deltas, note durations -> note-offs
std::vector<uint8_t> xmi_to_smf(const uint8_t* data, size_t size,
                                size_t seq_index, uint16_t ppqn = 60);
} // namespace fx
```

## mus.h — Music playlist sequencer

```cpp
namespace fx {

// One decoded sequencer instruction; fields carry meaning per `op`:
//   FF playlist id -> playlist_id · FA setup -> sub,value · FB play track ->
//   mode,track_idx,xmi · FC shuffle · FD jump -> value · FE branch -> value
struct MusOp     { uint32_t offset; uint8_t op, sub, mode, track_idx;
                   uint32_t value; std::string playlist_id, xmi; };
struct MusScript { bool valid; std::vector<MusOp> ops;
                   bool stopped_early; uint8_t stop_byte; };

// Map an XMI track index to its filename (1 -> VALK01.XMI, else AIRnnn.XMI).
std::string mus_xmi_name(uint8_t index);

// Disassemble the playlist bytecode from a .MUS DLL's CODE section.
// Read-only (the CODE section is Miles-consumed as-is; see #101).
// MusScript{valid=false} when there is no CODE section.
MusScript mus_disassemble(const uint8_t* data, size_t size);

} // namespace fx
```

## fnt.h — Font glyph compiler

```cpp
namespace fx {

struct FntGlyph { uint8_t ch; uint32_t width, height; std::vector<uint8_t> pixels; };
struct FntFile  { bool valid; uint32_t font_height, glyph_fn_va[256], glyph_width[256];
                  std::vector<FntGlyph> glyphs; };

// Parse the FONT struct from the PE DLL's CODE section.
FntFile fnt_parse(const uint8_t* data, size_t size);

// Interpret the x86 glyph functions into bitmaps (all 256 characters). The
// vocabulary — byte/word/dword run writes, row advance, RET — is complete
// across every install font.
void fnt_render_glyphs(FntFile& fnt, const uint8_t* cs_data, size_t cs_size, uint32_t cs_vma);

// Emit one glyph body with the original compiler's canonical encoding
// (greedy 4/2/1 pixel runs; byte-identical over all install bodies).
std::vector<uint8_t> fnt_emit_glyph(const uint8_t* pixels, uint32_t width, uint32_t height);

// Rebuild a FNT DLL around edited glyphs: bodies re-emitted in character
// order, the function-VA table rebuilt, the rest of the container carried
// verbatim. Empty if the code would overrun the original region.
std::vector<uint8_t> fnt_repack(const uint8_t* orig, size_t orig_size,
                                uint32_t height, const uint32_t widths[256],
                                const std::vector<FntGlyph>& glyphs);

} // namespace fx
```

## raw.h — Screenshot codec

```cpp
namespace fx {

struct RawInfo { uint32_t width, height; };

// Parse the mhwanh header (width/height u16 big-endian at +8/+10).
bool raw_info(const uint8_t* data, size_t size, RawInfo* info);

// Decode to width*height*4 RGBA through the embedded 8-bit palette.
std::vector<uint8_t> raw_decode(const uint8_t* data, size_t size);

// Encode RGBA to a RAW screenshot: palette rebuilt from distinct colours in
// first-seen order (max 256; alpha ignored). Empty on overflow.
std::vector<uint8_t> raw_encode(const uint8_t* rgba, int w, int h);

// Byte-identical structural repack; a non-empty result always equals the
// input. Trailing undescribed bytes fail.
std::vector<uint8_t> raw_repack(const uint8_t* data, size_t size);

} // namespace fx
```

## cb8.h — FMV video codec

```cpp
namespace fx {

struct Cb8Info {
    uint32_t width;
    uint32_t height;
    uint32_t frame_count;
    uint32_t samples_per_frame;  // sync counter ticks per frame (400)
    uint32_t audio_sync_rate;    // sync counter ticks per second (6000 = 400 x 15 fps)
};

// Parse VooM header from a CB8 file in memory.
bool cb8_info(const uint8_t* data, size_t size, Cb8Info* out);

struct Cb8Decoder;  // opaque

// Open a decoder. Returns nullptr on bad input. `data` must remain valid
// for the lifetime of the decoder.
Cb8Decoder* cb8_open(const uint8_t* data, size_t size);
void        cb8_close(Cb8Decoder* dec);

// Decode frame `frame_idx` to palette index bytes (width x height,
// row-major). Every frame is a self-contained key frame: frames decode in
// any order. Empty on error.
std::vector<uint8_t> cb8_decode_frame(Cb8Decoder* dec, uint32_t frame_idx);

// The frame's embedded 768-byte palette, widened 6->8 bit like pal_load.
bool cb8_frame_palette(Cb8Decoder* dec, uint32_t frame_idx, Palette* out);

// RGBA8 decode through the frame's embedded palette.
std::vector<uint8_t> cb8_decode_frame_rgba(Cb8Decoder* dec, uint32_t frame_idx);

// One re-encodable frame: indices plus the 6-bit palette to embed.
struct Cb8Frame {
    std::vector<uint8_t> indices;         // width * height
    std::array<uint8_t, 768> palette6{};  // 6-bit VGA RGB
};

// Rebuild a CB8 around new video frames. The DRBC header, audio chunks,
// stream order, and VooM timing carry from `orig` verbatim; each MRFI is
// re-encoded (pixel-exact; byte identity is a non-goal). Empty if a frame
// needs more than 256 codebook entries per band after splitting.
std::vector<uint8_t> cb8_repack(const uint8_t* orig, size_t orig_size,
                                const std::vector<Cb8Frame>& frames);

} // namespace fx
```

## hud.h — HUD overlay codec

```cpp
namespace fx {

struct HudParam { std::string gauge, field; int16_t value; };
struct HudFile  { bool valid; std::vector<std::string> asset_strings;
                  std::string icon_a, icon_b, icon_c, icon_d;
                  std::vector<HudParam> params; };

// Parse the fixed 0x2BB-byte HUD struct from the PE DLL's CODE section:
// asset strings, four advisory icon labels, and the named gauge parameters.
HudFile hud_parse(const uint8_t* data, size_t size);

// Rebuild a HUD DLL around edited gauge params and icon labels. `params`
// must hold exactly one entry per known gauge field (any order); icons fit
// their 8-byte slots. Asset strings and every unmodelled byte carry over
// verbatim — an unedited parse→repack is byte-identical. Empty on
// unknown/missing params, out-of-range values, or oversized labels.
std::vector<uint8_t> hud_repack(const uint8_t* orig, size_t orig_size,
                                const HudFile& hud);

} // namespace fx
```

## lay.h — Sky/atmosphere codec

```cpp
namespace fx {

struct LayGrad  { uint8_t r, g, b; };
struct LayLayer { uint8_t flags; int32_t sel_alt_min, /* ... */ gradient_val_end;
                  uint8_t base_rgb[3]; LayGrad zenith_grad[31], horizon_grad[32];
                  uint8_t horizon_base_rgb[3]; uint32_t fog_density;
                  std::string cloud_pic, sky_pic; uint8_t visibility; };
struct LayFile  { bool valid; uint32_t sky_angle_scale, below_angle_scale;
                  uint32_t sky_layer_va[10], below_layer_va[10];
                  uint32_t colour_entry_table_va, palette_buffer_va, layer_array_va;
                  std::vector<LayLayer> layers; };

// Parse the DLL data header and walk the LAYER array to its end sentinel
// (flags bit 0).
LayFile lay_parse(const uint8_t* data, size_t size);

// Rebuild a LAY DLL around edited header fields and layers. The layer
// count and each layer's sentinel bit must match the original, and the
// structural VAs (layer array, colour table, palette buffer) cannot be
// relocated; the sky/below band tables stay editable. cloud_pic/sky_pic
// fit their 22-byte slots. Unmodelled bytes carry over verbatim — an
// unedited parse→repack is byte-identical. Empty on any mismatch.
std::vector<uint8_t> lay_repack(const uint8_t* orig, size_t orig_size,
                                const LayFile& lay);

} // namespace fx
```

## t2.h — Terrain map

```cpp
namespace fx {

struct T2Info   { uint32_t dim_x, dim_y, tile_count, leaf_step,
                  leaves_w, leaves_h, leaf_offset, summary_offset;
                  std::map<uint8_t, uint32_t> surface_dist; };

// One 3-byte terrain record (leaf or tile summary).
struct T2Record { uint8_t surface_class;    // 0xFF = water
                  uint8_t elevation;        // elevation band
                  uint8_t texture_variant;  // 0-31 atlas sub-texture
};

struct T2Map    { std::string theater, atlas_pic;
                  uint32_t tiles_w, tiles_h, leaves_w, leaves_h, leaf_step;
                  std::vector<uint8_t> header;      // pre-payload bytes, verbatim
                  std::vector<T2Record> leaves;     // leaves_w*leaves_h row-major
                  std::vector<T2Record> summaries;  // tiles_w*tiles_h row-major
                  const T2Record& leaf(uint32_t x, uint32_t y) const;
                  const T2Record& summary(uint32_t col, uint32_t row) const; };

// Parse the header and build the per-tile surface class distribution.
bool t2_info(const uint8_t* data, size_t size, T2Info* info);

// Decode the full terrain map: header strings, the per-leaf records
// (surface class / elevation band / texture variant), and the per-tile
// summary array (the engine's far-LOD fallback). The raw header is kept
// verbatim, so header + records reassemble the file byte-identically.
// Returns false on any structural inconsistency.
bool t2_read(const uint8_t* data, size_t size, T2Map* map);

// Serialize a map back to file bytes: verbatim header + leaf array + summary
// array. The record vectors may be edited first but must still match the
// header's grid; empty on any mismatch. A t2_read -> t2_write round-trip is
// byte-identical.
std::vector<uint8_t> t2_write(const T2Map& map);

// Read then write — a byte-identical round-trip for valid T2 data, empty for
// anything t2_read rejects.
std::vector<uint8_t> t2_repack(const uint8_t* data, size_t size);

} // namespace fx
```

## plt.h — Pilot save

```cpp
namespace fx {

struct PltInfo  { uint8_t version_tag;
                  std::string name, callsign, voice_file, nose_art,
                              left_decal, right_decal, portrait, rank;
                  std::string cam_file, cam_name, aircraft;   // campaign block
                  std::vector<std::string> aircraft_pool, sensors;
                  std::vector<PltOrdnance>  ordnance; };

struct PltStats { /* mission counters, 13 kill tallies, 8 weapon-accuracy
                     groups — see plt.h for the full field list */ };

// A pilot file decoded for round-trip editing: `raw` is every original byte
// verbatim (the pass-through backbone); `info`/`stats` are decoded views over
// the mapped regions (`stats` valid only when `has_stats`).
struct PltFile  { std::vector<uint8_t> raw;
                  PltInfo info; PltStats stats; bool has_stats; };

// Parse the identity block (+ campaign scan). False if size < 0xB0 or the
// version tag != 0x0F.
bool plt_parse(const uint8_t* data, size_t size, PltInfo* info);

// Parse the confirmed stats block. False if size < 0x21F8 (stats not present).
bool plt_parse_stats(const uint8_t* data, size_t size, PltStats* stats);

// Read a pilot file: keep the full bytes in out->raw and decode the identity
// and stats views. Same validity criteria as plt_parse.
bool plt_read(const uint8_t* data, size_t size, PltFile* out);

// Serialize a pilot file: overlay only the fixed-offset mapped fields (the
// identity block, and the stats counters when has_stats) onto a copy of
// f.raw. The four unmapped gap regions and the variable-length
// campaign/ordnance region pass through verbatim, so a plt_read -> plt_write
// round-trip is byte-identical. Empty if f.raw is shorter than 0xB0.
std::vector<uint8_t> plt_write(const PltFile& f);

// Read then write — a byte-identical round-trip for a valid pilot file, empty
// for anything plt_read rejects.
std::vector<uint8_t> plt_repack(const uint8_t* data, size_t size);

} // namespace fx
```
