#include "fx/plt.h"
#include <algorithm>
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

// ─── Write path (issue #103) ────────────────────────────────────────────────
// plt_write overlays only the fixed-offset mapped fields onto a copy of the
// original bytes; the four unmapped gap regions and the variable-length
// campaign/ordnance region pass through verbatim.

// Overlay a null-padded fixed-width string field. `out` still holds the
// original bytes (plt_write copied raw and fields are disjoint), so if `value`
// matches what those bytes already decode to, the field is left untouched —
// any stale bytes past the terminator pass through and the round-trip stays
// byte-identical. Only a genuine edit zero-fills and rewrites the field (up to
// `len` bytes, so a value that fills the field with no terminator is kept).
static void overlay_field(std::vector<uint8_t>& out, size_t off, size_t len,
                          const std::string& value) {
    size_t cur_len = strnlen((const char*)out.data() + off, len);
    if (value.size() == cur_len &&
        std::memcmp(out.data() + off, value.data(), cur_len) == 0)
        return;
    std::memset(out.data() + off, 0, len);
    std::memcpy(out.data() + off, value.data(), std::min(value.size(), len));
}

static void write_u32le(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v);        p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);  p[3] = (uint8_t)(v >> 24);
}

static void write_kill(uint8_t* base, size_t offset, const PltKill& k) {
    write_u32le(base + offset,     k.player);
    write_u32le(base + offset + 4, k.wingman);
}

static void write_wpn_slot(uint8_t* p, const PltWpnSlot& s) {
    write_u32le(p,      s.damage_total);
    write_u32le(p + 4,  s.shots_fired);
    write_u32le(p + 8,  s.hits);
    write_u32le(p + 12, s.type3);
    write_u32le(p + 16, s.kills);
}

static void write_wpn_group(uint8_t* base, size_t offset, const PltWpnGroup& g) {
    write_wpn_slot(base + offset,        g.player);
    write_wpn_slot(base + offset + 0x14, g.wingman);
}

static void overlay_stats(std::vector<uint8_t>& out, const PltStats& st) {
    uint8_t* b = out.data();
    // Mission counters (12 u32s at 0x1F80)
    write_u32le(b + 0x1F80, st.missions_flown);
    write_u32le(b + 0x1F84, st.wingman_missions);
    write_u32le(b + 0x1F88, st.missions_failed);
    write_u32le(b + 0x1F8C, st.shots_fired_total);
    write_u32le(b + 0x1F90, st.ejections);
    write_u32le(b + 0x1F94, st.wingman_kia);
    write_u32le(b + 0x1F98, st.player_damage_pct);
    write_u32le(b + 0x1F9C, st.wingman_damage_pct);
    write_u32le(b + 0x1FA0, st.player_landings);
    write_u32le(b + 0x1FA4, st.wingman_landings);
    write_u32le(b + 0x1FA8, st.player_landing_score);
    write_u32le(b + 0x1FAC, st.wingman_landing_score);

    // Kill tallies (13 categories x 8 bytes at 0x1FB0)
    write_kill(b, 0x1FB0, st.kills_air_fighter);
    write_kill(b, 0x1FB8, st.kills_air_fighter_b);
    write_kill(b, 0x1FC0, st.kills_air_crash);
    write_kill(b, 0x1FC8, st.kills_naval);
    write_kill(b, 0x1FD0, st.kills_sam);
    write_kill(b, 0x1FD8, st.kills_aaa);
    write_kill(b, 0x1FE0, st.kills_armor);
    write_kill(b, 0x1FE8, st.kills_apc);
    write_kill(b, 0x1FF0, st.kills_vehicle);
    write_kill(b, 0x1FF8, st.kills_infantry);
    write_kill(b, 0x2000, st.kills_friendly_fire);
    write_kill(b, 0x2008, st.kills_air_nonfighter);
    write_kill(b, 0x2010, st.kills_capital_ship);

    // Weapon accuracy groups (8 x 0x28 bytes at 0x20B8)
    write_wpn_group(b, 0x20B8, st.wpn_aa_gun);
    write_wpn_group(b, 0x20E0, st.wpn_aa_missile);
    write_wpn_group(b, 0x2108, st.wpn_ground);
    write_wpn_group(b, 0x2130, st.wpn_naval);
    write_wpn_group(b, 0x2158, st.wpn_kill_aircraft);
    write_wpn_group(b, 0x2180, st.wpn_kill_b);
    write_wpn_group(b, 0x21A8, st.wpn_kill_c);
    write_wpn_group(b, 0x21D0, st.wpn_kill_d);
}

bool plt_read(const uint8_t* data, size_t size, PltFile* out) {
    if (!plt_parse(data, size, &out->info)) return false;
    out->raw.assign(data, data + size);
    out->has_stats = plt_parse_stats(data, size, &out->stats);
    return true;
}

std::vector<uint8_t> plt_write(const PltFile& f) {
    if (f.raw.size() < 0xB0) return {};  // no valid identity block to overlay

    std::vector<uint8_t> out = f.raw;    // pass-through backbone
    out[0x00] = f.info.version_tag;
    overlay_field(out, 0x01, 63, f.info.name);
    overlay_field(out, 0x40, 32, f.info.callsign);
    overlay_field(out, 0x61, 13, f.info.voice_file);
    overlay_field(out, 0x6E, 13, f.info.nose_art);
    overlay_field(out, 0x7B, 13, f.info.left_decal);
    overlay_field(out, 0x88, 13, f.info.right_decal);
    overlay_field(out, 0x95, 13, f.info.portrait);
    overlay_field(out, 0xA2, 14, f.info.rank);

    if (f.has_stats && out.size() >= 0x21F8)
        overlay_stats(out, f.stats);

    return out;
}

std::vector<uint8_t> plt_repack(const uint8_t* data, size_t size) {
    PltFile f;
    if (!plt_read(data, size, &f)) return {};
    return plt_write(f);
}

} // namespace fx
