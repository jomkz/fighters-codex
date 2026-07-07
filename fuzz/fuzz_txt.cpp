// Fuzz target: the TXT in-game text/directive engine (txt_read + txt_write),
// shared by MT/SSF. txt_write is the byte-identical inverse of txt_read for
// canonical input; the invariant that holds for any input is idempotency —
// serialize is a fixed point of parse. A mismatch is a real serializer bug.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include <fx/txt.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::vector<uint8_t> once = fx::txt_write(fx::txt_read(data, size));
    std::vector<uint8_t> twice =
        fx::txt_write(fx::txt_read(once.data(), once.size()));
    if (once != twice) abort();  // serialize is not a fixed point of parse
    return 0;
}
