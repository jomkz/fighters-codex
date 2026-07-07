// Fuzz target: the LAY sky/atmosphere codec (lay_parse + lay_repack), the
// MZ/LE overlay parser behind `fx lay`. lay_repack carries unmodeled bytes
// over verbatim and rewrites only the mapped band tables, returning empty on a
// count/sentinel/VA mismatch. The invariant that holds for any accepted input
// is idempotency — re-packing an already-canonical file changes nothing.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include <fx/lay.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    fx::LayFile lay = fx::lay_parse(data, size);
    std::vector<uint8_t> once = fx::lay_repack(data, size, lay);
    if (once.empty()) return 0;  // parser rejected or structure mismatch
    fx::LayFile lay2 = fx::lay_parse(once.data(), once.size());
    std::vector<uint8_t> twice = fx::lay_repack(once.data(), once.size(), lay2);
    if (once != twice) abort();  // repack is not a fixed point of parse
    return 0;
}
