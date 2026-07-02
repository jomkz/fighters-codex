// Fuzz target: EALIB container parsing and entry extraction, including the
// PKWare DCL ("blast") decompressor behind flags==4 entries. This is the
// exact path `fx lib unpack` runs over untrusted archive bytes.

#include <cstddef>
#include <cstdint>

#include <fx/ealib.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const auto entries = fx::ealib_read_dir(data, size);
    for (const auto& entry : entries) {
        fx::ealib_safe_name(entry.name);
        fx::ealib_extract(data, size, entry, /*decompress=*/true);
    }
    return 0;
}
