// Fuzz target: the HGR hangar-DLL reader (hgr_info) over untrusted bytes.

#include <cstddef>
#include <cstdint>

#include <fx/hgr.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    fx::HgrInfo info = fx::hgr_info(data, size);
    (void)info;
    return 0;
}
