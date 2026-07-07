// Fuzz target: the CN_INFO network-configuration codec (dat_read + dat_write).
// dat_write is the byte-identical inverse of dat_read (a fixed 3552 bytes); the
// invariant that holds for any accepted input is idempotency.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include <fx/dat.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    fx::CnInfo info{};
    if (!fx::dat_read(data, size, info)) return 0;
    std::vector<uint8_t> once = fx::dat_write(info);
    fx::CnInfo info2{};
    if (!fx::dat_read(once.data(), once.size(), info2)) abort();
    if (fx::dat_write(info2) != once) abort();  // not a fixed point
    return 0;
}
