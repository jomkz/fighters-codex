// Fuzz target: the VGA 6-bit palette loader (pal_load). It reads a 768-byte
// (256 x RGB) palette from untrusted bytes; a short/oversized buffer is the
// surface under test.

#include <cstddef>
#include <cstdint>

#include <fx/pal.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    fx::Palette pal = fx::pal_load(data, size);
    volatile uint8_t sink = pal.r[0] ^ pal.g[255] ^ pal.b[128];
    (void)sink;
    return 0;
}
