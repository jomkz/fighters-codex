#include "mission_editor.h"
#include "../app.h"
#include "imgui.h"
#include <string>

// .M/.MM/.MT mission files are all plain ASCII text — display and edit directly.
static std::string s_text;
static int s_lastEntry = -2;

void DrawMissionEditor(App& app) {
    auto& ed = app.editor;

    if (ed.entryIdx != s_lastEntry) {
        s_lastEntry = ed.entryIdx;
        s_text.assign((const char*)ed.data.data(), ed.data.size());
    }

    ImGui::TextDisabled("Edit mission text below. Changes are committed on Save.");
    ImGui::Separator();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    avail.y -= 36;

    // Wrap InputTextMultiline in a horizontally-scrollable child so long lines
    // are reachable. The input itself is given a large fixed width to prevent
    // word-wrap; the child provides the actual scroll.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::BeginChild("##missionScroll", avail, false,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PopStyleVar();

    float inputW = ImGui::GetContentRegionAvail().x;
    // Compute the width of the longest line so the input never wraps.
    float charW = ImGui::CalcTextSize("W").x;
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
                                  s_text.size() + 1,
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
