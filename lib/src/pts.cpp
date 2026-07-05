#include "fx/pts.h"
#include "fx/cam.h"
#include <cctype>

namespace fx {

PtsInfo pts_info(const uint8_t* data, size_t size) {
    PtsInfo info{};
    CamInfo c = cam_info(data, size);  // shared PL container family
    info.valid = c.valid;
    info.code = c.code;
    if (!info.valid) return info;

    // Exactly one PIC reference per file (PTS.md) — the aircraft icon.
    for (const std::string& s : cam_strings(data, size, 5)) {
        if (s.size() < 5) continue;
        std::string tail = s.substr(s.size() - 4);
        for (char& ch : tail) ch = (char)toupper((unsigned char)ch);
        if (tail == ".PIC") {
            info.icon = s;
            break;
        }
    }
    return info;
}

} // namespace fx
