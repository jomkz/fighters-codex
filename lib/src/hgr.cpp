#include "fx/hgr.h"
#include "fx/cam.h"
#include <cctype>

namespace fx {

// Same PL container family as CAM — delegate rather than duplicate.

HgrInfo hgr_info(const uint8_t* data, size_t size) {
    HgrInfo info{};
    CamInfo c = cam_info(data, size);
    info.valid = c.valid;
    info.code = c.code;
    if (!info.valid) return info;

    for (const std::string& s : cam_strings(data, size, 5)) {
        if (s.size() < 5) continue;
        std::string tail = s.substr(s.size() - 4);
        for (char& ch : tail) ch = (char)toupper((unsigned char)ch);
        if (tail == ".PIC") info.pics.push_back(s);
    }
    return info;
}

std::vector<std::string> hgr_strings(const uint8_t* data, size_t size,
                                     size_t min_len) {
    return cam_strings(data, size, min_len);
}

} // namespace fx
