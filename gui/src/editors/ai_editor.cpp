#include "ai_editor.h"
#include "../app.h"
#include "imgui.h"
#include "fx/ai.h"
#include "fx/ealib.h"
#include <string>
#include <vector>
#include <cstring>
#include <filesystem>
namespace fs = std::filesystem;

static std::string                    s_source;
static std::vector<fx::AiCompileError> s_errors;
static int  s_lastLib   = -2;
static int  s_lastEntry = -2;
static bool s_compiled  = false;

// Find the corresponding .BI entry index in the same session as the current .AI.
// Returns -1 if not found.
static int FindBiEntry(const App& app) {
    if (app.editor.libIdx < 0) return -1;
    const auto& sess = app.sessions[app.editor.libIdx];
    // Companion name: "F15.AI" -> "F15.BI"
    std::string biName = sess.entries[app.editor.entryIdx].name;
    auto dot = biName.rfind('.');
    if (dot != std::string::npos) biName.resize(dot);
    biName += ".BI";
    const fx::Entry* e = fx::ealib_find(sess.entries, biName);
    return e ? (int)(e - sess.entries.data()) : -1;
}

void DrawAiEditor(App& app) {
    auto& ed = app.editor;

    if (ed.libIdx != s_lastLib || ed.entryIdx != s_lastEntry) {
        s_lastLib   = ed.libIdx;
        s_lastEntry = ed.entryIdx;
        s_source.assign((const char*)ed.data.data(), ed.data.size());
        s_errors.clear();
        s_compiled = false;
    }

    // Linked BI info
    int biIdx = FindBiEntry(app);
    if (biIdx >= 0) {
        const char* biName = app.sessions[ed.libIdx].entries[biIdx].name;
        ImGui::TextColored({0.6f,1.0f,0.6f,1}, "Links to: %s", biName);
    } else {
        ImGui::TextDisabled("No matching .BI entry found in this session.");
    }

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 100);
    if (ImGui::Button("Compile -> BI", ImVec2(100, 0))) {
        s_errors.clear();
        s_compiled = false;
        auto biBytes = fx::ai_compile(s_source, s_errors);
        if (!biBytes.empty() && s_errors.empty()) {
            // Write BI back into session
            if (biIdx >= 0) {
                std::string biName = app.sessions[ed.libIdx].entries[biIdx].name;
                auto& sess = app.sessions[ed.libIdx];
                sess.data    = fx::ealib_patch(sess.data.data(), sess.data.size(),
                                               biName, biBytes);
                sess.entries = fx::ealib_read_dir(sess.data.data(), sess.data.size());
                sess.dirty   = true;
                app.statusMsg  = "Compiled OK â€” " + biName + " updated";
                app.statusKind = App::StatusKind::Info;
            } else {
                app.statusMsg  = "Compiled OK (no .BI entry to patch)";
                app.statusKind = App::StatusKind::Warning;
            }
            s_compiled = true;
        } else {
            app.statusMsg  = "Compile failed â€” " + std::to_string(s_errors.size()) + " error(s)";
            app.statusKind = App::StatusKind::Error;
        }
    }

    ImGui::Separator();

    // Compute editor height: leave room for error list if errors present
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float errH   = 0.0f;
    if (!s_errors.empty()) errH = ImGui::GetTextLineHeightWithSpacing() * 6.0f + 8.0f;
    float edH = avail.y - errH;
    if (edH < 60.0f) edH = 60.0f;

    // Source editor
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::BeginChild("##aiSrcScroll", ImVec2(0, edH), false,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PopStyleVar();

    float inputW = ImGui::GetContentRegionAvail().x;
    float charW  = ImGui::CalcTextSize("W").x;
    size_t maxLine = 0, cur = 0;
    for (char c : s_source) {
        if (c == '\n') { if (cur > maxLine) maxLine = cur; cur = 0; }
        else ++cur;
    }
    if (cur > maxLine) maxLine = cur;
    float naturalW = (float)(maxLine + 4) * charW;
    if (naturalW > inputW) inputW = naturalW;

    ImVec2 inputSize(inputW, ImGui::GetContentRegionAvail().y);
    if (ImGui::InputTextMultiline("##ai", s_source.data(), s_source.size() + 1,
                                  inputSize,
                                  ImGuiInputTextFlags_AllowTabInput |
                                  ImGuiInputTextFlags_CallbackResize,
                                  [](ImGuiInputTextCallbackData* d) -> int {
                                      if (d->EventFlag == ImGuiInputTextFlags_CallbackResize) {
                                          auto* s = (std::string*)d->UserData;
                                          s->resize((size_t)d->BufSize - 1);
                                          d->Buf = s->data();
                                      }
                                      return 0;
                                  }, &s_source)) {
        ed.data.assign(s_source.begin(), s_source.end());
        ed.modified = true;
        s_compiled  = false;
    }
    ImGui::EndChild();

    // Error list
    if (!s_errors.empty()) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.18f, 0.05f, 0.05f, 1.0f));
        ImGui::BeginChild("##aiErrors", ImVec2(0, errH), false);
        ImGui::PopStyleColor();
        ImGui::TextColored({1,0.4f,0.4f,1}, "Compile errors:");
        for (const auto& e : s_errors)
            ImGui::TextColored({1,0.6f,0.6f,1}, "  Line %d: %s", e.line, e.message.c_str());
        ImGui::EndChild();
    } else if (s_compiled) {
        ImGui::TextColored({0.5f,1.0f,0.5f,1}, "Compiled successfully.");
    }
}
