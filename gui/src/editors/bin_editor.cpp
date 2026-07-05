#include "bin_editor.h"
#include "../app.h"
#include "imgui.h"
#include <fx/bin.h>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <string>

void DrawBinEditor(App& app) {
    auto& ed = app.editor;
    const uint8_t* data = ed.data.data();
    size_t         size = ed.data.size();

    // Name-based interpretation (BIN.md: the bytes carry no structure; the
    // entry name identifies the table)
    std::string entryName;
    if (ed.libIdx >= 0 && ed.libIdx < (int)app.sessions.size())
        entryName = app.sessions[ed.libIdx].entries[ed.entryIdx].name;
    fx::BinKind kind = fx::bin_classify(entryName);
    if (kind != fx::BinKind::Unknown) {
        ImGui::TextColored({0.85f,0.82f,0.78f,1}, "%s", fx::bin_kind_desc(kind));
        size_t expected = fx::bin_expected_size(kind);
        if (expected != 0 && expected != size)
            ImGui::TextColored({1.f,0.7f,0.2f,1.f},
                "Size %zu differs from the documented %zu bytes.",
                size, expected);
    }

    static constexpr size_t kMaxDisplay = 4096;
    bool truncated = size > kMaxDisplay;
    size_t dispSize = truncated ? kMaxDisplay : size;

    if (truncated)
        ImGui::TextDisabled("Showing first %zu of %zu bytes", kMaxDisplay, size);
    else
        ImGui::TextDisabled("%zu bytes", size);
    ImGui::Separator();

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(3, 2));
    if (ImGui::BeginTable("##hex", 3,
            ImGuiTableFlags_ScrollY     |
            ImGuiTableFlags_RowBg       |
            ImGuiTableFlags_BordersInnerV,
            ImGui::GetContentRegionAvail())) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Addr", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Hex",  ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("ASCII",ImGuiTableColumnFlags_WidthFixed, 128);
        ImGui::TableHeadersRow();

        char hexBuf[16*3 + 1];  // "XX XX XX ..." with trailing NUL
        char asciiBuf[17];

        for (size_t row = 0; row < dispSize; row += 16) {
            size_t rowEnd = row + 16 < dispSize ? row + 16 : dispSize;
            size_t rowLen = rowEnd - row;

            // Build hex string
            size_t hp = 0;
            for (size_t i = 0; i < rowLen; i++) {
                if (i > 0) hexBuf[hp++] = ' ';
                snprintf(hexBuf + hp, 4, "%02X", data[row + i]);
                hp += 2;
            }
            hexBuf[hp] = '\0';

            // Build ASCII string
            for (size_t i = 0; i < rowLen; i++)
                asciiBuf[i] = isprint(data[row + i]) ? (char)data[row + i] : '.';
            asciiBuf[rowLen] = '\0';

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("%04X", (unsigned)row);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(hexBuf);
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(asciiBuf);
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}
