#include "fx/sh.h"
#include <algorithm>
#include <climits>
#include <cstring>
#include <sstream>
#include <utility>

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

// ---- PE relocations (x86 sub-stream discovery, #297) --------------------

struct PeSec { uint32_t rva, raw_ptr, raw_sz, vsize; };
struct PeInfo {
    bool ok = false;
    uint32_t image_base = 0;
    uint32_t code_rva = 0;               // rva of the code section
    uint32_t reloc_rva = 0, reloc_size = 0;
    std::vector<PeSec> secs;
};

static PeInfo parse_pe(const uint8_t* data, size_t size) {
    PeInfo pe{};
    if (size < 0x40 || data[0] != 'M' || data[1] != 'Z') return pe;
    uint32_t pe_off = u32le(data + 0x3C);
    if ((size_t)pe_off + 24 > size) return pe;
    const uint8_t* h = data + pe_off;
    if (h[0] != 'P' || h[2] != 0 || h[3] != 0) return pe;
    uint16_t num_sec = u16le(h + 6);
    uint16_t opt_sz  = u16le(h + 20);
    const uint8_t* opt = h + 24;
    if (opt + 32 > data + size) return pe;
    pe.image_base = u32le(opt + 28);                  // PE32 ImageBase
    if (opt_sz >= 96 + 6 * 8 && opt + 96 + 6 * 8 <= data + size) {
        pe.reloc_rva  = u32le(opt + 96 + 5 * 8);       // DataDirectory[5] = .reloc
        pe.reloc_size = u32le(opt + 96 + 5 * 8 + 4);
    }
    uint32_t sec_table = pe_off + 24 + opt_sz;
    bool have_code = false;
    for (uint16_t i = 0; i < num_sec; ++i) {
        uint32_t so = sec_table + (uint32_t)i * 40;
        if ((size_t)so + 40 > size) break;
        const uint8_t* s = data + so;
        PeSec ps{ u32le(s + 12), u32le(s + 20), u32le(s + 16), u32le(s + 8) };
        pe.secs.push_back(ps);
        if (!have_code && ps.raw_sz > 0 && (size_t)ps.raw_ptr + ps.raw_sz <= size) {
            pe.code_rva = ps.rva;
            have_code = true;
        }
    }
    pe.ok = have_code;
    return pe;
}

static bool rva_to_file(const PeInfo& pe, uint32_t rva, size_t size, size_t& out) {
    for (const auto& s : pe.secs) {
        uint32_t span = std::max(s.vsize, s.raw_sz);
        if (rva >= s.rva && rva < s.rva + span) {
            size_t fo = (size_t)s.raw_ptr + (rva - s.rva);
            if (fo <= size) { out = fo; return true; }
        }
    }
    return false;
}

// (site_code_off, target_code_off) for base relocations whose fixed-up value
// points into the code section — i.e. the internal `mov esi, <sub-stream>`
// pointers the x86 selectors use. Both offsets are code-section-relative.
static std::vector<std::pair<uint32_t, uint32_t>>
collect_reloc_targets(const uint8_t* data, size_t size, const PeInfo& pe, size_t code_sz) {
    std::vector<std::pair<uint32_t, uint32_t>> out;
    if (!pe.ok || !pe.reloc_rva || !pe.reloc_size) return out;
    size_t rbase;
    if (!rva_to_file(pe, pe.reloc_rva, size, rbase)) return out;
    size_t rend = std::min(rbase + pe.reloc_size, size);
    size_t p = rbase;
    while (p + 8 <= rend) {
        uint32_t page = u32le(data + p);
        uint32_t blk  = u32le(data + p + 4);
        if (blk < 8 || p + blk > rend) break;
        size_t nent = (blk - 8) / 2;
        for (size_t e = 0; e < nent; ++e) {
            uint16_t ent = u16le(data + p + 8 + e * 2);
            if ((ent >> 12) != 3) continue;             // IMAGE_REL_BASED_HIGHLOW
            uint32_t fixup_rva = page + (ent & 0xFFF);
            size_t ffo;
            if (!rva_to_file(pe, fixup_rva, size, ffo) || ffo + 4 > size) continue;
            uint32_t target_rva = u32le(data + ffo) - pe.image_base;
            if (fixup_rva  < pe.code_rva || fixup_rva  - pe.code_rva >= code_sz) continue;
            if (target_rva < pe.code_rva || target_rva - pe.code_rva >= code_sz) continue;
            out.push_back({ fixup_rva - pe.code_rva, target_rva - pe.code_rva });
        }
        p += blk;
    }
    return out;
}

// ---- scale factor -------------------------------------------------------

// Header layout: [FF FF][radius_world i16][radius i16][scale i16][ext[3] i16]  = 14 bytes
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
    out.texcoords.clear();

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
        if (off + tex_bytes > avail) return 0;
        out.texcoords.reserve(nidx);
        for (size_t i = 0; i < nidx; ++i) {
            float s, t;
            if (byte_tex) { s = p[off];            t = p[off + 1];        off += 2; }
            else          { s = u16le(p + off);    t = u16le(p + off + 2); off += 4; }
            out.texcoords.push_back({s, t});
        }
    }

    return off;
}

