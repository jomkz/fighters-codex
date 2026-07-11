// Fuzz target: the read-only .VDO decoder. A dumb fuzzer can't synthesize a
// valid RATVID header, so we wrap the input as a single frame behind a fixed
// valid header + a one-entry FBC — that drives the frame parser, the RLE
// decompressor, and the copy-mask blit over arbitrary bytes. The decoder must
// never read out of bounds regardless of input.

#include <cstddef>
#include <cstdint>
#include <vector>

#include <fx/vdo.h>

static void put16(std::vector<uint8_t>& v, size_t off, uint16_t x) {
    v[off] = (uint8_t)(x & 0xff);
    v[off + 1] = (uint8_t)(x >> 8);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size > (1u << 20)) return 0;

    std::vector<uint8_t> vdo(816, 0);
    vdo[0] = 'R'; vdo[1] = 'A'; vdo[2] = 'T'; vdo[3] = 'V'; vdo[4] = 'I'; vdo[5] = 'D';
    put16(vdo, 0x12, 8);   // 8 x 8 = 64 px, 8 groups
    put16(vdo, 0x14, 8);
    put16(vdo, 0x16, 256);
    vdo.insert(vdo.end(), data, data + size);  // input = single frame region

    // FBC: one frame covering the whole appended region.
    std::vector<uint8_t> fbc(4);
    uint32_t fsz = (uint32_t)size;
    for (int b = 0; b < 4; b++) fbc[b] = (uint8_t)(fsz >> (8 * b));

    fx::VdoDecoder* d = fx::vdo_open(vdo.data(), vdo.size(), fbc.data(), fbc.size());
    if (!d) return 0;
    volatile size_t sink = fx::vdo_decode_frame(d, 0).size();
    (void)sink;
    fx::vdo_close(d);
    return 0;
}
