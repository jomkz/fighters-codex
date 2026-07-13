#include "fx/vdo.h"
#include "fx/fbc.h"
#include <algorithm>
#include <cstring>

namespace fx {

namespace {

constexpr size_t kHeaderSize = 816;   // 0x330
constexpr size_t kPaletteOff = 0x30;

inline uint16_t rd16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

// Decompress one RLE sub-stream starting at `off` in [data,data+size).
// Layout: u16 output count, a discarded u16, then control bytes — a byte with
// bit 0x80 set is a run (count = low 7 bits + 1, or a following u16 + 1 when
// the low 7 bits are 0x7f, of the next value byte); otherwise it is a literal
// copy of `byte` source bytes. Appends exactly `count` bytes to `out`.
// Returns false on any out-of-bounds read (fuzz safety).
bool unrle(const uint8_t* data, size_t size, size_t off, std::vector<uint8_t>& out) {
    if (off + 4 > size) return false;
    uint32_t n = rd16(data + off);
    off += 4;  // count u16 + one discarded u16
    out.clear();
    out.reserve(n);
    while (out.size() < n) {
        if (off >= size) return false;
        uint8_t ctrl = data[off++];
        if (ctrl & 0x80) {
            uint32_t cnt = ctrl & 0x7f;
            if (cnt == 0x7f) {
                if (off + 2 > size) return false;
                cnt = rd16(data + off);
                off += 2;
            }
            cnt += 1;
            if (off >= size) return false;
            uint8_t val = data[off++];
            if (out.size() + cnt > n) cnt = (uint32_t)(n - out.size());
            out.insert(out.end(), cnt, val);
        } else {
            uint32_t cnt = ctrl;
            if (off + cnt > size) return false;
            if (out.size() + cnt > n) cnt = (uint32_t)(n - out.size());
            out.insert(out.end(), data + off, data + off + cnt);
            off += cnt;
        }
    }
    return true;
}

} // namespace

struct VdoDecoder {
    const uint8_t* vdo = nullptr;
    size_t vdo_size = 0;
    uint32_t width = 0, height = 0;
    std::vector<uint32_t> frame_sizes;   // from the FBC
    std::vector<uint64_t> frame_off;     // absolute byte offset of each frame
    std::vector<uint8_t> canvas;         // persistent framebuffer (indices)
    std::vector<uint8_t> mask, src;      // scratch decode buffers
    int64_t cursor = -1;                 // last decoded frame index
};

bool vdo_info(const uint8_t* data, size_t size, VdoInfo* out) {
    if (!data || size < kHeaderSize) return false;
    if (std::memcmp(data, "RATVID", 6) != 0) return false;
    uint32_t w = rd16(data + 0x12), h = rd16(data + 0x14);
    if (w == 0 || h == 0 || (uint64_t)w * h > (1u << 24)) return false;
    if ((w * h) % 8 != 0) return false;
    if (out) {
        out->width = w;
        out->height = h;
        out->frame_count = 0;
        out->fps = data[0x08] | (data[0x09] << 8) |
                   (data[0x0a] << 16) | (data[0x0b] << 24);
        out->audio_hz = rd16(data + 0x1a);
    }
    return true;
}

VdoDecoder* vdo_open(const uint8_t* vdo, size_t vdo_size,
                     const uint8_t* fbc, size_t fbc_size) {
    VdoInfo info;
    if (!vdo_info(vdo, vdo_size, &info)) return nullptr;
    bool ok = false;
    std::vector<uint32_t> sizes = fbc_read(fbc, fbc_size, &ok);
    if (!ok) return nullptr;

    // The FBC must exactly tile the VDO frame region.
    uint64_t total = kHeaderSize;
    for (uint32_t s : sizes) total += s;
    if (total != vdo_size) return nullptr;

    auto* dec = new VdoDecoder;
    dec->vdo = vdo;
    dec->vdo_size = vdo_size;
    dec->width = info.width;
    dec->height = info.height;
    dec->frame_sizes = std::move(sizes);
    uint64_t off = kHeaderSize;
    dec->frame_off.reserve(dec->frame_sizes.size());
    for (uint32_t s : dec->frame_sizes) {
        dec->frame_off.push_back(off);
        off += s;
    }
    dec->canvas.assign((size_t)dec->width * dec->height, 0);
    return dec;
}

void vdo_close(VdoDecoder* dec) { delete dec; }

bool vdo_palette(const VdoDecoder* dec, Palette* out) {
    if (!dec || !out) return false;
    *out = pal_load(dec->vdo + kPaletteOff, 768);
    return true;
}

uint32_t vdo_frame_count(const VdoDecoder* dec) {
    return dec ? (uint32_t)dec->frame_sizes.size() : 0;
}
uint32_t vdo_width(const VdoDecoder* dec) { return dec ? dec->width : 0; }
uint32_t vdo_height(const VdoDecoder* dec) { return dec ? dec->height : 0; }

namespace {

// Decode a single frame's data block into dec->canvas, given the last-decoded
// mask/source buffers for the tag-0 reuse case. Returns false on bad data.
bool decode_one(VdoDecoder* dec, const uint8_t* fr, size_t sz) {
    const uint32_t ngroups = (dec->width * dec->height) / 8;
    if (sz < 2) return false;
    uint16_t tag = rd16(fr);

    const uint8_t* mask = dec->mask.data();
    size_t mask_len = dec->mask.size();
    const uint8_t* src = nullptr;      // RLE-decoded source
    size_t src_len = 0;
    const uint8_t* raw_src = nullptr;  // uncompressed source
    size_t raw_len = 0;

    if (tag == 0) {
        // Reuse the previous mask + source buffers verbatim.
        src = dec->src.data();
        src_len = dec->src.size();
    } else if (tag == 1) {
        // Whole-canvas RLE keyframe: UnRLE straight into the canvas, no pixel blit (#491).
        //
        // The RLE header sits at frame+4, not frame+2. GetVDOFrame (0x4AF510) hands UnRLE
        // `(short *)((int)puVar10 + 2)` with puVar10 = frame+2 — so the u16 at frame+2 is
        // stepped over by the CALLER and is not part of the stream. It is the frame's own
        // remaining-byte count (it equals FBC[n] - 4 in all 89 tag-1 frames of the retail
        // corpus), and reading it as UnRLE's output count is what this codec did: it
        // decoded ~frame_size bytes instead of the 64,000 the count at frame+4 actually
        // asks for, and the picture was truncated — a black band across the bottom of
        // every keyframe.
        std::vector<uint8_t> full;
        if (!unrle(fr, sz, 4, full)) return false;
        size_t n = std::min(full.size(), dec->canvas.size());
        if (n) std::memcpy(dec->canvas.data(), full.data(), n);
        return true;
    } else if (tag == 2) {
        // Image-keyframe variant (DecompressVideoImage) — not present in the
        // stock corpus; treat as a no-op passthrough of the prior frame.
        return true;
    } else {
        uint32_t sz1 = (tag & 0x8000) ? (uint32_t)(tag & 0x7fff) : tag;
        if (tag & 0x8000) {
            if (!unrle(fr, sz, 2, dec->mask)) return false;
            mask = dec->mask.data();
            mask_len = dec->mask.size();
        } else {
            // Raw mask (rare): sz1 bytes after the count+dead u16 at fr+2.
            if ((size_t)2 + 4 + sz1 > sz) return false;
            mask = fr + 6;
            mask_len = sz1;
        }
        size_t mo = (size_t)2 + sz1;
        if (mo + 2 > sz) return false;
        uint16_t marker = rd16(fr + mo);
        if (marker == 0xFFFF) {
            if (!unrle(fr, sz, mo + 4, dec->src)) return false;
            src = dec->src.data();
            src_len = dec->src.size();
        } else if (marker != 0) {
            raw_src = fr + mo + 2;
            raw_len = sz - (mo + 2);
        } else {
            src = dec->src.data();  // reuse previous source
            src_len = dec->src.size();
        }
    }

    // Apply the per-pixel copy mask: one byte per 8-pixel group, MSB = pixel 0;
    // a set bit copies the next source pixel, a clear bit keeps the canvas.
    if (mask_len < ngroups) return false;
    uint8_t* canvas = dec->canvas.data();
    size_t si = 0;
    for (uint32_t g = 0; g < ngroups; g++) {
        uint8_t m = mask[g];
        if (m == 0) continue;
        uint32_t base = g * 8;
        for (int bit = 0; bit < 8; bit++) {
            if ((m >> (7 - bit)) & 1) {
                if (raw_src) {
                    if (si >= raw_len) return false;
                    canvas[base + bit] = raw_src[si++];
                } else {
                    if (si >= src_len) return false;
                    canvas[base + bit] = src[si++];
                }
            }
        }
    }
    return true;
}

} // namespace

std::vector<uint8_t> vdo_decode_frame(VdoDecoder* dec, uint32_t idx) {
    if (!dec || idx >= dec->frame_sizes.size()) return {};
    if ((int64_t)idx < dec->cursor) {
        // Rewind and replay from the start.
        std::fill(dec->canvas.begin(), dec->canvas.end(), (uint8_t)0);
        dec->mask.clear();
        dec->src.clear();
        dec->cursor = -1;
    }
    for (uint32_t f = (uint32_t)(dec->cursor + 1); f <= idx; f++) {
        const uint8_t* fr = dec->vdo + dec->frame_off[f];
        if (!decode_one(dec, fr, dec->frame_sizes[f])) return {};
        dec->cursor = f;
    }
    return dec->canvas;
}

std::vector<uint8_t> vdo_decode_frame_rgba(VdoDecoder* dec, uint32_t idx) {
    std::vector<uint8_t> ind = vdo_decode_frame(dec, idx);
    if (ind.empty()) return {};
    Palette pal;
    vdo_palette(dec, &pal);
    std::vector<uint8_t> out(ind.size() * 4);
    for (size_t i = 0; i < ind.size(); i++) {
        uint8_t c = ind[i];
        out[i * 4 + 0] = pal.r[c];
        out[i * 4 + 1] = pal.g[c];
        out[i * 4 + 2] = pal.b[c];
        out[i * 4 + 3] = 255;
    }
    return out;
}

} // namespace fx
