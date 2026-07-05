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

// Extract one entry (decompress if decompress=true and flags=4).
// Decompressed-size claims above 64 MiB are rejected as malformed (empty
// return) — the size prefix is attacker-controlled in a crafted archive
std::vector<uint8_t> ealib_extract(const uint8_t* data, size_t size,
                                    const Entry& entry, bool decompress = true);

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

## cb8.h — FMV video decoder

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

// Open a decoder for sequential frame access.  Returns nullptr on bad input.
// `data` must remain valid for the lifetime of the decoder.
Cb8Decoder* cb8_open(const uint8_t* data, size_t size);
void        cb8_close(Cb8Decoder* dec);

// Decode frame `frame_idx` and return palette index bytes (width x height),
// row-major, top-to-bottom.  Seeking backward resets the canvas to frame 0.
// Returns empty on error or out-of-range frame_idx.
std::vector<uint8_t> cb8_decode_frame(Cb8Decoder* dec, uint32_t frame_idx);

} // namespace fx
```

For best performance iterate frames in forward order; backward seeks replay
from frame 0.
