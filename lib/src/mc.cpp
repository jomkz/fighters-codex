#include "fx/mc.h"
#include "fx/cam.h"

namespace fx {

// Same PL container family as CAM — delegate rather than duplicate.

McInfo mc_info(const uint8_t* data, size_t size) {
    CamInfo c = cam_info(data, size);
    return McInfo{ c.valid, c.code };
}

std::vector<std::string> mc_strings(const uint8_t* data, size_t size,
                                    size_t min_len) {
    return cam_strings(data, size, min_len);
}

} // namespace fx
