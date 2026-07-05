#include "fx/mnu.h"
#include "fx/cam.h"

namespace fx {

// MNU and CAM share the PL container family; the extraction primitives are
// identical, so this delegates rather than duplicating them.

MnuInfo mnu_info(const uint8_t* data, size_t size) {
    CamInfo c = cam_info(data, size);
    return MnuInfo{ c.valid, c.code };
}

std::vector<std::string> mnu_strings(const uint8_t* data, size_t size,
                                     size_t min_len) {
    return cam_strings(data, size, min_len);
}

} // namespace fx
