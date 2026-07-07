// Fuzz target: the BRF record-document codec (brf_parse + brf_serialize)
// shared by the PT/OT/NT/JT/... type-definition formats — the path
// `fx <type> repack` runs over untrusted bytes. BRF round-trips byte-identically
// for canonical input; the invariant that holds for *any* input is idempotency
// — serialize is a fixed point of parse. A mismatch is a real serializer bug.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include <fx/brf.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::vector<uint8_t> once = fx::brf_serialize(fx::brf_parse(data, size));
    std::vector<uint8_t> twice =
        fx::brf_serialize(fx::brf_parse(once.data(), once.size()));
    if (once != twice) abort();  // serialize is not a fixed point of parse
    return 0;
}
