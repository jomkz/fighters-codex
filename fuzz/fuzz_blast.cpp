// Fuzz target: the PKWare DCL ("blast") decompressor, both entry points —
// the raw stream and the EA wrapper (4-byte LE decompressed-size prefix)
// that LIB flags==4 entries carry. Driven directly, without the container
// framing, so the bitstream state machine gets explored deeper than
// fuzz_ealib reaches through whole archives.

#include <cstddef>
#include <cstdint>
#include <vector>

#include <fx/blast.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    static std::vector<uint8_t> out(1 << 20);
    fx::blast_decompress(data, size, out.data(), out.size());
    fx::blast_decompress_ea(data, size, out.data(), out.size());
    // Output-full edges: zero capacity and a deliberately tight fit.
    fx::blast_decompress(data, size, out.data(), 0);
    fx::blast_decompress(data, size, out.data(), 13);
    return 0;
}
