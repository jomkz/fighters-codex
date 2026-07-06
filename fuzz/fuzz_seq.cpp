// Fuzz target: the SEQ cutscene-timeline parser and serializer. SEQ's
// serializer normalizes line endings to CRLF (bare CR and LF-only lines
// collapse), so byte-identity holds only for already-canonical input, not
// for arbitrary bytes. The invariant that holds for *any* input is
// idempotency: serialize is a fixed point of parse — once normalized, a
// second round trip changes nothing. A mismatch is a real serializer bug.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <fx/seq.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::vector<uint8_t> once = fx::seq_serialize(fx::seq_parse(data, size));
    std::vector<uint8_t> twice = fx::seq_serialize(fx::seq_parse(once.data(), once.size()));
    if (once != twice) abort();  // serialize is not a fixed point of parse
    return 0;
}
