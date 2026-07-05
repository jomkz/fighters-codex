#pragma once
#include <cstdint>
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

struct ShInfo {
    int   scale_raw;  // raw scale field from header (8 = 1 foot/unit)
    float scale;      // multiplier: raw_coord * scale = feet
    int   vert_count;
    int   face_count;
    int   frame_count;  // animation frames (max JumpToFrame nframes); 0 = static
    float bbox[6];    // min_x min_y min_z max_x max_y max_z (in feet)
    std::vector<std::string> textures;
};

struct ShMesh {
    float scale;
    int   frame_count = 0;  // animation frames; 0 = static
    std::vector<ShVertex>    vertices;
    std::vector<ShFace>      faces;
    std::vector<std::string> textures;
};

// Selects which conditional-geometry state the interpreter emits, for the
// state-aware viewer/exporter (see docs/fa/formats/SH.md § LOD and damage-state
// opcodes). Defaults reproduce the ordinary in-cockpit render (intact geometry).
struct ShState {
    bool destroyed = false;   // JumpToDamage (0xAC): show the wreck sub-model
    int  frame     = 0;       // JumpToFrame (0x40): animation frame index
};

ShInfo      sh_parse_info(const uint8_t* data, size_t size);
ShMesh      sh_parse_mesh(const uint8_t* data, size_t size);
ShMesh      sh_parse_mesh(const uint8_t* data, size_t size, const ShState& state);
std::string sh_to_obj(const ShMesh& mesh);

} // namespace fx
