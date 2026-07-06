// Fuzz target: the RAW screenshot codec — mhwanh header parse, palette
// decode, and the byte-identical structural repack; the path
// `fx raw unpack` / `fx raw pack` runs over untrusted bytes.
//
// Header-declared dimensions bound the decode (it allocates
// width*height*4 from the header).

#include <cstddef>
#include <cstdint>

#include <fx/raw.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    fx::RawInfo info;
    if (!fx::raw_info(data, size, &info)) return 0;

    if ((uint64_t)info.width * info.height <= (16u << 20))
        fx::raw_decode(data, size);

    fx::raw_repack(data, size);
    return 0;
}
