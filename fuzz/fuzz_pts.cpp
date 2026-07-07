// Fuzz target: the PTS aircraft screen-assets-DLL reader (pts_info) over
// untrusted overlay bytes.

#include <cstddef>
#include <cstdint>

#include <fx/pts.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    fx::PtsInfo info = fx::pts_info(data, size);
    (void)info;
    return 0;
}
