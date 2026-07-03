#include "mission_editor.h"
#include "../app.h"
#include "imgui.h"
#include <string>
#include <sstream>
#include <vector>

// .M / .MM / .MT mission files are all plain ASCII text.
// We show a parsed summary header above the text editor.

static std::string s_text;
static int s_lastLib   = -2;
static int s_lastEntry = -2;

// ---- lightweight line scanner -----------------------------------------------

static std::string FirstToken(const std::string& line) {
    size_t s = line.find_first_not_of(" \t\r");
    if (s == std::string::npos) return {};
    size_t e = line.find_first_of(" \t\r", s);
    return line.substr(s, e == std::string::npos ? std::string::npos : e - s);
}

static std::string RestAfterToken(const std::string& line) {
    size_t s = line.find_first_not_of(" \t\r");
    if (s == std::string::npos) return {};
    size_t e = line.find_first_of(" \t\r", s);
    if (e == std::string::npos) return {};
    s = line.find_first_not_of(" \t\r", e);
    if (s == std::string::npos) return {};
    size_t end = line.find_last_not_of("\r\n");
    return line.substr(s, end == std::string::npos ? std::string::npos : end - s + 1);
}

// Draw a two-column summary table inside a collapsed header.
// Scans for top-level keywords and obj blocks.
static void DrawMMSummary(const std::string& text) {
    std::istringstream ss(text);
    std::string line;
    std::string mapName, layerName, timeStr, codeStr;
    bool hasBrief = false;
    int objCount = 0;

    while (std::getline(ss, line)) {
        std::string tok = FirstToken(line);
        if (tok.empty() || tok[0] == ';') continue;
        std::string val = RestAfterToken(line);
        if (tok == "map")      { mapName   = val; }
        else if (tok == "layer")     { layerName  = val; }
        else if (tok == "time")      { timeStr    = val; }
        else if (tok == "code")      { codeStr    = val; }
        else if (tok == "brief")     { hasBrief   = true; }
        else if (tok == "obj")       { objCount++; }
    }

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6, 2));
    if (ImGui::BeginTable("##mmsumm", 2,
            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp,
            ImVec2(0, 0))) {
        ImGui::TableSetupColumn("Key",  ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("Val",  ImGuiTableColumnFlags_WidthStretch);

        auto Row = [](const char* k, const char* v) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", k);
            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(v);
        };

        if (!mapName.empty())   Row("Map",    mapName.c_str());
        if (!layerName.empty()) Row("Layer",  layerName.c_str());
        if (!timeStr.empty())   Row("Time",   timeStr.c_str());
        if (!codeStr.empty())   Row("Code",   codeStr.c_str());
        if (hasBrief)           Row("Brief",  "yes");
        if (objCount > 0) {
            char buf[32]; snprintf(buf, sizeof(buf), "%d", objCount);
            Row("Objects", buf);
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

// Draw section summary for .MT briefing files.
static void DrawMTSummary(const std::string& text) {
    std::istringstream ss(text);
    std::string line;
    std::vector<std::string> sections;
    std::string pending;

    while (std::getline(ss, line)) {
        std::string tok = FirstToken(line);
        if (tok == ".section") {
            if (!pending.empty()) sections.push_back(pending);
            pending = RestAfterToken(line);
        } else if (tok == "--" || (tok.size() > 2 && tok[0] == '-' && tok[1] == '-')) {
            // Title line like "--AF01  (extra01)"
            if (!pending.empty() && sections.empty())
                sections.push_back(pending + "  " + line.substr(line.find_first_not_of(" \t")));
        }
    }
    if (!pending.empty()) sections.push_back(pending);

    char buf[32]; snprintf(buf, sizeof(buf), "%d section(s)", (int)sections.size());
    ImGui::TextDisabled("%s", buf);
    for (auto& s : sections) {
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::TextUnformatted(s.c_str());
    }
}

// ---- main draw --------------------------------------------------------------

void DrawMissionEditor(App& app) {
    auto& ed = app.editor;

    if (ed.libIdx != s_lastLib || ed.entryIdx != s_lastEntry) {
        s_lastLib   = ed.libIdx;
        s_lastEntry = ed.entryIdx;
        s_text.assign((const char*)ed.data.data(), ed.data.size());
    }

    // Parsed summary header
    if (ed.ext == "mt") {
        DrawMTSummary(s_text);
    } else {
        DrawMMSummary(s_text);
    }
    ImGui::Separator();

    // Text editor
    ImVec2 avail = ImGui::GetContentRegionAvail();
    avail.y -= 36;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::BeginChild("##missionScroll", avail, false,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PopStyleVar();

    float inputW = ImGui::GetContentRegionAvail().x;
    float charW  = ImGui::CalcTextSize("W").x;
    size_t maxLine = 0, cur = 0;
    for (char c : s_text) {
        if (c == '\n') { if (cur > maxLine) maxLine = cur; cur = 0; }
        else ++cur;
    }
    if (cur > maxLine) maxLine = cur;
    float naturalW = (float)(maxLine + 4) * charW;
    if (naturalW > inputW) inputW = naturalW;

    ImVec2 inputSize(inputW, ImGui::GetContentRegionAvail().y);
    if (ImGui::InputTextMultiline("##mission", s_text.data(),
                                  s_text.size() + 1, inputSize,
                                  ImGuiInputTextFlags_AllowTabInput |
                                  ImGuiInputTextFlags_CallbackResize,
                                  [](ImGuiInputTextCallbackData* d) -> int {
                                      if (d->EventFlag == ImGuiInputTextFlags_CallbackResize) {
                                          auto* s = (std::string*)d->UserData;
                                          s->resize((size_t)d->BufSize - 1);
                                          d->Buf = s->data();
                                      }
                                      return 0;
                                  }, &s_text)) {
        ed.modified = true;
    }
    ImGui::EndChild();

    if (ImGui::Button("Save")) {
        ed.data.assign(s_text.begin(), s_text.end());
        ed.modified = true;
        app.CommitEntry(ed.data);
    }
}
