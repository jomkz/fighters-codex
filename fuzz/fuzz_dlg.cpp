// Fuzz target: the DLG menu-dialog-DLL reader (dlg_info) over untrusted bytes.

#include <cstddef>
#include <cstdint>

#include <fx/dlg.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    fx::DlgInfo info = fx::dlg_info(data, size);
    (void)info;
    return 0;
}
