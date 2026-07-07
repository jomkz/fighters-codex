// Fuzz target: the CAM campaign-DLL reader (cam_info), the shared cam_info
// parser behind the PL container family (CAM/MNU/PTS/MC/HGR/DLG). The parse
// walks untrusted overlay bytes under ASan; the call cannot be elided (it is a
// separate translation unit), so exercising it is enough to catch OOB reads.

#include <cstddef>
#include <cstdint>

#include <fx/cam.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    fx::CamInfo info = fx::cam_info(data, size);
    (void)info;
    return 0;
}
