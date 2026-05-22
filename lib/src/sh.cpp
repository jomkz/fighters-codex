#include "fx/sh.h"
#include <algorithm>
#include <climits>
#include <cstring>
#include <sstream>

namespace fx {

// ---- PE/LE header parsing -----------------------------------------------

static uint16_t u16le(const uint8_t* p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t u32le(const uint8_t* p) {
    return (uint32_t)(p[0] | ((uint32_t)p[1] << 8) |
                      ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

struct CodeSection { const uint8_t* data; size_t size; };

// Returns the first section's raw data (the code section) from an MZ/PE or MZ/LE binary.
// FA SH files use Phar Lap LE format with "PL\0\0" where PE uses "PE\0\0".
static CodeSection find_code_section(const uint8_t* data, size_t size) {
    if (size < 0x40 || data[0] != 'M' || data[1] != 'Z')
        return {nullptr, 0};
    uint32_t pe_off = u32le(data + 0x3C);
    if (pe_off + 24 + 2 > size)
        return {nullptr, 0};
    const uint8_t* pe = data + pe_off;
    if (pe[0] != 'P' || pe[2] != 0 || pe[3] != 0)
        return {nullptr, 0};
    // COFF header at pe+4
    uint16_t num_sec    = u16le(pe + 6);
    uint16_t opt_hdr_sz = u16le(pe + 20);
    uint32_t sec_table  = pe_off + 24 + opt_hdr_sz;
    for (uint16_t i = 0; i < num_sec; ++i) {
        uint32_t sec_off = sec_table + (uint32_t)i * 40;
        if (sec_off + 40 > size) break;
        const uint8_t* sec = data + sec_off;
        uint32_t raw_sz  = u32le(sec + 16);
        uint32_t raw_ptr = u32le(sec + 20);
        if (raw_sz > 0 && raw_ptr + raw_sz <= size)
            return {data + raw_ptr, raw_sz};
    }
    return {nullptr, 0};
}

// ---- scale factor -------------------------------------------------------

// Header layout: [FF FF][unk0 i16][unk1 i16][scale i16][ext[3] i16]  = 14 bytes
static float read_scale(const uint8_t* code, size_t sz) {
    if (sz < 14 || code[0] != 0xFF) return 1.0f;
    int16_t s = (int16_t)u16le(code + 6);
    switch (s) {
    case 7:  return 0.5f;
    case  0: // treat as 8
    case 8:  return 1.0f;
    case 9:  return 2.0f;
    case 10: return 4.0f;
    case 11: return 8.0f;
    default: return 1.0f;
    }
}

static int16_t read_scale_raw(const uint8_t* code, size_t sz) {
    if (sz < 14 || code[0] != 0xFF) return 8;
    return (int16_t)u16le(code + 6);
}

// ---- instruction skip table ---------------------------------------------

// Returns the byte size of the instruction at p (including opcode),
// or 0 if unknown / error.
static size_t instr_skip(const uint8_t* p, size_t avail) {
    if (avail == 0) return 0;
    uint8_t op = p[0];

    // Byte-magic opcodes (no zero padding byte)
    switch (op) {
    case 0x00: return 0; // EndObject: handled by caller
    case 0x01: return 0; // EndShape: handled by caller
    case 0x1E: {          // Pad: run of 0x1E
        size_t n = 1;
        while (n < avail && p[n] == 0x1E) ++n;
        return n;
    }
    case 0x38: return 3;
    case 0xBC: return 2;
    case 0xF0: return 0; // X86Code: bail
    case 0xF6: return 7;
    case 0xFC: return 0; // Face: handled by caller
    case 0xFF: return 14;
    default: break;
    }

    // Word-magic opcodes ([op, 0x00, ...])
    if (avail < 2) return 0;
    switch (op) {
    case 0x06: return (avail >= 16) ? (size_t)(16 + u16le(p + 14)) : 0;
    case 0x08: return 4;
    case 0x0C: return (avail >= 12) ? (size_t)(12 + u16le(p + 10)) : 0;
    case 0x0E: return (avail >= 12) ? (size_t)(12 + u16le(p + 10)) : 0;
    case 0x10: return (avail >= 12) ? (size_t)(12 + u16le(p + 10)) : 0;
    case 0x12: return 4;
    case 0x2E: return 4;
    case 0x3A: return 6;
    case 0x40: return (avail >= 4) ? (size_t)(4 + (size_t)u16le(p + 2) * 2) : 0;
    case 0x42: { // SourceName: [42 00] + null-terminated string
        size_t n = 2;
        while (n < avail && p[n] != 0) ++n;
        return (n < avail) ? n + 1 : 0;
    }
    case 0x44: return 4;
    case 0x46: return 2;
    case 0x48: return 4;
    case 0x4E: return 2;
    case 0x50: return 6;
    case 0x66: return 10;
    case 0x68: return 8;
    case 0x6C:
        if (avail >= 11) {
            switch (p[10]) {
            case 0x38: return 13;
            case 0x48: return 14;
            case 0x50: return 16;
            }
        }
        return 0;
    case 0x6E: return 6;
    case 0x72: return 4;
    case 0x76: return 10;
    case 0x78: return 12;
    case 0x7A: return 10;
    case 0x82: return (avail >= 4) ? (size_t)(6 + (size_t)u16le(p + 2) * 6) : 0;
    case 0x96: return 6;
    case 0xA6: return 6;
    case 0xAC: return 4;
    case 0xB2: return 2;
    case 0xB8: return 4;
    case 0xC4: return 16;
    case 0xC6: return 18;
    case 0xC8: return 8;
    case 0xCA: return 4;
    case 0xCE: return 40;
    case 0xD0: return 4;
    case 0xD2: return 8;
    case 0xDA: return 4;
    case 0xDC: return 12;
    case 0xE0: return 4;
    case 0xE2: return 16;
    case 0xE4: return 20;
    case 0xE6: return 10;
    case 0xE8: return 6;
    case 0xEA: return 8;
    case 0xEE: return 2;
    case 0xF2: return 4;
    default:   return 0;
    }
}

// ---- face parser --------------------------------------------------------

static size_t parse_face(const uint8_t* p, size_t avail, ShFace& out, const std::string& cur_tex) {
    if (avail < 6) return 0;
    uint8_t content = p[1];
    uint8_t layout  = p[2];
    out.color   = p[3];
    out.texture = cur_tex;
    out.indices.clear();

    bool have_normal = (content & 0x40) != 0;
    bool have_tex    = (content & 0x04) != 0;
    bool short_idx   = (layout  & 0x04) != 0;
    bool byte_center = (layout  & 0x02) != 0;
    bool byte_tex    = (layout  & 0x01) != 0;

    size_t off = 5;

    if (have_normal) {
        off += 6; // face normal i16[3]
        if (off > avail) return 0;
        off += byte_center ? 3 : 6; // face center
        if (off > avail) return 0;
    }

    if (off >= avail) return 0;
    size_t nidx = p[off++];

    size_t idx_bytes = short_idx ? nidx * 2 : nidx;
    if (off + idx_bytes > avail) return 0;
    for (size_t i = 0; i < nidx; ++i) {
        uint32_t idx;
        if (short_idx) { idx = u16le(p + off); off += 2; }
        else           { idx = p[off++]; }
        out.indices.push_back(idx);
    }

    if (have_tex) {
        size_t tex_bytes = byte_tex ? nidx * 2 : nidx * 4;
        off += tex_bytes;
        if (off > avail) return 0;
    }

    return off;
}

// ---- code walker --------------------------------------------------------

static void walk_code(const uint8_t* code, size_t code_sz,
                      float scale_factor,
                      std::vector<ShVertex>& vpool,
                      std::vector<ShFace>& faces,
                      std::vector<std::string>& textures) {
    size_t off         = 0;
    size_t obj_end_off = SIZE_MAX;
    std::string cur_tex;

    while (off < code_sz) {
        const uint8_t* p    = code + off;
        size_t         avail = code_sz - off;
        uint8_t        op   = p[0];

        if (op == 0x01) break; // EndShape

        if (op == 0x00) {
            // EndObject: skip X86Unknown region if obj_end_off is set
            if (obj_end_off != SIZE_MAX && off < obj_end_off) {
                off = obj_end_off;
            } else {
                break;
            }
            continue;
        }

        if (op == 0xF0) break; // X86Code: stop

        if (op == 0xFC) {
            ShFace face;
            size_t sz = parse_face(p, avail, face, cur_tex);
            if (sz == 0) break;
            if (!face.indices.empty())
                faces.push_back(std::move(face));
            off += sz;
            continue;
        }

        if (op == 0x82) {
            // VertexBuffer: push into global pool at push_at/8
            if (avail < 6) break;
            uint16_t nverts   = u16le(p + 2);
            uint16_t push_at  = u16le(p + 4);
            size_t   pool_idx = push_at / 8;
            size_t   data_sz  = 6 + (size_t)nverts * 6;
            if (data_sz > avail) break;
            size_t needed = pool_idx + nverts;
            if (needed > vpool.size())
                vpool.resize(needed, {0.f, 0.f, 0.f});
            for (size_t i = 0; i < nverts; ++i) {
                size_t vo = 6 + i * 6;
                int16_t x = (int16_t)u16le(p + vo);
                int16_t y = (int16_t)u16le(p + vo + 2);
                int16_t z = (int16_t)u16le(p + vo + 4);
                vpool[pool_idx + i] = { x * scale_factor,
                                        y * scale_factor,
                                        z * scale_factor };
            }
            off += data_sz;
            continue;
        }

        if (op == 0xE2) {
            // TextureFile: [E2 00] + 14-byte null-padded name
            if (avail < 16) break;
            std::string name;
            for (size_t i = 2; i < 16 && p[i] != 0; ++i)
                name += (char)p[i];
            if (!name.empty()) {
                cur_tex = name;
                bool found = false;
                for (auto& t : textures) if (t == name) { found = true; break; }
                if (!found) textures.push_back(name);
            }
            off += 16;
            continue;
        }

        if (op == 0xF2) {
            // PtrToObjEnd: absolute code-section offset at [2..4]
            if (avail < 4) break;
            obj_end_off = u16le(p + 2);
            off += 4;
            continue;
        }

        // All other instructions: skip via size table
        size_t sz = instr_skip(p, avail);
        if (sz == 0) break;
        off += sz;
    }
}

// ---- public API ---------------------------------------------------------

ShMesh sh_parse_mesh(const uint8_t* data, size_t size) {
    ShMesh mesh{};
    auto cs = find_code_section(data, size);
    if (!cs.data) return mesh;

    mesh.scale = read_scale(cs.data, cs.size);
    walk_code(cs.data, cs.size, mesh.scale,
              mesh.vertices, mesh.faces, mesh.textures);
    return mesh;
}

ShInfo sh_parse_info(const uint8_t* data, size_t size) {
    ShInfo info{};
    auto cs = find_code_section(data, size);
    if (!cs.data) return info;

    info.scale_raw = read_scale_raw(cs.data, cs.size);
    info.scale     = read_scale(cs.data, cs.size);

    // Read extent from header bytes [8..14] for bbox estimate
    if (cs.size >= 14 && cs.data[0] == 0xFF) {
        float ext[3] = {
            (int16_t)u16le(cs.data + 8)  * info.scale,
            (int16_t)u16le(cs.data + 10) * info.scale,
            (int16_t)u16le(cs.data + 12) * info.scale,
        };
        info.bbox[0] = -ext[0]; info.bbox[3] = ext[0];
        info.bbox[1] = -ext[1]; info.bbox[4] = ext[1];
        info.bbox[2] = -ext[2]; info.bbox[5] = ext[2];
    }

    // Full parse for counts
    ShMesh mesh = sh_parse_mesh(data, size);
    info.vert_count = (int)mesh.vertices.size();
    info.face_count = (int)mesh.faces.size();
    info.textures   = mesh.textures;

    // Refine bbox from actual vertices if available
    if (!mesh.vertices.empty()) {
        float mn[3], mx[3];
        mn[0] = mx[0] = mesh.vertices[0].x;
        mn[1] = mx[1] = mesh.vertices[0].y;
        mn[2] = mx[2] = mesh.vertices[0].z;
        for (auto& v : mesh.vertices) {
            mn[0] = std::min(mn[0], v.x); mx[0] = std::max(mx[0], v.x);
            mn[1] = std::min(mn[1], v.y); mx[1] = std::max(mx[1], v.y);
            mn[2] = std::min(mn[2], v.z); mx[2] = std::max(mx[2], v.z);
        }
        for (int i = 0; i < 3; ++i) {
            info.bbox[i]     = mn[i];
            info.bbox[i + 3] = mx[i];
        }
    }
    return info;
}

std::string sh_to_obj(const ShMesh& mesh) {
    std::ostringstream ss;
    ss << "# Generated by fighters-toolkit\n";
    if (!mesh.textures.empty())
        ss << "mtllib shape.mtl\n";
    ss << '\n';

    for (auto& v : mesh.vertices)
        ss << "v " << v.x << " " << v.y << " " << v.z << '\n';

    std::string cur_tex;
    for (auto& f : mesh.faces) {
        if (f.indices.empty()) continue;
        if (f.texture != cur_tex) {
            cur_tex = f.texture;
            if (!cur_tex.empty())
                ss << "usemtl " << cur_tex << '\n';
        }
        ss << 'f';
        for (uint32_t idx : f.indices)
            ss << ' ' << (idx + 1); // OBJ is 1-based
        ss << '\n';
    }
    return ss.str();
}

} // namespace fx
