// Fuzz target: ESA installer-archive parsing and entry extraction, including the
// raw PKWare DCL ("blast") decode behind PKWA entries. This is the exact path
// `fx esa unpack` runs over untrusted archive bytes.

#include <cstddef>
#include <cstdint>

#include <fx/esa.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const auto dir = fx::esa_read_dir(data, size);
    for (const auto& e : dir) {
        fx::esa_safe_name(e.name);
        fx::esa_extract(data, size, e, /*decompress=*/true);
    }
    fx::esa_repack(data, size);
    return 0;
}
