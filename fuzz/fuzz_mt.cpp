// Fuzz target: the MT mission-briefing-text reader (mt_info), which layers a
// briefing model over a TXT document. The fuzz input is parsed as TXT first
// (txt_read), then interpreted by mt_info — both walk untrusted bytes.

#include <cstddef>
#include <cstdint>

#include <fx/mt.h>
#include <fx/txt.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    fx::MtInfo info = fx::mt_info(fx::txt_read(data, size));
    (void)info;
    return 0;
}
