#include "fx/cb8.h"
#include <algorithm>
#include <cstring>
#include <vector>

namespace fx {

// ---- helpers -------------------------------------------------------

static uint32_t read_u32le(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// ---- index ---------------------------------------------------------

struct IndexEntry {
    uint32_t file_off;
    uint32_t chunk_size;
    uint32_t cumul_audio;
    uint32_t samples_per_frame;
};

// Locate the VooM chunk and parse its header + index entries.
// Returns false if the file is not a valid CB8 or VooM is missing/malformed.
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
            // VooM payload: width @ +8, height @ +12, audio_sync @ +16
            // Index entries start @ +20, 16 bytes each.
            // N = (chunk_size - 20) / 16
            if (chunk_size < 20) return false;
            *out_width        = read_u32le(p + 8);
            *out_height       = read_u32le(p + 12);
            if (out_audio_sync) *out_audio_sync = read_u32le(p + 16);
            uint32_t n = (chunk_size - 20) / 16;
            out_index->resize(n);
            const uint8_t* ep = p + 20;
            for (uint32_t i = 0; i < n; i++, ep += 16) {
                (*out_index)[i].file_off        = read_u32le(ep + 0);
                (*out_index)[i].chunk_size      = read_u32le(ep + 4);
                (*out_index)[i].cumul_audio     = read_u32le(ep + 8);
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
    std::vector<uint8_t>    canvas;     // (width/4)*(height/4)*16 bytes
    uint32_t                next_frame; // next frame not yet applied to canvas
};

Cb8Decoder* cb8_open(const uint8_t* data, size_t size) {
    auto* dec = new Cb8Decoder;
    dec->data       = data;
    dec->size       = size;
    dec->next_frame = 0;

    uint32_t sync_unused = 0;
    if (!parse_voorm(data, size, &dec->width, &dec->height, &sync_unused, &dec->index) ||
        dec->width == 0 || dec->height == 0 ||
        (dec->width % 4) != 0 || (dec->height % 4) != 0) {
        delete dec;
        return nullptr;
    }

    uint32_t n_blocks = (dec->width / 4) * (dec->height / 4);
    dec->canvas.assign((size_t)n_blocks * 16, 0);
    return dec;
}

void cb8_close(Cb8Decoder* dec) { delete dec; }

// Apply one MRFI delta frame to the decoder canvas.
//
// MRFI chunk layout (offsets from chunk start):
//   0â€“3    "MRFI" tag
//   4â€“7    uint32 LE chunk_size (includes these 8 bytes)
//   8â€“23   16-byte payload header (4 Ã— uint32 LE, purpose unknown)
//   24â€“623 skip map: 4800 bits, one per 4Ã—4 block; bit=1 â†’ block changed
//   624+   block data (chunk_size âˆ’ 624 bytes)
//
// Block data â€” two sections:
//   Section 1 (bytes 0 .. n_changed*16 - 1):
//     n_changed Ã— 16-byte delta blocks, ordered by skip-map position.
//   Section 2 (bytes n_changed*16 .. bdSize - 1, when present):
//     Full-state blocks starting from block 0.  Floor(extra/16) complete
//     blocks; any trailing bytes (<16) are ignored.
static void apply_mrfi(Cb8Decoder* dec, uint32_t frame_idx) {
    if (frame_idx >= (uint32_t)dec->index.size()) return;
    const IndexEntry& ie = dec->index[frame_idx];
    if ((size_t)ie.file_off + ie.chunk_size > dec->size) return;
    if (ie.chunk_size < 624) return;

    const uint8_t* c          = dec->data + ie.file_off;
    const uint8_t* skip_map   = c + 24;
    const uint8_t* block_data = c + 624;
    uint32_t       bd_size    = ie.chunk_size - 624;

    uint32_t n_blocks = (uint32_t)(dec->canvas.size() / 16);

    // Collect indices of changed blocks (bit=1 in skip map, LSB-first).
    std::vector<uint32_t> changed;
    changed.reserve(256);
    for (uint32_t b = 0; b < n_blocks; b++) {
        if ((skip_map[b / 8] >> (b % 8)) & 1)
            changed.push_back(b);
    }

    uint32_t n_changed     = (uint32_t)changed.size();
    uint32_t section1_size = n_changed * 16;

    // Section 1: delta blocks at their skip-map positions.
    if (section1_size <= bd_size) {
        for (uint32_t i = 0; i < n_changed; i++)
            memcpy(dec->canvas.data() + changed[i] * 16, block_data + i * 16, 16);
    }

    // Section 2: full-state overwrite from block 0.
    if (bd_size > section1_size) {
        uint32_t extra_blocks = (bd_size - section1_size) / 16;
        uint32_t limit        = std::min(extra_blocks, n_blocks);
        const uint8_t* src2   = block_data + section1_size;
        for (uint32_t i = 0; i < limit; i++)
            memcpy(dec->canvas.data() + i * 16, src2 + i * 16, 16);
    }
}

// Flatten the block canvas into a contiguous widthÃ—height pixel array.
static std::vector<uint8_t> render_canvas(const Cb8Decoder* dec) {
    uint32_t w              = dec->width;
    uint32_t h              = dec->height;
    uint32_t blocks_per_row = w / 4;
    std::vector<uint8_t> frame((size_t)w * h);

    for (uint32_t b = 0; b < (uint32_t)(dec->canvas.size() / 16); b++) {
        uint32_t       bx  = b % blocks_per_row;
        uint32_t       by  = b / blocks_per_row;
        const uint8_t* blk = dec->canvas.data() + b * 16;
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++)
                frame[(by * 4 + r) * w + bx * 4 + c] = blk[r * 4 + c];
    }
    return frame;
}

std::vector<uint8_t> cb8_decode_frame(Cb8Decoder* dec, uint32_t frame_idx) {
    if (!dec || frame_idx >= (uint32_t)dec->index.size()) return {};

    // Seeking backward: reset canvas and replay from frame 0.
    if (frame_idx < dec->next_frame) {
        std::fill(dec->canvas.begin(), dec->canvas.end(), 0);
        dec->next_frame = 0;
    }

    for (uint32_t f = dec->next_frame; f <= frame_idx; f++)
        apply_mrfi(dec, f);
    dec->next_frame = frame_idx + 1;

    return render_canvas(dec);
}

} // namespace fx