// ---- code walker --------------------------------------------------------

static void walk_code(const uint8_t* code, size_t code_sz,
                      float scale_factor,
                      std::vector<ShVertex>& vpool,
                      std::vector<ShFace>& faces,
                      std::vector<std::string>& textures,
                      bool destroyed, int frame, int& frame_count) {
    size_t off         = 0;
    size_t obj_end_off = SIZE_MAX;
    std::string cur_tex;

    while (off < code_sz) {
        const uint8_t* p    = code + off;
        size_t         avail = code_sz - off;
        uint8_t        op   = p[0];

        // JumpToFrame (0x40): [40 00][nframes u16][rel16 x nframes]. Select the
        // frame's block: idx = frame mod nframes; each rel16 is relative to its
        // own slot (SH.md). frame_count exposes the animation length.
        if (op == 0x40 && avail >= 4 && p[1] == 0x00) {
            uint16_t nframes = u16le(p + 2);
            size_t   table   = 4;                          // rel16 table start
            if (nframes == 0 || table + (size_t)nframes * 2 > avail) { off += 4; continue; }
            if ((int)nframes > frame_count) frame_count = (int)nframes;
            size_t idx    = (size_t)(frame % (int)nframes);
            size_t slot   = off + table + idx * 2;
            size_t target = slot + (size_t)(int16_t)u16le(code + slot);
            off = (target < code_sz) ? target : (off + table + (size_t)nframes * 2);
            continue;
        }

        // JumpToDamage (0xAC): branch to the inline damaged sub-model only for
        // destroyed entities; intact geometry is the fall-through (SH.md).
        if (op == 0xAC && avail >= 4 && p[1] == 0x00) {
            off += destroyed ? (size_t)(4 + (int16_t)u16le(p + 2)) : 4;
            continue;
        }

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

// Parse a run of geometry bytecode starting at an x86-selected sub-stream entry
// (a relocation target). Entries are exact — no false positives — so we walk
// VertexBuffer/Face/TextureFile, follow Unmask sub-model calls, and stop at the
// first other control opcode. Appends into the shared pool via push_at.
// `visited` (one byte per code offset) bounds recursion and prevents loops.
static void harvest_target(const uint8_t* code, size_t code_sz, size_t start,
                           float scale_factor, std::vector<ShVertex>& vpool,
                           std::vector<ShFace>& faces, std::string cur_tex,
                           size_t base_count, std::vector<char>& visited, int depth,
                           int frame, int& frame_count) {
    if (depth > 16) return;
    size_t off = start;
    size_t budget = 4096;  // a sub-stream is small; bound the walk
    while (off < code_sz && budget--) {
        if (visited[off]) return;   // already harvested / loop guard
        visited[off] = 1;
        const uint8_t* p = code + off;
        size_t avail = code_sz - off;
        uint8_t op = p[0];
        if (op == 0x00 || op == 0x01 || op == 0xF0) return;  // terminators

        // Sub-model call: draw a referenced sub-stream, then resume after it.
        if (op == 0x12 && avail >= 4 && p[1] == 0x00) {       // Unmask [12 00][rel16]
            size_t tgt = off + 4 + (int16_t)u16le(p + 2);
            if (tgt < code_sz)
                harvest_target(code, code_sz, tgt, scale_factor, vpool, faces,
                               cur_tex, base_count, visited, depth + 1, frame, frame_count);
            off += 4;
            continue;
        }
        if (op == 0x6E && avail >= 6 && p[1] == 0x00) {       // UnmaskLong [6E 00][rel32]
            size_t tgt = off + 6 + (int32_t)u32le(p + 2);
            if (tgt < code_sz)
                harvest_target(code, code_sz, tgt, scale_factor, vpool, faces,
                               cur_tex, base_count, visited, depth + 1, frame, frame_count);
            off += 6;
            continue;
        }

        // JumpToFrame (0x40) inside a sub-stream: select the frame's block and
        // keep walking there. This is how the animation of aircraft whose frame
        // table lives in an x86-gated sub-stream (e.g. the F-16) is reached —
        // the frame blocks are only referenced by this jump, never by a reloc.
        if (op == 0x40 && avail >= 4 && p[1] == 0x00) {       // [40 00][nframes u16][rel16 x nframes]
            uint16_t nframes = u16le(p + 2);
            if (nframes == 0 || 4 + (size_t)nframes * 2 > avail) { off += 4; continue; }
            if ((int)nframes > frame_count) frame_count = (int)nframes;
            size_t idx    = (size_t)(frame % (int)nframes);
            size_t slot   = off + 4 + idx * 2;
            size_t target = slot + (size_t)(int16_t)u16le(code + slot);
            off = (target < code_sz) ? target : (off + 4 + (size_t)nframes * 2);
            continue;
        }

        if (op == 0x82 && avail >= 6 && p[1] == 0x00) {
            uint16_t nv = u16le(p + 2), pa = u16le(p + 4);
            size_t dsz = 6 + (size_t)nv * 6;
            if (nv == 0 || nv > 512 || dsz > avail) return;
            size_t pool_idx = pa / 8, needed = pool_idx + nv;
            // Append-only: never overwrite the base mesh (state variants reuse
            // low pool slots; merging them statically would corrupt the base).
            if (pool_idx < base_count) { off += dsz; continue; }
            if (needed > vpool.size()) vpool.resize(needed, {0.f, 0.f, 0.f});
            for (size_t i = 0; i < nv; ++i) {
                size_t vo = 6 + i * 6;
                vpool[pool_idx + i] = {
                    (int16_t)u16le(p + vo)     * scale_factor,
                    (int16_t)u16le(p + vo + 2) * scale_factor,
                    (int16_t)u16le(p + vo + 4) * scale_factor };
            }
            off += dsz;
            continue;
        }
        if (op == 0xFC) {
            ShFace f;
            size_t fs = parse_face(p, avail, f, cur_tex);
            if (fs == 0 || f.indices.empty()) return;
            faces.push_back(std::move(f));
            off += fs;
            continue;
        }
        if (op == 0xE2 && avail >= 16) {
            std::string name;
            for (size_t i = 2; i < 16 && p[i] != 0; ++i) name += (char)p[i];
            if (!name.empty()) cur_tex = name;
            off += 16;
            continue;
        }
        if (op == 0x1E) { off += 1; continue; }             // Pad
        if (op == 0xF6 && avail >= 7) { off += 7; continue; } // VertexInfo
        // Other control/attribute opcodes (LOD/detail jumps, culls, render
        // state, …): skip by size and keep collecting facets in this region
        // rather than stopping, so the full model's geometry is recovered.
        size_t sz = instr_skip(p, avail);
        if (sz == 0) return;
        off += sz;
    }
}

// ---- public API ---------------------------------------------------------

ShMesh sh_parse_mesh(const uint8_t* data, size_t size) {
    return sh_parse_mesh(data, size, ShState{});
}

ShMesh sh_parse_mesh(const uint8_t* data, size_t size, const ShState& state) {
    ShMesh mesh{};
    auto cs = find_code_section(data, size);
    if (!cs.data) return mesh;

    mesh.scale = read_scale(cs.data, cs.size);

    // Base geometry (everything up to the first x86 selector).
    walk_code(cs.data, cs.size, mesh.scale,
              mesh.vertices, mesh.faces, mesh.textures,
              state.destroyed, state.frame, mesh.frame_count);

    // The x86 conditional selectors gate the articulation geometry. We can't
    // execute the x86, but its `esi` re-entry points are internal pointers in
    // the PE base-relocation table (#297). Harvest each internal sub-stream.
    PeInfo pe = parse_pe(data, size);
    auto targets = collect_reloc_targets(data, size, pe, cs.size);
    const size_t base_count = mesh.vertices.size();
    std::vector<char> visited(cs.size, 0);
    for (auto& t : targets) {
        uint32_t to = t.second;
        // skip trampolines (FF 25 -> external import) — those aren't geometry
        if (to + 1 < cs.size && cs.data[to] == 0xFF && cs.data[to + 1] == 0x25) continue;
        harvest_target(cs.data, cs.size, to, mesh.scale, mesh.vertices, mesh.faces,
                       mesh.textures.empty() ? std::string() : mesh.textures.back(),
                       base_count, visited, 0, state.frame, mesh.frame_count);
    }
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
    info.vert_count  = (int)mesh.vertices.size();
    info.face_count  = (int)mesh.faces.size();
    info.frame_count = mesh.frame_count;
    info.textures    = mesh.textures;

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
    // Texture coordinates (vt) are in texel space (origin top-left); divide by
    // the referenced PIC's width/height, and flip V, for a 0..1 sampler.
    ss << '\n';

    for (auto& v : mesh.vertices)
        ss << "v " << v.x << " " << v.y << " " << v.z << '\n';

    std::string cur_tex;
    size_t vt_next = 1;  // OBJ vt indices are 1-based
    for (auto& f : mesh.faces) {
        if (f.indices.empty()) continue;
        if (f.texture != cur_tex) {
            cur_tex = f.texture;
            if (!cur_tex.empty())
                ss << "usemtl " << cur_tex << '\n';
        }
        const bool tex = f.texcoords.size() == f.indices.size();
        size_t vt_base = vt_next;
        if (tex) {
            for (auto& tc : f.texcoords)
                ss << "vt " << tc.s << " " << tc.t << '\n';
            vt_next += f.texcoords.size();
        }
        ss << 'f';
        for (size_t k = 0; k < f.indices.size(); ++k) {
            ss << ' ' << (f.indices[k] + 1); // OBJ is 1-based
            if (tex) ss << '/' << (vt_base + k);
        }
        ss << '\n';
    }
    return ss.str();
}

} // namespace fx
