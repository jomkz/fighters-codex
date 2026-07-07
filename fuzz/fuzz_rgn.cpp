// Fuzz target: the RGN installer UI region-map codec (rgn_read + rgn_write).
// rgn_write is the byte-identical inverse of rgn_read; the invariant that holds
// for any accepted input is idempotency.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include <fx/rgn.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    fx::RgnFile rgn{};
    if (!fx::rgn_read(data, size, rgn)) return 0;
    std::vector<uint8_t> once = fx::rgn_write(rgn);
    fx::RgnFile rgn2{};
    if (!fx::rgn_read(once.data(), once.size(), rgn2)) abort();
    if (fx::rgn_write(rgn2) != once) abort();  // not a fixed point
    return 0;
}
