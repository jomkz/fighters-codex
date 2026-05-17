#include "ft/plt.h"
#include <cstring>

namespace ft {

static std::string read_fixed(const uint8_t* data, size_t max_len) {
    size_t len = strnlen((const char*)data, max_len);
    return std::string((const char*)data, len);
}

// Scan forward from pos for null-terminated strings until we hit two consecutive
// nulls or reach end. Returns a list of all non-empty strings found.
static std::vector<std::string> scan_strings(const uint8_t* data, size_t size,
                                              size_t pos, size_t max_bytes) {
    std::vector<std::string> result;
    size_t end = std::min(pos + max_bytes, size);
    while (pos < end) {
        const char* s = (const char*)(data + pos);
        size_t max_len = end - pos;
        size_t len = strnlen(s, max_len);
        if (len == 0) {
            pos++; // skip null, keep scanning briefly
            if (result.size() > 2) break; // end of meaningful block
        } else {
            result.push_back(std::string(s, len));
            pos += len + 1;
        }
    }
    return result;
}

// Returns true if the string looks like a FA asset filename (has an extension we know)
static bool looks_like_asset(const std::string& s) {
    auto dot = s.rfind('.');
    if (dot == std::string::npos) return false;
    std::string ext = s.substr(dot);
    for (auto& c : ext) c = (char)toupper((unsigned char)c);
    return ext == ".CAM" || ext == ".PT" || ext == ".JT" || ext == ".SEE" || ext == ".ECM";
}

bool plt_parse(const uint8_t* data, size_t size, PltInfo* info) {
    if (size < 0xB0) return false;
    if (data[0x00] != 0x0F) return false;

    info->version_tag = data[0x00];
    info->name        = read_fixed(data + 0x01, 63);
    info->callsign    = read_fixed(data + 0x40, 32);
    info->voice_file  = read_fixed(data + 0x61, 13);
    info->nose_art    = read_fixed(data + 0x6E, 13);
    info->left_decal  = read_fixed(data + 0x7B, 13);
    info->right_decal = read_fixed(data + 0x88, 13);
    info->portrait    = read_fixed(data + 0x95, 13);
    info->rank        = read_fixed(data + 0xA2, 14);

    // Campaign block: scan from 0x0D7F backwards up to 512 bytes looking for a .CAM ref
    // The block is near 0x0D7F but the exact start varies; scan a window to find it.
    size_t scan_start = (size > 0x0E00) ? 0x0D60 : (size > 0x0D7F ? 0x0D60 : 0xB0);
    size_t scan_end   = std::min(size, (size_t)0x0F00);

    // Find first .CAM string in the window
    size_t cam_pos = std::string::npos;
    for (size_t i = scan_start; i + 4 < scan_end; i++) {
        if (memcmp(data + i, ".CAM", 4) == 0 || memcmp(data + i, ".cam", 4) == 0) {
            // Walk back to start of this string
            size_t start = i;
            while (start > scan_start && data[start - 1] != 0) start--;
            cam_pos = start;
            break;
        }
    }

    if (cam_pos == std::string::npos) return true; // no active campaign, identity only

    // Collect all strings from the campaign block
    auto strings = scan_strings(data, size, cam_pos, scan_end - cam_pos);

    // Classify strings by extension
    for (size_t i = 0; i < strings.size(); i++) {
        const std::string& s = strings[i];
        auto dot = s.rfind('.');
        if (dot == std::string::npos) {
            // Could be campaign display name (no extension, human-readable)
            if (!info->cam_file.empty() && info->cam_name.empty())
                info->cam_name = s;
            continue;
        }
        std::string ext = s.substr(dot);
        for (auto& c : ext) c = (char)toupper((unsigned char)c);

        if (ext == ".CAM" && info->cam_file.empty()) {
            info->cam_file = s;
        } else if (ext == ".PT") {
            if (info->aircraft.empty())
                info->aircraft = s;
            else
                info->aircraft_pool.push_back(s);
        } else if (ext == ".JT") {
            // Next byte after this string's null terminator is the quantity
            size_t str_abs = cam_pos; // recalculate absolute position
            // Walk through raw data to find this JT string and its quantity byte
            for (size_t p = cam_pos; p + s.size() < scan_end; p++) {
                if (memcmp(data + p, s.c_str(), s.size()) == 0 && data[p + s.size()] == 0) {
                    uint8_t qty = (p + s.size() + 1 < size) ? data[p + s.size() + 1] : 0;
                    info->ordnance.push_back({ s, qty });
                    break;
                }
            }
        } else if (ext == ".SEE" || ext == ".ECM") {
            info->sensors.push_back(s);
        }
    }

    return true;
}

} // namespace ft
