// Fuzz target: the FBC video frame-index codec (fbc_read + fbc_write).
// fbc_write is the byte-identical inverse of fbc_read; the invariant that holds
// for any accepted input is idempotency. fbc_frame_offset is also exercised.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include <fx/fbc.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    bool ok = false;
    std::vector<uint32_t> frames = fx::fbc_read(data, size, &ok);
    if (!ok) return 0;
    volatile uint64_t sink = fx::fbc_frame_offset(frames, frames.size());
    (void)sink;
    std::vector<uint8_t> once = fx::fbc_write(frames);
    bool ok2 = false;
    std::vector<uint32_t> f2 = fx::fbc_read(once.data(), once.size(), &ok2);
    if (!ok2 || fx::fbc_write(f2) != once) abort();  // not a fixed point
    return 0;
}
