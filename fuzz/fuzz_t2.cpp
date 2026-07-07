// Fuzz target: the T2 terrain-map reader + serializer (t2_repack = t2_read →
// t2_write). t2_read validates the grid structurally and rejects inconsistent
// headers, so the leaf/summary allocations are bounded by the file's own
// declared grid. The invariant that holds for any accepted input is
// idempotency — the serializer is a fixed point once the bytes are canonical;
// a mismatch is a real serializer bug.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include <fx/t2.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::vector<uint8_t> once = fx::t2_repack(data, size);
    if (once.empty()) return 0;  // rejected by t2_read
    std::vector<uint8_t> twice = fx::t2_repack(once.data(), once.size());
    if (once != twice) abort();  // serialize is not a fixed point of read
    return 0;
}
