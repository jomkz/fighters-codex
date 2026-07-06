// Fuzz target: the PIC image codec — header parse, palette decode of the
// dense/sparse native formats, and the strict structural repack; the path
// `fx pic unpack` / `fx pic repack` runs over untrusted bytes.
//
// JPEG-format PICs (format 0xD8FF) skip the decode: that path hands the
// payload to vendored stb_image, whose hardening is upstream's business —
// the PIC-native parsers are the surface under test. Header-declared
// dimensions bound the decode (the decoders allocate width*height from the
// header; the cap keeps hostile headers under the harness malloc limit).

#include <cstddef>
#include <cstdint>

#include <fx/pal.h>
#include <fx/pic.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    fx::PicInfo info;
    if (!fx::pic_info(data, size, &info)) return 0;

    if (info.format != 0xD8FF &&
        (uint64_t)info.width * info.height <= (16u << 20)) {
        fx::Palette pal{};
        for (int i = 0; i < 256; ++i) {
            pal.r[i] = (uint8_t)i;
            pal.g[i] = (uint8_t)(255 - i);
            pal.b[i] = (uint8_t)(i ^ 0x55);
        }
        fx::pic_decode(data, size, &pal);
    }

    fx::pic_repack(data, size);
    return 0;
}
