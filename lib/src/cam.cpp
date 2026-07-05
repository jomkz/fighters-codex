#include "fx/cam.h"

namespace fx {

CamInfo cam_info(const uint8_t* data, size_t size) {
    CamInfo info{};
    info.code = pe_code_section(data, size);
    info.valid = info.code.data != nullptr;
    return info;
}

std::vector<std::string> cam_strings(const uint8_t* data, size_t size,
                                     size_t min_len) {
    std::vector<std::string> result;
    std::string cur;
    for (size_t i = 0; i < size; i++) {
        uint8_t c = data[i];
        if (c >= 0x20 && c < 0x7F) {
            cur += (char)c;
        } else {
            if (cur.size() >= min_len) result.push_back(cur);
            cur.clear();
        }
    }
    if (cur.size() >= min_len) result.push_back(cur);
    return result;
}

} // namespace fx
