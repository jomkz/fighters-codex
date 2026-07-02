#include "fx/plt.h"
#include <cstring>

namespace fx {

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
            // Next byte after this string's null terminator is the quantity;
            // walk through raw data to find this JT string and its quantity byte
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

static uint32_t read_u32le(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static PltKill read_kill(const uint8_t* base, size_t offset) {
    return { read_u32le(base + offset), read_u32le(base + offset + 4) };
}

static PltWpnSlot read_wpn_slot(const uint8_t* p) {
    return { read_u32le(p), read_u32le(p+4), read_u32le(p+8),
             read_u32le(p+12), read_u32le(p+16) };
}

static PltWpnGroup read_wpn_group(const uint8_t* base, size_t offset) {
    return { read_wpn_slot(base + offset), read_wpn_slot(base + offset + 0x14) };
}

bool plt_parse_stats(const uint8_t* data, size_t size, PltStats* st) {
    if (size < 0x21F8) return false;

    const uint8_t* b = data;
    // Mission counters (12 u32s at 0x1F80)
    st->missions_flown        = read_u32le(b + 0x1F80);
    st->wingman_missions      = read_u32le(b + 0x1F84);
    st->missions_failed       = read_u32le(b + 0x1F88);
    st->shots_fired_total     = read_u32le(b + 0x1F8C);
    st->ejections             = read_u32le(b + 0x1F90);
    st->wingman_kia           = read_u32le(b + 0x1F94);
    st->player_damage_pct     = read_u32le(b + 0x1F98);
    st->wingman_damage_pct    = read_u32le(b + 0x1F9C);
    st->player_landings       = read_u32le(b + 0x1FA0);
    st->wingman_landings      = read_u32le(b + 0x1FA4);
    st->player_landing_score  = read_u32le(b + 0x1FA8);
    st->wingman_landing_score = read_u32le(b + 0x1FAC);

    // Kill tallies (13 categories x 8 bytes at 0x1FB0)
    st->kills_air_fighter    = read_kill(b, 0x1FB0);
    st->kills_air_fighter_b  = read_kill(b, 0x1FB8);
    st->kills_air_crash      = read_kill(b, 0x1FC0);
    st->kills_naval          = read_kill(b, 0x1FC8);
    st->kills_sam            = read_kill(b, 0x1FD0);
    st->kills_aaa            = read_kill(b, 0x1FD8);
    st->kills_armor          = read_kill(b, 0x1FE0);
    st->kills_apc            = read_kill(b, 0x1FE8);
    st->kills_vehicle        = read_kill(b, 0x1FF0);
    st->kills_infantry       = read_kill(b, 0x1FF8);
    st->kills_friendly_fire  = read_kill(b, 0x2000);
    st->kills_air_nonfighter = read_kill(b, 0x2008);
    st->kills_capital_ship   = read_kill(b, 0x2010);

    // Weapon accuracy groups (8 x 0x28 bytes at 0x20B8)
    st->wpn_aa_gun       = read_wpn_group(b, 0x20B8);
    st->wpn_aa_missile   = read_wpn_group(b, 0x20E0);
    st->wpn_ground       = read_wpn_group(b, 0x2108);
    st->wpn_naval        = read_wpn_group(b, 0x2130);
    st->wpn_kill_aircraft= read_wpn_group(b, 0x2158);
    st->wpn_kill_b       = read_wpn_group(b, 0x2180);
    st->wpn_kill_c       = read_wpn_group(b, 0x21A8);
    st->wpn_kill_d       = read_wpn_group(b, 0x21D0);

    return true;
}

} // namespace fx
