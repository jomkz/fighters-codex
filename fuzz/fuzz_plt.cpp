// Fuzz target: the pilot-save round-trip codec (plt_repack = plt_read →
// plt_write), which the fxs PLT editor runs over untrusted save files.
//
// plt_write overlays only the mapped fields onto a verbatim copy of the input
// and leaves unedited fields byte-for-byte, so an *unedited* round-trip of any
// input plt_read accepts must reproduce that input exactly (P.md § Round-Trip
// Notes). Asserting that byte-identity on arbitrary accepted bytes is the
// tightest oracle for the codec's contract — a mismatch is a real bug.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include <fx/plt.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::vector<uint8_t> once = fx::plt_repack(data, size);
    if (once.empty()) return 0;  // rejected by plt_read (size < 0xB0 or bad tag)
    if (once.size() != size || !std::equal(once.begin(), once.end(), data))
        abort();  // unedited round-trip is not byte-identical
    return 0;
}
