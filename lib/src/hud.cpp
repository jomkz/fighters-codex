#include "ft/hud.h"
#include "ft/pe.h"
#include <cstring>

namespace ft {

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
    enum Type { S16, S8, U8 } type;
};

static const GaugeEntry kGauges[] = {
    {0x1E1, "heading_tape",       "width",            GaugeEntry::S16},
    {0x1E5, "heading_tape",       "dy",               GaugeEntry::S16},
    {0x1ED, "heading_tape",       "tick_spacing",     GaugeEntry::S16},
    {0x1F7, "speed_tape",         "dx",               GaugeEntry::S16},
    {0x1FB, "speed_tape",         "dy",               GaugeEntry::S16},
    {0x1FF, "speed_tape",         "height",           GaugeEntry::S16},
    {0x209, "speed_tape",         "tick_increment",   GaugeEntry::S16},
    {0x214, "altitude_tape",      "dx",               GaugeEntry::S16},
    {0x218, "altitude_tape",      "dy",               GaugeEntry::S16},
    {0x21C, "altitude_tape",      "height",           GaugeEntry::S16},
    {0x226, "altitude_tape",      "tick_increment",   GaugeEntry::S16},
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
        case GaugeEntry::S16: p.value = rd_s16(cs.data, cs.size, g.offset); break;
        case GaugeEntry::S8:  p.value = (int16_t)rd_s8(cs.data, cs.size, g.offset); break;
        case GaugeEntry::U8:  p.value = (int16_t)rd_u8(cs.data, cs.size, g.offset); break;
        }
        result.params.push_back(std::move(p));
    }

    result.valid = true;
    return result;
}

} // namespace ft
