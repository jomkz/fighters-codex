#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace fx {

struct ShVertex { float x, y, z; };

// Texel-space texture coordinate (origin top-left, in pixels of the referenced
// PIC). Normalize by the texture's width/height for a 0..1 sampler; the SH bytes
// carry raw texels because the shape does not know its texture's dimensions.
struct ShTexCoord { float s, t; };

struct ShFace {
    uint8_t  color;
    std::string texture;
    std::vector<uint32_t> indices; // 0-based into ShMesh::vertices
    // Per-corner texel coordinates, parallel to `indices` (same count) when the
    // face carries HAVE_TEXCOORDS; empty for untextured faces.
    std::vector<ShTexCoord> texcoords;
};

// An articulation input a shape's embedded x86 selectors read to choose a
// moving-part state (gear/flaps/hook/…). `input` is the `_PL*` engine global;
// `values` are the distinct compare cases the shape's OWN selectors test,
// ascending — recovered clean-room from the shape's bytes (docs/fa/formats/SH.md
// § X86Unknown Region). The meaning of each value is documented externally.
struct ShArticulation {
    std::string      input;   // e.g. "_PLgearDown", "_PLleftFlap"
    std::vector<int> values;  // distinct selector cases, ascending
};

struct ShInfo {
    int   scale_raw;  // raw scale field from header (8 = 1 foot/unit)
    float scale;      // multiplier: raw_coord * scale = feet
    int   vert_count;
    int   face_count;
    int   frame_count;  // animation frames (max JumpToFrame nframes); 0 = static
    int   lod_count;    // selectable LOD levels (1 = no distance LODs)
    bool  has_detail;   // any JumpToDetail (0xA6) preference switch present
    bool  has_damage;   // any JumpToDamage (0xAC) inline damage branch present
    float bbox[6];    // min_x min_y min_z max_x max_y max_z (in feet)
    std::vector<std::string> textures;
    std::vector<ShArticulation> articulations; // x86 moving-part selectors (#295)
};

struct ShMesh {
    float scale;
    int   frame_count = 0;   // animation frames; 0 = static
    int   lod_count   = 1;   // selectable LOD levels (1 = no 0xC8 JumpToLOD sites)
    bool  has_detail  = false; // any 0xA6 JumpToDetail present
    bool  has_damage  = false; // any 0xAC JumpToDamage present
    std::vector<ShVertex>    vertices;
    std::vector<ShFace>      faces;
    std::vector<std::string> textures;
    std::vector<ShArticulation> articulations; // x86 moving-part selectors (#295)
};

// Selects which conditional-geometry state the interpreter emits, for the
// state-aware viewer/exporter (see docs/fa/formats/SH.md § LOD and damage-state
// opcodes). Defaults reproduce the ordinary close-up render (intact geometry,
// full detail, finest LOD).
struct ShState {
    bool destroyed = false;   // JumpToDamage (0xAC): show the wreck sub-model
    int  frame     = 0;       // JumpToFrame (0x40): animation frame index
    // JumpToLOD (0xC8) level: 0 = finest .. ShMesh::lod_count-1 = coarsest. The
    // engine picks by projected on-screen size; level k stands in for a size
    // just below the k-th largest pixel threshold in the shape.
    int  lod       = 0;
    // JumpToDetail (0xA6) preference (the engine's `_detail` word): a site
    // branches to its lower-detail block when detail < its threshold. Default
    // max keeps every full-detail block.
    int  detail    = 0xFFFF;
    // Chosen value per articulation input (ShArticulation::input -> value). An
    // input left unset renders every state of that selector merged (the codec's
    // whole-airframe default); setting it emits only the matching sub-stream.
    std::map<std::string, int> articulation;
};

// Derive an engine-generated sibling shape name for a base shape name:
// variant 'a'..'d' = the wreck/damage-swap models SetupOT builds at type-load
// time, 's' = the shadow shape (docs/fa/shape-selection.md). "A10.SH" + 'a'
// -> "A10_A.SH". Which slots the engine actually fills depends on the type
// record's obj_class, which the shape alone does not carry — callers probe
// which siblings exist (e.g. in the same LIB). Returns "" for an invalid
// variant letter or an empty stem.
std::string sh_variant_name(const std::string& base, char variant);

ShInfo      sh_parse_info(const uint8_t* data, size_t size);
ShMesh      sh_parse_mesh(const uint8_t* data, size_t size);
ShMesh      sh_parse_mesh(const uint8_t* data, size_t size, const ShState& state);
std::string sh_to_obj(const ShMesh& mesh);

// The articulation inputs a shape's x86 selectors read, with the distinct
// selector cases each tests (#295). Empty for non-articulated shapes.
std::vector<ShArticulation> sh_articulations(const uint8_t* data, size_t size);

} // namespace fx
