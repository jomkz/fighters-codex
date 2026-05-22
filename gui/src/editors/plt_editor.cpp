#include "plt_editor.h"
#include "../app.h"
#include "fx/plt.h"
#include "imgui.h"
#include <cstring>
#include <cstdio>

using namespace fx;

// Identity block offsets
static const int OFF_NAME     = 0x01;  // 63 bytes
static const int OFF_CALLSIGN = 0x40;  // 32 bytes
static const int OFF_VOICE    = 0x61;  // 13 bytes
static const int OFF_NOSE     = 0x6E;  // 13 bytes
static const int OFF_LEFT     = 0x7B;  // 13 bytes
static const int OFF_RIGHT    = 0x88;  // 13 bytes
static const int OFF_PORTRAIT = 0x95;  // 13 bytes
static const int OFF_RANK     = 0xA2;  // 14 bytes
static const int PLT_MIN_SIZE = 0xB0;

static char s_name[64]     = {};
static char s_callsign[33] = {};
static char s_voice[14]    = {};
static char s_nose[14]     = {};
static char s_left[14]     = {};
static char s_right[14]    = {};
static char s_portrait[14] = {};
static char s_rank[15]     = {};
static PltStats s_stats    = {};
static bool s_hasStats     = false;
static int  s_lastEntry    = -2;

static void ReadField(const std::vector<uint8_t>& data, int off, char* buf, int len) {
    if (off + len > (int)data.size()) { buf[0] = 0; return; }
    memcpy(buf, data.data() + off, (size_t)(len - 1));
    buf[len - 1] = 0;
}
static void WriteField(std::vector<uint8_t>& data, int off, const char* buf, int len) {
    if (off + len > (int)data.size()) return;
    memset(data.data() + off, 0, (size_t)len);
    strncpy_s((char*)data.data() + off, (size_t)len, buf, (size_t)(len - 1));
}

static void ShowKillRow(const char* label, const PltKill& k) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1); ImGui::Text("%u", k.player);
    ImGui::TableSetColumnIndex(2); ImGui::Text("%u", k.wingman);
}

static void ShowWpnGroup(const char* label, const PltWpnGroup& g) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1); ImGui::Text("%u", g.player.damage_total);
    ImGui::TableSetColumnIndex(2); ImGui::Text("%u", g.player.shots_fired);
    ImGui::TableSetColumnIndex(3); ImGui::Text("%u", g.player.hits);
    ImGui::TableSetColumnIndex(4); ImGui::Text("%u", g.player.kills);
    ImGui::TableSetColumnIndex(5); ImGui::Text("%u", g.wingman.damage_total);
    ImGui::TableSetColumnIndex(6); ImGui::Text("%u", g.wingman.shots_fired);
    ImGui::TableSetColumnIndex(7); ImGui::Text("%u", g.wingman.hits);
    ImGui::TableSetColumnIndex(8); ImGui::Text("%u", g.wingman.kills);
}

