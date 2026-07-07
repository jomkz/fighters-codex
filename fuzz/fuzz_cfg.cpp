// Fuzz target: the EA.CFG game-configuration codec (cfg_read + cfg_write).
// cfg_write is the byte-identical inverse of cfg_read (a fixed 347 bytes); the
// invariant that holds for any accepted input is idempotency.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include <fx/cfg.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    fx::EaCfg c{};
    if (!fx::cfg_read(data, size, c)) return 0;
    std::vector<uint8_t> once = fx::cfg_write(c);
    fx::EaCfg c2{};
    if (!fx::cfg_read(once.data(), once.size(), c2)) abort();  // own output rejected
    if (fx::cfg_write(c2) != once) abort();                    // not a fixed point
    return 0;
}
