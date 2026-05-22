#include "txt_editor.h"
#include "../app.h"
#include "imgui.h"
#include <string>

static std::string s_text;
static int s_lastLib   = -2;
static int s_lastEntry = -2;

void DrawTxtEditor(App& app) {
    auto& ed = app.editor;

    if (ed.libIdx != s_lastLib || ed.entryIdx != s_lastEntry) {
        s_lastLib   = ed.libIdx;
        s_lastEntry = ed.entryIdx;
        s_text.assign((const char*)ed.data.data(), ed.data.size());
    }

    ImGui::TextDisabled("%zu bytes", ed.data.size());
    ImGui::Separator();

    ImVec2 avail = ImGui::GetContentRegionAvail();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::BeginChild("##txtScroll", avail, false,
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
    ImGui::InputTextMultiline("##txt", s_text.data(), s_text.size() + 1, inputSize,
                              ImGuiInputTextFlags_ReadOnly);
    ImGui::EndChild();
}
