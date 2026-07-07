// Fuzz target: the INF aircraft tech-sheet parser/serializer (fx inf). INF
// sections carry their exact source bytes, so inf_serialize is byte-identical
// for canonical input; the invariant that holds for *any* input is
// idempotency — serialize is a fixed point of parse. A mismatch is a bug.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include <fx/inf.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::vector<uint8_t> once = fx::inf_serialize(fx::inf_parse(data, size));
    std::vector<uint8_t> twice =
        fx::inf_serialize(fx::inf_parse(once.data(), once.size()));
    if (once != twice) abort();  // serialize is not a fixed point of parse
    return 0;
}
