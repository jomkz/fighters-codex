#include "fx/hud.h"
#include "fx/pe.h"
#include <cstring>

namespace fx {

static uint16_t u16le(const uint8_t* p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

// ---- Gauge parameter table --------------------------------------------------

// Offsets are within the CODE section data, which the engine bulk-copies to
// DAT_00521360 at load time. Confirmed via Ghidra traces of HUD draw functions.
struct GaugeEntry {
    uint16_t offset;
    const char* gauge;
    const char* field;
    enum Type { I32, S16, S8, U8 } type;
};

// The three tape gauges store their parameters as 32-bit ints, not 16 (#491).
//
// The table's own stride says so — 0x1E1 → 0x1E5 → 0x1E9 → 0x1ED is four bytes apart, and
// the fields at 0x231+ that really ARE 16-bit sit two apart. The bytes confirm it: across
// all 46 shipped HUDs every one of these fields is sign-extended over four bytes
// (A7.HUD: `d8 ff ff ff` = -40, `c8 ff ff ff` = -56), and 44 of the 46 carry a negative
// value in at least one of them, so the high half is not incidental zero padding.
//
// Reading them as S16 LOOKED right, because the low half is the value. WRITING them as
// S16 was the corruption: `fx hud set A7.HUD speed_tape.dx=40` wrote `28 00` and left the
// old `ff ff` in the high half, so the engine read -65496. No round-trip test could see
// it, because an UNEDITED repack rewrites the same bytes it read.
static const GaugeEntry kGauges[] = {
    {0x1E1, "heading_tape",       "width",            GaugeEntry::I32},
    {0x1E5, "heading_tape",       "dy",               GaugeEntry::I32},
    // 0x1E9 is a fourth 4-byte field of this gauge (910 in all 46 HUDs). It is left out
    // rather than named: constant everywhere, so the assets alone cannot say what it is.
    {0x1ED, "heading_tape",       "tick_spacing",     GaugeEntry::I32},
    {0x1F7, "speed_tape",         "dx",               GaugeEntry::I32},
    {0x1FB, "speed_tape",         "dy",               GaugeEntry::I32},
    {0x1FF, "speed_tape",         "height",           GaugeEntry::I32},
    {0x209, "speed_tape",         "tick_increment",   GaugeEntry::I32},
    {0x214, "altitude_tape",      "dx",               GaugeEntry::I32},
    {0x218, "altitude_tape",      "dy",               GaugeEntry::I32},
    {0x21C, "altitude_tape",      "height",           GaugeEntry::I32},
    {0x226, "altitude_tape",      "tick_increment",   GaugeEntry::I32},
    {0x231, "flight_path_marker", "dx",               GaugeEntry::S16},
    {0x233, "flight_path_marker", "dy",               GaugeEntry::S16},
    {0x235, "flight_path_marker", "box_half_width",   GaugeEntry::S16},
    {0x237, "flight_path_marker", "box_half_height",  GaugeEntry::S16},
    {0x239, "lock_indicator",     "flag_a",           GaugeEntry::U8},
    {0x23A, "lock_indicator",     "flag_b",           GaugeEntry::U8},
    {0x23B, "hud",                "center_dot_enable",GaugeEntry::U8},
    {0x23C, "ecm_bar",            "enable",           GaugeEntry::U8},
    {0x23D, "active_lock_threat", "enable",           GaugeEntry::U8},
    {0x23E, "hvel",               "alt_max",          GaugeEntry::S16},
    {0x240, "lead_indicator",     "enable",           GaugeEntry::U8},
    {0x241, "score_indicator",    "dx",               GaugeEntry::S8},
    {0x243, "score_indicator",    "dy",               GaugeEntry::S8},
    {0x265, "warning_lights",     "dx",               GaugeEntry::S16},
    {0x267, "warning_lights",     "dy",               GaugeEntry::S16},
    {0x269, "throttle_readout",   "dx",               GaugeEntry::S16},
    {0x26B, "throttle_readout",   "dy",               GaugeEntry::S16},
    {0x26D, "weapon_info",        "dx",               GaugeEntry::S16},
    {0x26F, "weapon_info",        "dy",               GaugeEntry::S16},
    {0x271, "range_info",         "dx",               GaugeEntry::S16},
    {0x273, "range_info",         "dy",               GaugeEntry::S16},
};

static int32_t rd_i32(const uint8_t* cs, size_t sz, uint32_t off) {
    if (off + 4 > sz) return 0;
    return (int32_t)((uint32_t)cs[off] | ((uint32_t)cs[off + 1] << 8)
                     | ((uint32_t)cs[off + 2] << 16) | ((uint32_t)cs[off + 3] << 24));
}
static int16_t rd_s16(const uint8_t* cs, size_t sz, uint32_t off) {
    if (off + 2 > sz) return 0;
    return (int16_t)u16le(cs + off);
}
static int8_t rd_s8(const uint8_t* cs, size_t sz, uint32_t off) {
    if (off >= sz) return 0;
    return (int8_t)cs[off];
}
static uint8_t rd_u8(const uint8_t* cs, size_t sz, uint32_t off) {
    if (off >= sz) return 0;
    return cs[off];
}

// Read a null-terminated string from the code section, up to max_len bytes.
static std::string rd_str(const uint8_t* cs, size_t sz, uint32_t off, size_t max_len = 8) {
    std::string s;
    for (size_t i = 0; i < max_len && off + i < sz && cs[off + i] != 0; i++)
        s += (char)cs[off + i];
    return s;
}

// ---- Public API -------------------------------------------------------------

HudFile hud_parse(const uint8_t* data, size_t size) {
    HudFile result{};
    CodeSection cs = pe_code_section(data, size);
    if (!cs.data || cs.size < 0x2BB) return result;

    // Scan printable null-terminated strings in two passes: the string region
    // before the gauge params (offsets 1–0x1E0), and the string region after
    // the gauge params (offsets 0x275 onward). Strings are asset references
    // (~<ac>, ~<ac>h, hudsym, winfont, etc.) and sub-panel sprite suffixes.
    auto scan_strings = [&](size_t from, size_t to) {
        size_t i = from;
        while (i < to && i < cs.size) {
            if (cs.data[i] == 0 || cs.data[i] <= 0x20 || cs.data[i] >= 0x7F) { i++; continue; }
            std::string s;
            while (i < cs.size && cs.data[i] > 0x20 && cs.data[i] < 0x7F)
                s += (char)cs.data[i++];
            if (s.size() >= 3) result.asset_strings.push_back(std::move(s));
            if (i < cs.size && cs.data[i] == 0) i++;
        }
    };
    scan_strings(1, 0x1E1);    // before gauge params
    scan_strings(0x275, cs.size); // after gauge params

    // Advisory icon labels -- four sequential 8-byte entries starting at VA 0x1245.
    // Order confirmed from A7.HUD: GEAR, FLAP, BRAKE, HOOK/BAY.
    result.icon_a = rd_str(cs.data, cs.size, 0x245);
    result.icon_b = rd_str(cs.data, cs.size, 0x24D);
    result.icon_c = rd_str(cs.data, cs.size, 0x255);
    result.icon_d = rd_str(cs.data, cs.size, 0x25D);

    // Gauge parameters
    for (const auto& g : kGauges) {
        HudParam p;
        p.gauge = g.gauge;
        p.field = g.field;
        switch (g.type) {
        case GaugeEntry::I32: p.value = rd_i32(cs.data, cs.size, g.offset); break;
        case GaugeEntry::S16: p.value = rd_s16(cs.data, cs.size, g.offset); break;
        case GaugeEntry::S8:  p.value = (int32_t)rd_s8(cs.data, cs.size, g.offset); break;
        case GaugeEntry::U8:  p.value = (int32_t)rd_u8(cs.data, cs.size, g.offset); break;
        }
        result.params.push_back(std::move(p));
    }

    result.valid = true;
    return result;
}

// Write an icon label into its fixed 8-byte slot: content, then a NUL when
// shorter than the slot (mirrors rd_str, which stops at a NUL or 8 bytes).
// Slot bytes past the NUL carry over verbatim.
static bool wr_icon(uint8_t* cs, size_t sz, uint32_t off, const std::string& s) {
    if (s.size() > 8) return false;
    if (off + 8 > sz) return false;
    memcpy(cs + off, s.data(), s.size());
    if (s.size() < 8) cs[off + s.size()] = 0;
    return true;
}

std::vector<uint8_t> hud_repack(const uint8_t* orig, size_t orig_size,
                                const HudFile& hud) {
    CodeSection cs = pe_code_section(orig, orig_size);
    if (!cs.data || cs.size < 0x2BB) return {};

    const size_t n_gauges = sizeof(kGauges) / sizeof(kGauges[0]);
    if (hud.params.size() != n_gauges) return {};

    std::vector<uint8_t> out(orig, orig + orig_size);
    uint8_t* ocs = out.data() + (cs.data - orig);

    // Every known gauge field must be supplied exactly once (any order);
    // unknown names reject rather than silently no-op.
    std::vector<bool> used(hud.params.size(), false);
    for (const auto& g : kGauges) {
        int found = -1;
        for (size_t i = 0; i < hud.params.size(); ++i) {
            if (used[i]) continue;
            if (hud.params[i].gauge == g.gauge && hud.params[i].field == g.field) {
                found = (int)i;
                break;
            }
        }
        if (found < 0) return {};
        used[(size_t)found] = true;

        const int32_t v = hud.params[(size_t)found].value;
        switch (g.type) {
        case GaugeEntry::I32:
            // All four bytes, every time. Writing only the low half is what corrupted the
            // field: the stale high half turned 40 into -65496 for the engine (#491).
            ocs[g.offset]     = (uint8_t)((uint32_t)v);
            ocs[g.offset + 1] = (uint8_t)((uint32_t)v >> 8);
            ocs[g.offset + 2] = (uint8_t)((uint32_t)v >> 16);
            ocs[g.offset + 3] = (uint8_t)((uint32_t)v >> 24);
            break;
        case GaugeEntry::S16:
            if (v < -32768 || v > 32767) return {};
            ocs[g.offset]     = (uint8_t)v;
            ocs[g.offset + 1] = (uint8_t)((uint16_t)(int16_t)v >> 8);
            break;
        case GaugeEntry::S8:
            if (v < -128 || v > 127) return {};
            ocs[g.offset] = (uint8_t)(int8_t)v;
            break;
        case GaugeEntry::U8:
            if (v < 0 || v > 255) return {};
            ocs[g.offset] = (uint8_t)v;
            break;
        }
    }

    if (!wr_icon(ocs, cs.size, 0x245, hud.icon_a)) return {};
    if (!wr_icon(ocs, cs.size, 0x24D, hud.icon_b)) return {};
    if (!wr_icon(ocs, cs.size, 0x255, hud.icon_c)) return {};
    if (!wr_icon(ocs, cs.size, 0x25D, hud.icon_d)) return {};

    // asset_strings are informational: the string regions carry verbatim.
    return out;
}

} // namespace fx
