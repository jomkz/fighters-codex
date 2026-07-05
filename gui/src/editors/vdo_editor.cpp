#include "vdo_editor.h"
#include "../app.h"
#include "imgui.h"
#include <fx/fbc.h>
#include <cstdint>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

// ---- little-endian helpers ------------------------------------------------

static inline uint16_t u16le(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}
static inline uint32_t u32le(const uint8_t* p) {
    return (uint32_t)(p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24));
}
// Convert 6-bit VGA DAC value (0–63) to 8-bit (0–255).
static inline float vga6(uint8_t v) { return (v * 255.f) / 63.f; }

// ---- FBC viewer -----------------------------------------------------------

static void DrawFbc(const std::vector<uint8_t>& data) {
    bool ok = false;
    auto sizes = fx::fbc_read(data.data(), data.size(), &ok);
    if (!ok) {
        ImGui::TextColored({1.f,0.4f,0.4f,1.f},
            "File size %zu is not a multiple of 4 — not a valid FBC index.",
            data.size());
        return;
    }
    ImGui::TextDisabled("FBC Frame Index  |  %zu frame(s)  |  %zu bytes",
                        sizes.size(), data.size());
    ImGui::Separator();

    if (sizes.empty()) { ImGui::TextDisabled("(empty)"); return; }

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4,2));
    if (ImGui::BeginTable("##fbc", 3,
            ImGuiTableFlags_ScrollY       |
            ImGuiTableFlags_RowBg         |
            ImGuiTableFlags_BordersInnerV,
            ImGui::GetContentRegionAvail())) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Frame",       ImGuiTableColumnFlags_WidthFixed,   60);
        ImGui::TableSetupColumn("Frame bytes", ImGuiTableColumnFlags_WidthFixed,  100);
        ImGui::TableSetupColumn("VDO offset",  ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        uint64_t off = fx::fbc_frame_offset(sizes, 0);
        for (size_t i = 0; i < sizes.size(); i++) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%zu", i);
            ImGui::TableSetColumnIndex(1); ImGui::Text("%u", sizes[i]);
            ImGui::TableSetColumnIndex(2); ImGui::TextDisabled("0x%06llX",
                                           (unsigned long long)off);
            off += sizes[i];
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

// ---- VDO viewer -----------------------------------------------------------

static void DrawVdo(const std::vector<uint8_t>& data) {
    const size_t HEADER_SIZE = 816;   // 48 bytes fields + 768 bytes palette

    // ---- magic / size check -----------------------------------------------
    if (data.size() < HEADER_SIZE) {
        ImGui::TextColored({1.f,0.4f,0.4f,1.f},
            "File is only %zu bytes — too small for the 816-byte VDO header.",
            data.size());
        // Still show whatever bytes we have
    }

    bool hasRatvid = data.size() >= 6 &&
                     memcmp(data.data(), "RATVID", 6) == 0;
    if (!hasRatvid) {
        ImGui::TextColored({1.f,0.7f,0.2f,1.f},
            "RATVID magic not found at offset 0.");
        ImGui::Separator();
    }

    if (data.size() < HEADER_SIZE) return;

    // ---- parse header fields ----------------------------------------------
    const uint8_t* h = data.data();

    uint8_t  ver_maj    = h[6];
    uint8_t  ver_min    = h[7];
    uint32_t fps        = u32le(h +  8);
    uint32_t unk_12     = u32le(h + 12);
    uint16_t frame_cnt  = u16le(h + 16);
    uint16_t width      = u16le(h + 18);
    uint16_t height     = u16le(h + 20);
    uint16_t pal_count  = u16le(h + 22);   // observed 256
    uint16_t channels   = u16le(h + 24);   // observed 1 (mono)
    uint16_t audio_hz   = u16le(h + 26);
    uint32_t unk_28     = u32le(h + 28);
    // h[32..47] = 16 bytes, observed all-zero (padding)
    // h[48..815] = VGA palette (256 × RGB, 6-bit each)

    float    duration   = (fps > 0) ? (float)frame_cnt / (float)fps : 0.f;
    size_t   frame_data = (data.size() > HEADER_SIZE) ? data.size() - HEADER_SIZE : 0;

    // ---- summary line -----------------------------------------------------
    ImGui::TextDisabled(
        "RATVID v%u.%u  |  %u \xc3\x97 %u  |  %u fps  |  %u frames  |  %.1f s  |  %zu bytes",
        ver_maj, ver_min, width, height, fps, frame_cnt, duration, data.size());
    ImGui::Separator();

    // ---- properties table -------------------------------------------------
    if (ImGui::CollapsingHeader("Header Fields", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6,3));
        if (ImGui::BeginTable("##vdohdr", 3,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed,  56);
            ImGui::TableSetupColumn("Field",  ImGuiTableColumnFlags_WidthFixed, 180);
            ImGui::TableSetupColumn("Value",  ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            auto Row = [](const char* off, const char* field, const char* fmt, ...) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", off);
                ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(field);
                ImGui::TableSetColumnIndex(2);
                char buf[128];
                va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
                ImGui::TextUnformatted(buf);
            };

            Row("+0",  "Magic",             "\"RATVID\" %s", hasRatvid ? "\xe2\x9c\x93" : "(MISSING)");
            Row("+6",  "Version",           "%u.%u", ver_maj, ver_min);
            Row("+8",  "Frame rate",        "%u fps", fps);
            Row("+12", "Unknown (unk_12)",  "0x%08X%s", unk_12, unk_12==0?" (always 0)":"");
            Row("+16", "Frame count",       "%u", frame_cnt);
            Row("+18", "Width",             "%u px", width);
            Row("+20", "Height",            "%u px", height);
            Row("+22", "Palette entries",   "%u%s", pal_count, pal_count==256?" (full VGA palette)":"");
            Row("+24", "Audio channels",    "%u%s", channels, channels==1?" (mono)":channels==2?" (stereo)":"");
            Row("+26", "Audio sample rate", "%u Hz", audio_hz);
            Row("+28", "Unknown (unk_28)",  "0x%08X", unk_28);
            Row("+32", "Padding (16 B)",    "(reserved, zero)");
            Row("+48", "VGA palette",       "768 bytes (256 \xc3\x97 RGB, 6-bit/ch)");
            Row("+816","Frame data",        "%zu bytes across %u frames", frame_data, frame_cnt);
            if (fps > 0 && frame_cnt > 0)
                Row("",  "Duration",        "%.2f s (at %u fps)", duration, fps);

            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
    }

    // ---- embedded palette -------------------------------------------------
    if (ImGui::CollapsingHeader("Embedded VGA Palette (256 colours)",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextDisabled("Each colour is rendered from the 6-bit VGA DAC values at header +48.");
        ImGui::Spacing();

        const uint8_t* pal = h + 48;
        ImVec2 sz(14, 14);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 2));
        for (int i = 0; i < 256; i++) {
            if (i % 16 != 0) ImGui::SameLine();
            float r = vga6(pal[i*3+0]);
            float g = vga6(pal[i*3+1]);
            float b = vga6(pal[i*3+2]);
            ImVec4 col(r/255.f, g/255.f, b/255.f, 1.f);
            char lbl[8]; snprintf(lbl, sizeof(lbl), "##p%d", i);
            ImGui::ColorButton(lbl, col,
                ImGuiColorEditFlags_NoTooltip |
                ImGuiColorEditFlags_NoBorder, sz);
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Index %d (0x%02X)", i, i);
                ImGui::Text("VGA: R=%u G=%u B=%u (6-bit)",
                            pal[i*3+0], pal[i*3+1], pal[i*3+2]);
                ImGui::Text("8-bit: R=%u G=%u B=%u",
                            (unsigned)(r+.5f), (unsigned)(g+.5f), (unsigned)(b+.5f));
                ImGui::ColorButton("##prev", col,
                    ImGuiColorEditFlags_NoTooltip, ImVec2(40,40));
                ImGui::EndTooltip();
            }
        }
        ImGui::PopStyleVar();
        ImGui::Spacing();
    }

    // ---- unknown bytes raw dump (fields we haven't decoded) ---------------
    if (ImGui::CollapsingHeader("Raw Header (hex)")) {
        ImGui::TextDisabled("First 48 bytes (fields) — palette omitted for brevity.");
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(3,2));
        if (ImGui::BeginTable("##vdoraw", 3,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("Addr",  ImGuiTableColumnFlags_WidthFixed,  56);
            ImGui::TableSetupColumn("Hex",   ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("ASCII", ImGuiTableColumnFlags_WidthFixed, 130);
            ImGui::TableHeadersRow();

            char hexBuf[16*3+1], asciiBuf[17];
            for (size_t row = 0; row < 48; row += 16) {
                size_t rowLen = (row + 16 <= 48) ? 16 : 48 - row;
                size_t hp = 0;
                for (size_t i = 0; i < rowLen; i++) {
                    if (i > 0) hexBuf[hp++] = ' ';
                    snprintf(hexBuf + hp, 4, "%02X", h[row+i]);
                    hp += 2;
                }
                hexBuf[hp] = '\0';
                for (size_t i = 0; i < rowLen; i++)
                    asciiBuf[i] = isprint(h[row+i]) ? (char)h[row+i] : '.';
                asciiBuf[rowLen] = '\0';

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("+%04zu", row);
                ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(hexBuf);
                ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(asciiBuf);
            }
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
    }
}

// ---- dispatch -------------------------------------------------------------

void DrawVdoEditor(App& app) {
    auto& ed = app.editor;
    if (ed.ext == "fbc")
        DrawFbc(ed.data);
    else
        DrawVdo(ed.data);
}
