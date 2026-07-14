// Fuzz target: the DLG menu-dialog-DLL reader (dlg_info) over untrusted bytes.

#include <cstddef>
#include <cstdint>

#include <fx/dlg.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    fx::DlgInfo info = fx::dlg_info(data, size);
    (void)info;
    for (const fx::PeImport& im : fx::dlg_imports(data, size)) {
        volatile size_t n = im.name.size();
        (void)n;
    }
    return 0;
}