void DrawPltEditor(App& app) {
    auto& ed = app.editor;

    if ((int)ed.data.size() < PLT_MIN_SIZE) {
        ImGui::TextColored({1,0.4f,0.4f,1},
            "File too small to be a valid PLT (%zu bytes).", ed.data.size());
        return;
    }

    if (ed.entryIdx != s_lastEntry) {
        s_lastEntry = ed.entryIdx;
        ReadField(ed.data, OFF_NAME,     s_name,     sizeof(s_name));
        ReadField(ed.data, OFF_CALLSIGN, s_callsign, sizeof(s_callsign));
        ReadField(ed.data, OFF_VOICE,    s_voice,    sizeof(s_voice));
        ReadField(ed.data, OFF_NOSE,     s_nose,     sizeof(s_nose));
        ReadField(ed.data, OFF_LEFT,     s_left,     sizeof(s_left));
        ReadField(ed.data, OFF_RIGHT,    s_right,    sizeof(s_right));
        ReadField(ed.data, OFF_PORTRAIT, s_portrait, sizeof(s_portrait));
        ReadField(ed.data, OFF_RANK,     s_rank,     sizeof(s_rank));
        s_hasStats = plt_parse_stats(ed.data.data(), ed.data.size(), &s_stats);
    }

    // â”€â”€ Identity â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    ImGui::SeparatorText("Identity");

    bool changed = false;
    auto field = [&](const char* label, char* buf, int bufSize) {
        ImGui::SetNextItemWidth(220);
        if (ImGui::InputText(label, buf, (size_t)bufSize))
            changed = true;
    };

    field("Pilot name",    s_name,     sizeof(s_name));
    field("Callsign",      s_callsign, sizeof(s_callsign));
    field("Voice file",    s_voice,    sizeof(s_voice));
    field("Rank",          s_rank,     sizeof(s_rank));

    ImGui::SeparatorText("Cosmetics");
    field("Portrait ID",    s_portrait, sizeof(s_portrait));
    field("Nose art ID",    s_nose,     sizeof(s_nose));
    field("Left decal ID",  s_left,     sizeof(s_left));
    field("Right decal ID", s_right,    sizeof(s_right));

    if (changed) {
        WriteField(ed.data, OFF_NAME,     s_name,     sizeof(s_name));
        WriteField(ed.data, OFF_CALLSIGN, s_callsign, sizeof(s_callsign));
        WriteField(ed.data, OFF_VOICE,    s_voice,    sizeof(s_voice));
        WriteField(ed.data, OFF_NOSE,     s_nose,     sizeof(s_nose));
        WriteField(ed.data, OFF_LEFT,     s_left,     sizeof(s_left));
        WriteField(ed.data, OFF_RIGHT,    s_right,    sizeof(s_right));
        WriteField(ed.data, OFF_PORTRAIT, s_portrait, sizeof(s_portrait));
        WriteField(ed.data, OFF_RANK,     s_rank,     sizeof(s_rank));
        ed.modified = true;
    }

    // â”€â”€ Stats â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    ImGui::SeparatorText("Stats");

    if (!s_hasStats) {
        ImGui::TextDisabled("Stats block not present (file < 0x21F8 bytes).");
    } else {
        const auto& s = s_stats;

        // Mission counters
        if (ImGui::TreeNodeEx("Mission counters", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Missions flown:     %u  (wingman: %u)", s.missions_flown, s.wingman_missions);
            ImGui::Text("Missions failed:    %u", s.missions_failed);
            ImGui::Text("Total shots fired:  %u", s.shots_fired_total);
            ImGui::Text("Ejections:          %u", s.ejections);
            ImGui::Text("Wingman KIA:        %u", s.wingman_kia);
            ImGui::Text("Player landings:    %u  (score: %u)", s.player_landings, s.player_landing_score);
            ImGui::Text("Wingman landings:   %u  (score: %u)", s.wingman_landings, s.wingman_landing_score);
            ImGui::TreePop();
        }

        // Kill tallies
        if (ImGui::TreeNodeEx("Kill tallies", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("kills", 3,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Player",   ImGuiTableColumnFlags_WidthFixed, 60);
                ImGui::TableSetupColumn("Wingman",  ImGuiTableColumnFlags_WidthFixed, 60);
                ImGui::TableHeadersRow();
                ShowKillRow("Aircraft / fighters", s.kills_air_fighter);
                ShowKillRow("Fighters (type B)",   s.kills_air_fighter_b);
                ShowKillRow("Aircraft (crash/BA)", s.kills_air_crash);
                ShowKillRow("Naval vessels",        s.kills_naval);
                ShowKillRow("SAM launchers",        s.kills_sam);
                ShowKillRow("AAA guns",             s.kills_aaa);
                ShowKillRow("Armor / tanks",        s.kills_armor);
                ShowKillRow("APCs",                 s.kills_apc);
                ShowKillRow("Vehicles / trucks",    s.kills_vehicle);
                ShowKillRow("Infantry",             s.kills_infantry);
                ShowKillRow("Friendly fire",        s.kills_friendly_fire);
                ShowKillRow("Air (non-fighter)",    s.kills_air_nonfighter);
                ShowKillRow("Capital ships",        s.kills_capital_ship);
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }

        // Weapon accuracy
        if (ImGui::TreeNode("Weapon accuracy")) {
            if (ImGui::BeginTable("wpn", 9,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollX |
                    ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn("Group",    ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("P.dmg",    ImGuiTableColumnFlags_WidthFixed, 54);
                ImGui::TableSetupColumn("P.shots",  ImGuiTableColumnFlags_WidthFixed, 54);
                ImGui::TableSetupColumn("P.hits",   ImGuiTableColumnFlags_WidthFixed, 54);
                ImGui::TableSetupColumn("P.kills",  ImGuiTableColumnFlags_WidthFixed, 54);
                ImGui::TableSetupColumn("W.dmg",    ImGuiTableColumnFlags_WidthFixed, 54);
                ImGui::TableSetupColumn("W.shots",  ImGuiTableColumnFlags_WidthFixed, 54);
                ImGui::TableSetupColumn("W.hits",   ImGuiTableColumnFlags_WidthFixed, 54);
                ImGui::TableSetupColumn("W.kills",  ImGuiTableColumnFlags_WidthFixed, 54);
                ImGui::TableHeadersRow();
                ShowWpnGroup("AA gun",       s.wpn_aa_gun);
                ShowWpnGroup("AA missile",   s.wpn_aa_missile);
                ShowWpnGroup("Ground atk",   s.wpn_ground);
                ShowWpnGroup("Naval atk",    s.wpn_naval);
                ShowWpnGroup("Kill/aircraft",s.wpn_kill_aircraft);
                ShowWpnGroup("Kill/type B",  s.wpn_kill_b);
                ShowWpnGroup("Kill/type C",  s.wpn_kill_c);
                ShowWpnGroup("Kill/type D",  s.wpn_kill_d);
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }

        ImGui::Spacing();
    }

    // â”€â”€ Gap Explorer â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    ImGui::SeparatorText("Gap Explorer");
    ImGui::TextDisabled("Probe workflow: set bytes below, Save, launch FA.EXE, observe what changes.");
    ImGui::Spacing();

    // Renders a compact editable hex grid for ed.data[base .. base+len).
    // Returns true if any byte was modified.
    auto GapHexGrid = [&](int base, int len) -> bool {
        if (base + len > (int)ed.data.size()) {
            ImGui::TextDisabled("(file too small â€” %d bytes needed)", base + len);
            return false;
        }
        bool changed = false;
        const int COLS = 8;
        for (int i = 0; i < len; i += COLS) {
            ImGui::Text("%04X", base + i);
            for (int j = 0; j < COLS && i + j < len; j++) {
                int idx = base + i + j;
                uint8_t v = ed.data[idx];
                char lbl[16];
                snprintf(lbl, sizeof(lbl), "##g%04x", idx);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(34);
                if (ImGui::InputScalar(lbl, ImGuiDataType_U8, &v, nullptr, nullptr, "%02X",
                                       ImGuiInputTextFlags_CharsHexadecimal)) {
                    ed.data[idx] = v;
                    changed = true;
                }
            }
        }
        return changed;
    };

    // Renders just the non-zero bytes (offset + value) to quickly spot prior edits.
    auto ShowNonZero = [&](int base, int len) {
        if (base + len > (int)ed.data.size()) return;
        int count = 0;
        for (int i = 0; i < len; i++)
            if (ed.data[base + i]) count++;
        if (count == 0) { ImGui::TextDisabled("  (all zero)"); return; }
        ImGui::TextDisabled("  %d non-zero byte(s):", count);
        for (int i = 0; i < len; i++) {
            uint8_t v = ed.data[base + i];
            if (v) ImGui::TextDisabled("    %04X = %02X", base + i, v);
        }
    };

    if (ImGui::TreeNodeEx("Gap 1 â€” 0xB0â€“0xC1  (18 bytes)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextDisabled("Adjacent to rank string. Likely rank index, score tier, or medal count.");
        ShowNonZero(0xB0, 18);
        if (GapHexGrid(0xB0, 18)) ed.modified = true;
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Gap 2 â€” 0xCFâ€“0x5AE  (1344 bytes, first 128 shown)")) {
        ImGui::TextDisabled("Likely variable-length mission log text (null-terminated strings).");
        ImGui::TextDisabled("Edit as text with a hex editor for full access; probe first 128 bytes here.");
        ShowNonZero(0xCF, 1344);
        if (GapHexGrid(0xCF, 128)) ed.modified = true;
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Gap 3 â€” 0x2018â€“0x20B7  (160 bytes)")) {
        ImGui::TextDisabled("Between kill tallies (ends 0x2017) and weapon accuracy (starts 0x20B8).");
        ShowNonZero(0x2018, 0xA0);
        if (GapHexGrid(0x2018, 0xA0)) ed.modified = true;
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Gap 4 â€” 0x21F8â€“0x25DF  (~1000 bytes, first 128 shown)")) {
        ImGui::TextDisabled("Fort/campaign-phase stats. Populated only after fort-assault missions.");
        ShowNonZero(0x21F8, 0x3E8);
        if (GapHexGrid(0x21F8, 128)) ed.modified = true;
        ImGui::TreePop();
    }

    if (ed.modified && ImGui::Button("Save")) {
        app.CommitEntry(ed.data);
    }
}
