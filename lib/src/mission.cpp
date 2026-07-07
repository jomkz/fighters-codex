#include "fx/mission.h"
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace fx {

// ---- helpers ----------------------------------------------------------

static std::string strip_leading(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return s.substr(i);
}

// Split a line into tokens (whitespace-delimited, no quote handling needed here)
static std::vector<std::string> split_tokens(const std::string& s) {
    std::vector<std::string> tokens;
    std::istringstream ss(s);
    std::string t;
    while (ss >> t) tokens.push_back(t);
    return tokens;
}

// Split into lines; bytes after the last CRLF are put in *trailer (not given a CRLF on re-emit).
static std::vector<std::string> split_lines(const uint8_t* data, size_t size,
                                              std::string* trailer = nullptr) {
    std::vector<std::string> lines;
    std::string cur;
    bool had_term = false;
    for (size_t i = 0; i < size; ++i) {
        char c = (char)data[i];
        if (c == '\r') {
            if (i + 1 < size && data[i + 1] == '\n') ++i;
            lines.push_back(cur); cur.clear(); had_term = true;
        } else if (c == '\n') {
            lines.push_back(cur); cur.clear(); had_term = true;
        } else {
            cur += c;
        }
    }
    // Unterminated bytes after the last CRLF become the trailer.
    if (!cur.empty()) {
        if (had_term && trailer) *trailer = cur;
        else lines.push_back(cur);
    }
    return lines;
}

// ---- parse_info -------------------------------------------------------

MissionInfo mission_parse_info(const uint8_t* data, size_t size) {
    MissionInfo info{};
    info.layer_index = -1;
    info.time_h = -1;
    info.time_m = -1;

    auto lines = split_lines(data, size);

    bool in_obj = false;
    for (auto& line : lines) {
        std::string s = strip_leading(line);
        if (s.empty()) continue;

        auto toks = split_tokens(s);
        if (toks.empty()) continue;

        const std::string& key = toks[0];

        if (key == "textFormat") continue;
        if (key == "brief" || key == "briefmap" ||
            key == "armplane" || key == "selectplane") {
            info.screen_flags.push_back(key);
            continue;
        }
        if (key == "obj") { in_obj = true; ++info.obj_count; continue; }
        if (key == "." && in_obj) { in_obj = false; continue; }

        if (!in_obj) {
            if (key == "map"   && toks.size() >= 2) info.map_file   = toks[1];
            if (key == "layer" && toks.size() >= 3) {
                info.layer_file  = toks[1];
                info.layer_index = atoi(toks[2].c_str());
            }
            if (key == "clouds" && toks.size() >= 2) info.clouds    = atoi(toks[1].c_str());
            if (key == "wind"   && toks.size() >= 3) {
                info.wind_dir   = atoi(toks[1].c_str());
                info.wind_speed = atoi(toks[2].c_str());
            }
            if (key == "time"   && toks.size() >= 3) {
                info.time_h = atoi(toks[1].c_str());
                info.time_m = atoi(toks[2].c_str());
            }
        }
    }
    return info;
}

// ---- object list ------------------------------------------------------

static const std::vector<std::string>* MissionField_get(
    const std::vector<MissionField>& fields, const std::string& key) {
    for (const auto& f : fields)
        if (f.key == key) return &f.values;
    return nullptr;
}

const std::vector<std::string>* MissionWaypoint::get(const std::string& key) const {
    return MissionField_get(fields, key);
}
const std::vector<std::string>* MissionObj::get(const std::string& key) const {
    return MissionField_get(fields, key);
}

MissionObjects mission_parse_objects(const uint8_t* data, size_t size) {
    MissionObjects out;
    auto& objs = out.objects;
    auto lines = split_lines(data, size);

    enum State { TOP, OBJ, WPT } state = TOP;
    MissionObj      cur;
    MissionWaypoint wcur;
    bool            have_wcur = false;

    // Flush the in-progress waypoint into the current (last-opened) block.
    auto flush_wpt = [&]() {
        if (have_wcur) {
            if (!out.waypoint_blocks.empty())
                out.waypoint_blocks.back().waypoints.push_back(std::move(wcur));
            wcur = MissionWaypoint{};
            have_wcur = false;
        }
    };

    // Structure is driven purely by block state and the delimiter keywords —
    // never by indentation, which the stock files use but the format does not
    // require. Inside a block, any line that is not a delimiter is a field.
    auto close_obj = [&]() { if (state == OBJ) { objs.push_back(std::move(cur)); cur = MissionObj{}; } };

    for (const std::string& raw : lines) {
        std::string s = strip_leading(raw);
        if (s.empty()) continue;
        auto toks = split_tokens(s);
        if (toks.empty()) continue;
        const std::string& key = toks[0];
        std::vector<std::string> vals(toks.begin() + 1, toks.end());

        // Delimiters — recognised in any state.
        if (key == ".") {                 // close the current obj / waypoint block
            close_obj();
            flush_wpt();
            state = TOP;
            continue;
        }
        if (key == "obj") {               // open an object block
            flush_wpt(); close_obj();     // defensive: recover from a missing `.`
            cur = MissionObj{};
            state = OBJ;
            continue;
        }
        if (key == "waypoint2") {         // open a waypoint block
            flush_wpt(); close_obj();
            MissionWaypointBlock blk;
            blk.count = vals.empty() ? 0 : std::atoi(vals[0].c_str());
            out.waypoint_blocks.push_back(std::move(blk));
            state = WPT;
            continue;
        }

        // Field line — meaningful only inside a block.
        if (state == OBJ) {
            if (key == "type" && !vals.empty()) {
                cur.type_file = vals[0];
            } else if (key == "pos" && vals.size() >= 3) {
                for (int i = 0; i < 3; ++i) cur.pos[i] = std::strtoll(vals[i].c_str(), nullptr, 10);
            } else if (key == "angle" && vals.size() >= 3) {
                for (int i = 0; i < 3; ++i) cur.angle[i] = std::atoi(vals[i].c_str());
            } else {
                cur.fields.push_back({ key, std::move(vals) });
            }
        } else if (state == WPT) {
            if (key == "w_index") {
                flush_wpt();
                wcur = MissionWaypoint{};
                wcur.index = vals.empty() ? 0 : std::atoi(vals[0].c_str());
                have_wcur = true;
            } else if (have_wcur) {
                wcur.fields.push_back({ key, std::move(vals) });
            }
        }
        // state == TOP: a top-level mission key (map/time/sides4/…) — ignored
        // here; mission_parse_info handles those.
    }
    flush_wpt();
    if (state == OBJ) objs.push_back(std::move(cur));  // defensive: unterminated final obj
    return out;
}

// ---- round-trip -------------------------------------------------------

std::vector<uint8_t> mission_roundtrip(const uint8_t* data, size_t size) {
    std::string trailer;
    auto lines = split_lines(data, size, &trailer);
    std::vector<uint8_t> out;
    for (auto& line : lines) {
        for (char c : line) out.push_back((uint8_t)c);
        out.push_back('\r');
        out.push_back('\n');
    }
    for (char c : trailer) out.push_back((uint8_t)c);
    return out;
}

} // namespace fx
