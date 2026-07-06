// Fuzz target: the CB8 FMV container — VooM index parse, keyframe decode
// (mode bitmap + codebooks + index stream), the embedded per-frame palette,
// and the re-encoder driven by whatever the decoder produced; the path
// `fx cb8 unpack` / `fx cb8 repack` runs over untrusted bytes.
//
// Header-declared dimensions bound the decode (frames allocate
// width*height); the repack leg runs only for small movies so the smoke
// spends its budget in the parsers, not the encoder.

#include <cstddef>
#include <cstdint>
#include <vector>

#include <fx/cb8.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    fx::Cb8Info info;
    if (!fx::cb8_info(data, size, &info)) return 0;

    fx::Cb8Decoder* dec = fx::cb8_open(data, size);
    if (!dec) return 0;
    if ((uint64_t)info.width * info.height > (4u << 20) ||
        info.frame_count > 64) {
        fx::cb8_close(dec);
        return 0;
    }

    fx::Palette pal;
    auto px = fx::cb8_decode_frame(dec, 0);
    fx::cb8_frame_palette(dec, 0, &pal);
    fx::cb8_decode_frame_rgba(dec, 0);
    fx::cb8_decode_frame(dec, info.frame_count);  // out-of-range edge

    if (!px.empty() && info.frame_count <= 2 &&
        (uint64_t)info.width * info.height <= (64u << 10)) {
        std::vector<fx::Cb8Frame> frames(info.frame_count);
        for (uint32_t i = 0; i < info.frame_count; ++i) {
            frames[i].indices = fx::cb8_decode_frame(dec, i);
            if (frames[i].indices.empty())
                frames[i].indices.assign((size_t)info.width * info.height, 0);
        }
        fx::cb8_repack(data, size, frames);
    }
    fx::cb8_close(dec);
    return 0;
}
