// Fuzz target: the SSF installer-script codec (ssf_read + ssf_write), built on
// the TXT engine with statements layered on top. ssf_write is the byte-identical
// inverse of ssf_read; the invariant that holds for any input is idempotency.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include <fx/ssf.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::vector<uint8_t> once = fx::ssf_write(fx::ssf_read(data, size));
    std::vector<uint8_t> twice =
        fx::ssf_write(fx::ssf_read(once.data(), once.size()));
    if (once != twice) abort();  // serialize is not a fixed point of parse
    return 0;
}
