// Fuzz target: the MC mission-condition-DLL reader (mc_info) over untrusted bytes.

#include <cstddef>
#include <cstdint>

#include <fx/mc.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    fx::McInfo info = fx::mc_info(data, size);
    (void)info;
    return 0;
}
