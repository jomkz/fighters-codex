// CB8 codec — engine-traced model (#95): self-contained VQ keyframes with
// embedded palettes. Decode mirrors DecodeSVGA8Frame/ExpandDB/CopySB8/CopyDB8
// (see docs/fa/formats/CB8.md and scripts/ghidra/AnalyzeCB8.java); the
// encoder inverts that decode exactly.
#include "fx/cb8.h"
#include <algorithm>
#include <cstring>
#include <map>

namespace fx {

// ---- helpers -------------------------------------------------------

static uint32_t read_u32le(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t read_u16le(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}
static void write_u32le(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back((uint8_t)x);
    v.push_back((uint8_t)(x >> 8));
    v.push_back((uint8_t)(x >> 16));
    v.push_back((uint8_t)(x >> 24));
}
static void write_u16le(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back((uint8_t)x);
    v.push_back((uint8_t)(x >> 8));
}

// ---- index ---------------------------------------------------------

struct IndexEntry {
    uint32_t file_off;
    uint32_t chunk_size;
    uint32_t cumul_audio;
    uint32_t samples_per_frame;
};

// Locate the VooM chunk and parse its header + index entries.
static bool parse_voorm(const uint8_t* data, size_t size,
                        uint32_t* out_width, uint32_t* out_height,
                        uint32_t* out_audio_sync,
                        std::vector<IndexEntry>* out_index) {
    if (size < 64 || memcmp(data, "DRBC", 4) != 0) return false;

    size_t pos = 64;
    while (pos + 8 <= size) {
        const uint8_t* p = data + pos;
        uint32_t chunk_size = read_u32le(p + 4);
        if (chunk_size < 8 || (size_t)chunk_size > size - pos) return false;

        if (memcmp(p, "VooM", 4) == 0) {
            if (chunk_size < 20) return false;
            *out_width  = read_u32le(p + 8);
            *out_height = read_u32le(p + 12);
            if (out_audio_sync) *out_audio_sync = read_u32le(p + 16);
            uint32_t n = (chunk_size - 20) / 16;
            out_index->resize(n);
            const uint8_t* ep = p + 20;
            for (uint32_t i = 0; i < n; i++, ep += 16) {
                (*out_index)[i].file_off          = read_u32le(ep + 0);
                (*out_index)[i].chunk_size        = read_u32le(ep + 4);
                (*out_index)[i].cumul_audio       = read_u32le(ep + 8);
                (*out_index)[i].samples_per_frame = read_u32le(ep + 12);
            }
            return true;
        }
        pos += chunk_size;
    }
    return false;
}

// ---- public API ----------------------------------------------------

bool cb8_info(const uint8_t* data, size_t size, Cb8Info* out) {
    if (!out) return false;
    uint32_t w, h, sync = 0;
    std::vector<IndexEntry> index;
    if (!parse_voorm(data, size, &w, &h, &sync, &index)) return false;
    out->width             = w;
    out->height            = h;
    out->frame_count       = (uint32_t)index.size();
    out->samples_per_frame = index.empty() ? 0 : index[0].samples_per_frame;
    out->audio_sync_rate   = sync;
    return true;
}

// ---- decoder -------------------------------------------------------

struct Cb8Decoder {
    const uint8_t*          data;
    size_t                  size;
    uint32_t                width;
    uint32_t                height;
    std::vector<IndexEntry> index;
};

Cb8Decoder* cb8_open(const uint8_t* data, size_t size) {
    auto* dec = new Cb8Decoder;
    dec->data = data;
    dec->size = size;

    uint32_t sync_unused = 0;
    if (!parse_voorm(data, size, &dec->width, &dec->height, &sync_unused, &dec->index) ||
        dec->width == 0 || dec->height == 0 ||
        (dec->width % 4) != 0 || (dec->height % 4) != 0) {
        delete dec;
        return nullptr;
    }
    return dec;
}

void cb8_close(Cb8Decoder* dec) { delete dec; }

// The parsed regions of one MRFI frame (kind 0, submode 5).
struct FrameRegions {
    uint16_t A, B, C, S, D, X;
    const uint8_t* palette;  // D bytes (6-bit VGA RGB)
    const uint8_t* detail;   // (A+B)*4 bytes, 2x2-pixel entries
    const uint8_t* single;   // C*4 bytes, 2x2-pixel entries
    const uint8_t* bitmap;   // S bytes, u32-LE words consumed MSB-first
    const uint8_t* idx;      // index stream
    size_t         idx_size;
};

static bool parse_frame(const Cb8Decoder* dec, uint32_t frame_idx, FrameRegions* r) {
    if (!dec || frame_idx >= (uint32_t)dec->index.size()) return false;
    const IndexEntry& ie = dec->index[frame_idx];
    if ((size_t)ie.file_off + ie.chunk_size > dec->size || ie.chunk_size < 0x18) return false;

    const uint8_t* c = dec->data + ie.file_off;
    if (c[8] != 0 || c[9] != 5) return false;  // key frame, 8-bit submode only
    r->A = read_u16le(c + 0x0A);
    r->B = read_u16le(c + 0x0C);
    r->C = read_u16le(c + 0x0E);
    r->S = read_u16le(c + 0x10);
    r->D = read_u16le(c + 0x12);
    r->X = read_u16le(c + 0x14);

    const uint64_t fixed = 0x18ull + r->D + ((uint32_t)r->A + r->B) * 4ull +
                           (uint32_t)r->C * 4ull + r->S;
    if (fixed > ie.chunk_size) return false;
    const uint8_t* base = c + 0x18;
    r->palette  = base;
    r->detail   = base + r->D;
    r->single   = r->detail + ((uint32_t)r->A + r->B) * 4;
    r->bitmap   = r->single + (uint32_t)r->C * 4;
    r->idx      = r->bitmap + r->S;
    r->idx_size = ie.chunk_size - fixed;
    return true;
}

std::vector<uint8_t> cb8_decode_frame(Cb8Decoder* dec, uint32_t frame_idx) {
    FrameRegions r;
    if (!parse_frame(dec, frame_idx, &r)) return {};
    const uint32_t w = dec->width, h = dec->height;
    const uint32_t cw = w / 4, ch = h / 4, cells = cw * ch;
    if ((uint64_t)r.S * 8 < cells) return {};

    std::vector<uint8_t> frame((size_t)w * h, 0);
    size_t ip = 0;
    uint32_t cell = 0;
    for (uint32_t cy = 0; cy < ch; cy++) {
        const uint32_t py = cy * 4;
        // Detail-book band switch at pixel row X (0xFFFF = never).
        const uint32_t book_off = (r.X != 0xFFFF && py >= r.X) ? (uint32_t)r.A * 4 : 0;
        for (uint32_t cx = 0; cx < cw; cx++, cell++) {
            const uint32_t word = read_u32le(r.bitmap + (cell / 32) * 4);
            const uint32_t bit = (word >> (31 - (cell % 32))) & 1;
            const uint32_t px = cx * 4;
            if (bit == 0) {
                // Single: one index, 2x2 entry pixel-doubled to 4x4 (ExpandDB).
                if (ip + 1 > r.idx_size) return {};
                const uint8_t e = r.idx[ip++];
                if (e >= r.C) return {};
                const uint8_t* t = r.single + (uint32_t)e * 4;
                for (int q = 0; q < 4; q++) {
                    const uint8_t v = t[q];
                    const uint32_t qy = py + ((q >> 1) << 1), qx = px + ((q & 1) << 1);
                    frame[(size_t)qy * w + qx]           = v;
                    frame[(size_t)qy * w + qx + 1]       = v;
                    frame[(size_t)(qy + 1) * w + qx]     = v;
                    frame[(size_t)(qy + 1) * w + qx + 1] = v;
                }
            } else {
                // Detail: four indices, TL/TR/BL/BR 2x2 entries (CopyDB8).
                if (ip + 4 > r.idx_size) return {};
                for (int q = 0; q < 4; q++) {
                    const uint8_t e = r.idx[ip++];
                    if ((uint32_t)e * 4 + book_off + 4 > ((uint32_t)r.A + r.B) * 4) return {};
                    const uint8_t* t = r.detail + book_off + (uint32_t)e * 4;
                    const uint32_t qy = py + ((q >> 1) << 1), qx = px + ((q & 1) << 1);
                    frame[(size_t)qy * w + qx]           = t[0];
                    frame[(size_t)qy * w + qx + 1]       = t[1];
                    frame[(size_t)(qy + 1) * w + qx]     = t[2];
                    frame[(size_t)(qy + 1) * w + qx + 1] = t[3];
                }
            }
        }
    }
    return frame;
}

bool cb8_frame_palette(Cb8Decoder* dec, uint32_t frame_idx, Palette* out) {
    FrameRegions r;
    if (!out || !parse_frame(dec, frame_idx, &r) || r.D < 768) return false;
    for (int i = 0; i < 256; i++) {
        const uint8_t rv = r.palette[i * 3 + 0];
        const uint8_t gv = r.palette[i * 3 + 1];
        const uint8_t bv = r.palette[i * 3 + 2];
        out->r[i] = (uint8_t)((rv << 2) | (rv >> 6));
        out->g[i] = (uint8_t)((gv << 2) | (gv >> 6));
        out->b[i] = (uint8_t)((bv << 2) | (bv >> 6));
    }
    return true;
}

std::vector<uint8_t> cb8_decode_frame_rgba(Cb8Decoder* dec, uint32_t frame_idx) {
    auto idxs = cb8_decode_frame(dec, frame_idx);
    Palette pal;
    if (idxs.empty() || !cb8_frame_palette(dec, frame_idx, &pal)) return {};
    std::vector<uint8_t> rgba(idxs.size() * 4);
    for (size_t i = 0; i < idxs.size(); i++) {
        const uint8_t v = idxs[i];
        rgba[i * 4 + 0] = pal.r[v];
        rgba[i * 4 + 1] = pal.g[v];
        rgba[i * 4 + 2] = pal.b[v];
        rgba[i * 4 + 3] = 0xFF;
    }
    return rgba;
}

// ---- encoder -------------------------------------------------------

// A 2x2 pixel tile as a codebook key.
struct Tile {
    uint8_t b[4];
    bool operator<(const Tile& o) const { return memcmp(b, o.b, 4) < 0; }
};

// One classified 4x4 cell: either a single 2x2 tile (the cell is that tile
// pixel-doubled) or four detail tiles (TL, TR, BL, BR).
struct Cell {
    bool single;
    Tile t[4];  // t[0] when single
};

static Cell classify_cell(const uint8_t* px, uint32_t w, uint32_t x, uint32_t y) {
    Cell cell;
    // Quadrant values when every 2x2 quadrant is uniform -> single.
    bool uniform = true;
    uint8_t qv[4];
    for (int q = 0; q < 4; q++) {
        const uint32_t qy = y + ((q >> 1) << 1), qx = x + ((q & 1) << 1);
        const uint8_t v = px[(size_t)qy * w + qx];
        qv[q] = v;
        uniform = uniform && px[(size_t)qy * w + qx + 1] == v &&
                  px[(size_t)(qy + 1) * w + qx] == v &&
                  px[(size_t)(qy + 1) * w + qx + 1] == v;
    }
    if (uniform) {
        cell.single = true;
        memcpy(cell.t[0].b, qv, 4);
        return cell;
    }
    cell.single = false;
    for (int q = 0; q < 4; q++) {
        const uint32_t qy = y + ((q >> 1) << 1), qx = x + ((q & 1) << 1);
        cell.t[q].b[0] = px[(size_t)qy * w + qx];
        cell.t[q].b[1] = px[(size_t)qy * w + qx + 1];
        cell.t[q].b[2] = px[(size_t)(qy + 1) * w + qx];
        cell.t[q].b[3] = px[(size_t)(qy + 1) * w + qx + 1];
    }
    return cell;
}

// Encode one frame as an MRFI chunk (without the audio/index bookkeeping).
// Returns empty when a codebook overflows 256 entries even after splitting.
static std::vector<uint8_t> encode_mrfi(const Cb8Frame& f, uint32_t w, uint32_t h) {
    const uint32_t cw = w / 4, ch = h / 4, cells = cw * ch;
    std::vector<Cell> classified;
    classified.reserve(cells);
    for (uint32_t cy = 0; cy < ch; cy++)
        for (uint32_t cx = 0; cx < cw; cx++)
            classified.push_back(classify_cell(f.indices.data(), w, cx * 4, cy * 4));

    // Single book: global, capped at 256. When more distinct doubled tiles
    // exist (the originals hit this too), keep the most frequent 256 and
    // demote the rest to detail cells of four uniform 2x2 tiles — the pixel
    // output is identical either way.
    std::map<Tile, uint32_t> single_count;
    for (const Cell& c : classified)
        if (c.single) single_count[c.t[0]]++;
    std::map<Tile, uint8_t> single_book;
    if (single_count.size() <= 256) {
        uint8_t id = 0;
        for (const auto& kv : single_count) single_book.emplace(kv.first, id++);
    } else {
        std::vector<std::pair<Tile, uint32_t>> by_freq(single_count.begin(),
                                                       single_count.end());
        std::stable_sort(by_freq.begin(), by_freq.end(),
                         [](const auto& a, const auto& b) { return a.second > b.second; });
        by_freq.resize(256);
        std::sort(by_freq.begin(), by_freq.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        uint8_t id = 0;
        for (const auto& kv : by_freq) single_book.emplace(kv.first, id++);
        for (Cell& c : classified) {
            if (!c.single || single_book.count(c.t[0])) continue;
            Cell d;
            d.single = false;
            for (int q = 0; q < 4; q++)
                memset(d.t[q].b, c.t[0].b[q], 4);  // uniform 2x2 per quadrant
            c = d;
        }
    }

    // Detail book: try one band; on overflow, greedily split at the first
    // cell row whose tiles no longer fit, giving each band its own 256.
    auto count_band = [&](uint32_t row_from, uint32_t row_to,
                          std::map<Tile, uint8_t>& book) -> bool {
        for (uint32_t cy = row_from; cy < row_to; cy++)
            for (uint32_t cx = 0; cx < cw; cx++) {
                const Cell& c = classified[cy * cw + cx];
                if (c.single) continue;
                for (int q = 0; q < 4; q++) {
                    if (!book.count(c.t[q])) {
                        if (book.size() >= 256) return false;
                        book.emplace(c.t[q], (uint8_t)book.size());
                    }
                }
            }
        return true;
    };
    uint16_t X = 0xFFFF;
    std::map<Tile, uint8_t> band1, band2;
    if (!count_band(0, ch, band1)) {
        band1.clear();
        uint32_t split = 0;
        for (uint32_t cy = 0; cy < ch; cy++) {
            std::map<Tile, uint8_t> trial = band1;
            bool ok = true;
            for (uint32_t cx = 0; cx < cw && ok; cx++) {
                const Cell& c = classified[cy * cw + cx];
                if (c.single) continue;
                for (int q = 0; q < 4 && ok; q++) {
                    if (!trial.count(c.t[q])) {
                        if (trial.size() >= 256) ok = false;
                        else trial.emplace(c.t[q], (uint8_t)trial.size());
                    }
                }
            }
            if (!ok) { split = cy; break; }
            band1 = std::move(trial);
            split = cy + 1;
        }
        if (split == 0 || split >= ch) return {};
        X = (uint16_t)(split * 4);
        if (!count_band(split, ch, band2)) return {};
    }

    const uint16_t A = (uint16_t)band1.size();
    const uint16_t B = (uint16_t)band2.size();
    const uint16_t C = (uint16_t)single_book.size();
    const uint16_t S = (uint16_t)(((cells + 31) / 32) * 4);
    const uint16_t D = 768;

    // Mode bitmap + index stream.
    std::vector<uint8_t> bitmap(S, 0);
    std::vector<uint8_t> idx;
    idx.reserve(cells);
    for (uint32_t cy = 0; cy < ch; cy++) {
        const std::map<Tile, uint8_t>& book =
            (X != 0xFFFF && cy * 4 >= X) ? band2 : band1;
        for (uint32_t cx = 0; cx < cw; cx++) {
            const uint32_t cell = cy * cw + cx;
            const Cell& c = classified[cell];
            if (c.single) {
                idx.push_back(single_book.at(c.t[0]));
            } else {
                // Set the cell's bit, MSB-first within the u32-LE word.
                const uint32_t bitpos = 31 - (cell % 32);
                const uint32_t word_byte = (cell / 32) * 4;
                bitmap[word_byte + (bitpos / 8)] |= (uint8_t)(1u << (bitpos % 8));
                for (int q = 0; q < 4; q++) idx.push_back(book.at(c.t[q]));
            }
        }
    }

    std::vector<uint8_t> chunk;
    const size_t payload = 0x18 + D + ((uint32_t)A + B) * 4 + (uint32_t)C * 4 + S + idx.size();
    const size_t total = (payload + 3) & ~(size_t)3;  // pad to 4 like the originals
    chunk.reserve(total);
    chunk.insert(chunk.end(), {'M', 'R', 'F', 'I'});
    write_u32le(chunk, (uint32_t)total);
    chunk.push_back(0);  // kind: key frame
    chunk.push_back(5);  // submode: 8-bit paletted
    write_u16le(chunk, A);
    write_u16le(chunk, B);
    write_u16le(chunk, C);
    write_u16le(chunk, S);
    write_u16le(chunk, D);
    write_u16le(chunk, X);
    write_u16le(chunk, 0);
    chunk.insert(chunk.end(), f.palette6.begin(), f.palette6.end());
    auto emit_book = [&](const std::map<Tile, uint8_t>& book) {
        std::vector<Tile> ordered(book.size());
        for (const auto& kv : book) ordered[kv.second] = kv.first;
        for (const Tile& t : ordered) chunk.insert(chunk.end(), t.b, t.b + 4);
    };
    emit_book(band1);
    emit_book(band2);
    emit_book(single_book);
    chunk.insert(chunk.end(), bitmap.begin(), bitmap.end());
    chunk.insert(chunk.end(), idx.begin(), idx.end());
    chunk.resize(total, 0);
    return chunk;
}

std::vector<uint8_t> cb8_repack(const uint8_t* orig, size_t orig_size,
                                const std::vector<Cb8Frame>& frames) {
    uint32_t w, h, sync = 0;
    std::vector<IndexEntry> index;
    if (!parse_voorm(orig, orig_size, &w, &h, &sync, &index)) return {};
    if (frames.size() != index.size()) return {};
    for (const Cb8Frame& f : frames)
        if (f.indices.size() != (size_t)w * h) return {};

    // Pass 1: walk the original chunk stream in order, encoding each MRFI.
    struct ChunkRef {
        size_t   orig_off;
        uint32_t orig_size;
        int      frame;  // -1 = carried verbatim (MRFA etc.), -2 = VooM
    };
    std::vector<ChunkRef> stream;
    std::vector<std::vector<uint8_t>> encoded(frames.size());
    {
        // Map file offsets to frame numbers via the index.
        std::map<uint32_t, int> frame_at;
        for (size_t i = 0; i < index.size(); i++) frame_at[index[i].file_off] = (int)i;
        size_t pos = 64;
        while (pos + 8 <= orig_size) {
            const uint32_t csz = read_u32le(orig + pos + 4);
            if (csz < 8 || csz > orig_size - pos) return {};
            ChunkRef ref{pos, csz, -1};
            if (memcmp(orig + pos, "VooM", 4) == 0) {
                ref.frame = -2;
            } else if (frame_at.count((uint32_t)pos)) {
                ref.frame = frame_at[(uint32_t)pos];
                encoded[ref.frame] = encode_mrfi(frames[ref.frame], w, h);
                if (encoded[ref.frame].empty()) return {};
            }
            stream.push_back(ref);
            pos += csz;
        }
    }

    // Pass 2: assign new offsets, rebuild VooM, emit.
    const uint32_t voom_size = 20 + (uint32_t)index.size() * 16;
    std::vector<uint32_t> new_off(frames.size(), 0);
    {
        size_t pos = 64;
        for (const ChunkRef& ref : stream) {
            if (ref.frame >= 0) {
                new_off[ref.frame] = (uint32_t)pos;
                pos += encoded[ref.frame].size();
            } else if (ref.frame == -2) {
                pos += voom_size;
            } else {
                pos += ref.orig_size;
            }
        }
    }
    std::vector<uint8_t> out;
    out.insert(out.end(), orig, orig + 64);  // DRBC header verbatim
    for (const ChunkRef& ref : stream) {
        if (ref.frame >= 0) {
            out.insert(out.end(), encoded[ref.frame].begin(), encoded[ref.frame].end());
        } else if (ref.frame == -2) {
            out.insert(out.end(), {'V', 'o', 'o', 'M'});
            write_u32le(out, voom_size);
            write_u32le(out, w);
            write_u32le(out, h);
            write_u32le(out, sync);
            for (size_t i = 0; i < index.size(); i++) {
                write_u32le(out, new_off[i]);
                write_u32le(out, (uint32_t)encoded[i].size());
                write_u32le(out, index[i].cumul_audio);
                write_u32le(out, index[i].samples_per_frame);
            }
        } else {
            out.insert(out.end(), orig + ref.orig_off, orig + ref.orig_off + ref.orig_size);
        }
    }
    return out;
}

} // namespace fx
