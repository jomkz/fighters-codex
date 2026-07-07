// Fuzz target: the MNU menu-DLL reader (mnu_info) over untrusted overlay bytes.

#include <cstddef>
#include <cstdint>

#include <fx/mnu.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    fx::MnuInfo info = fx::mnu_info(data, size);
    (void)info;
    return 0;
}
