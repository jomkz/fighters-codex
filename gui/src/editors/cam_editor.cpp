#include "cam_editor.h"
#include "../app.h"
#include "imgui.h"
#include <cstdint>
#include <cstdio>
#include <cctype>
#include <string>
#include <vector>

// CAM — Campaign file. Phar Lap PE DLL, format partially unknown.
// We extract embedded null-terminated strings and show a hex dump of the header.

static std::vector<std::string> ExtractStrings(const uint8_t* data, size_t size) {
    std::vector<std::string> result;
    std::string cur;
    for (size_t i = 0; i < size; i++) {
        uint8_t c = data[i];
        if (c >= 0x20 && c < 0x7F) {
            cur += (char)c;
        } else {
            if (cur.size() >= 3)   // discard very short fragments
                result.push_back(cur);
            cur.clear();
        }
    }
    if (cur.size() >= 3) result.push_back(cur);
    return result;
}

void DrawCamEditor(App& app) {
    auto& ed = app.editor;

    const uint8_t* data = ed.data.data();
    size_t size = ed.data.size();

    ImGui::TextDisabled("Campaign file — format partially unknown  |  %zu bytes", size);
    ImGui::Separator();

    // Header hex dump (first 256 bytes)
    if (ImGui::CollapsingHeader("Header Hex Dump (first 256 bytes)")) {
        size_t dispSize = size < 256 ? size : 256;

        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(3,2));
        if (ImGui::BeginTable("##camhex", 3,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("Addr",  ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableSetupColumn("Hex",   ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("ASCII", ImGuiTableColumnFlags_WidthFixed, 128);
            ImGui::TableHeadersRow();

            char hexBuf[16*3+1];
            char asciiBuf[17];
            for (size_t row = 0; row < dispSize; row += 16) {
                size_t rowEnd = row + 16 < dispSize ? row + 16 : dispSize;
                size_t rowLen = rowEnd - row;
                size_t hp = 0;
                for (size_t i = 0; i < rowLen; i++) {
                    if (i > 0) hexBuf[hp++] = ' ';
                    snprintf(hexBuf + hp, 4, "%02X", data[row+i]);
                    hp += 2;
                }
                hexBuf[hp] = '\0';
                for (size_t i = 0; i < rowLen; i++)
                    asciiBuf[i] = isprint(data[row+i]) ? (char)data[row+i] : '.';
                asciiBuf[rowLen] = '\0';

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%04X", (unsigned)row);
                ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(hexBuf);
                ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(asciiBuf);
            }
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
    }

    // Extracted strings
    if (ImGui::CollapsingHeader("Embedded Strings", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto strings = ExtractStrings(data, size);
        if (strings.empty()) {
            ImGui::TextDisabled("(no printable strings found)");
        } else {
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4,2));
            if (ImGui::BeginTable("##camstrs", 2,
                    ImGuiTableFlags_ScrollY       |
                    ImGuiTableFlags_RowBg         |
                    ImGuiTableFlags_BordersInnerV,
                    ImGui::GetContentRegionAvail())) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("#",    ImGuiTableColumnFlags_WidthFixed, 40);
                ImGui::TableSetupColumn("String", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();
                for (int si = 0; si < (int)strings.size(); si++) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%d", si);
                    ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(strings[si].c_str());
                }
                ImGui::EndTable();
            }
            ImGui::PopStyleVar();
        }
    }
}
