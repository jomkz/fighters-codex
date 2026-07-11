#include "vdo_editor.h"
#include "../app.h"
#include "../platform/audio_player.h"
#include "../platform/texture.h"
#include "imgui.h"
#include <fx/ealib.h>
#include <fx/fbc.h>
#include <fx/vdo.h>
#include <cstdarg>
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
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV,
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

// ---- header / palette panels (info only) ----------------------------------

static void DrawVdoInfo(const uint8_t* h, size_t size, uint32_t frameCnt) {
    uint32_t fps = u32le(h + 8);
    uint16_t width = u16le(h + 18), height = u16le(h + 20);
    if (ImGui::CollapsingHeader("Header Fields")) {
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
                char buf[128]; va_list ap; va_start(ap, fmt);
                vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
                ImGui::TextUnformatted(buf);
            };
            Row("+6",  "Version",           "%u.%u", h[6], h[7]);
            Row("+8",  "Frame rate",        "%u fps", fps);
            Row("+16", "Frame count",       "%u", frameCnt);
            Row("+18", "Width",             "%u px", width);
            Row("+20", "Height",            "%u px", height);
            Row("+22", "Palette entries",   "%u", u16le(h + 22));
            Row("+24", "Audio channels",    "%u%s", u16le(h + 24), u16le(h+24)==1?" (mono)":"");
            Row("+26", "Audio sample rate", "%u Hz", u16le(h + 26));
            Row("+816","Frame data",        "%zu bytes", size > 816 ? size - 816 : 0);
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
    }
    if (ImGui::CollapsingHeader("Embedded VGA Palette (256 colours)")) {
        const uint8_t* pal = h + 48;
        ImVec2 sz(14, 14);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 2));
        for (int i = 0; i < 256; i++) {
            if (i % 16 != 0) ImGui::SameLine();
            ImVec4 col(vga6(pal[i*3+0])/255.f, vga6(pal[i*3+1])/255.f, vga6(pal[i*3+2])/255.f, 1.f);
            char lbl[8]; snprintf(lbl, sizeof(lbl), "##p%d", i);
            ImGui::ColorButton(lbl, col,
                ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, sz);
        }
        ImGui::PopStyleVar();
    }
}

// ---- playback state -------------------------------------------------------

static fx::VdoDecoder*        s_dec        = nullptr;
static GpuTexture             s_tex;
static std::vector<uint8_t>   s_vdo, s_fbc, s_pcm;  // kept alive for s_dec
static uint32_t               s_cur        = 0;
static uint32_t               s_frameCount = 0;
static int                    s_w = 0, s_h = 0;
static uint32_t               s_fps = 15;
static uint32_t               s_audioHz = 8000;
static bool                   s_playing = false;
static bool                   s_hasAudio = false;
static double                 s_clock = 0.0;         // seconds, video-only pacing
static int                    s_lastLib = -2, s_lastEntry = -2;
static platform::AudioPlayer  s_player;

static std::vector<uint8_t> extract_sibling(App& app, int libIdx, const std::string& name) {
    if (libIdx < 0 || libIdx >= (int)app.sessions.size()) return {};
    auto& s = app.sessions[libIdx];
    const fx::Entry* e = fx::ealib_find(s.entries, name);
    if (!e) return {};
    return fx::ealib_extract(s.data.data(), s.data.size(), *e, true);
}

static void DecodeCurrentToTexture() {
    s_tex.Release();
    if (!s_dec) return;
    auto rgba = fx::vdo_decode_frame_rgba(s_dec, s_cur);
    if (!rgba.empty()) s_tex = platform::UploadTexture(rgba.data(), s_w, s_h);
}

static void LoadVdo(App& app) {
    s_tex.Release();
    if (s_dec) { fx::vdo_close(s_dec); s_dec = nullptr; }
    s_player.Stop();
    s_playing = false; s_cur = 0; s_clock = 0.0; s_hasAudio = false;
    s_frameCount = 0;

    auto& ed = app.editor;
    s_vdo = ed.data;   // own a copy — vdo_open keeps pointers into it

    // Locate the paired .FBC (same 4-char stem) in the same LIB session.
    std::string name;
    if (ed.libIdx >= 0 && ed.libIdx < (int)app.sessions.size() &&
        ed.entryIdx >= 0 && ed.entryIdx < (int)app.sessions[ed.libIdx].entries.size()) {
        name = app.sessions[ed.libIdx].entries[ed.entryIdx].name;
    }
    auto dot = name.find_last_of('.');
    std::string stem = (dot == std::string::npos) ? name : name.substr(0, dot);
    if (!stem.empty())
        s_fbc = extract_sibling(app, ed.libIdx, stem + ".FBC");
    else
        s_fbc.clear();

    if (s_fbc.empty()) return;  // no index → header-only view

    s_dec = fx::vdo_open(s_vdo.data(), s_vdo.size(), s_fbc.data(), s_fbc.size());
    if (!s_dec) return;

    s_frameCount = fx::vdo_frame_count(s_dec);
    s_w = (int)fx::vdo_width(s_dec);
    s_h = (int)fx::vdo_height(s_dec);
    const uint8_t* h = s_vdo.data();
    s_fps = u32le(h + 8); if (s_fps == 0 || s_fps > 60) s_fps = 15;
    s_audioHz = u16le(h + 26); if (s_audioHz == 0) s_audioHz = 11025;

    // Audio is shared per 3-character briefing-group prefix (AAC.11K -> AACA…).
    if (stem.size() >= 3)
        s_pcm = extract_sibling(app, ed.libIdx, stem.substr(0, 3) + ".11K");
    else
        s_pcm.clear();
    s_hasAudio = !s_pcm.empty();

    DecodeCurrentToTexture();
}

static void StartAudioFromFrame() {
    if (!s_hasAudio) return;
    int fromSample = (int)((double)s_cur / s_fps * s_audioHz);
    s_player.Play(s_pcm.data(), (int)s_pcm.size(), (int)s_audioHz, fromSample);
}

// ---- main editor ----------------------------------------------------------

static void DrawVdoPlayer(App& app) {
    if (!s_dec) {
        auto& ed = app.editor;
        if (ed.data.size() >= 6 && memcmp(ed.data.data(), "RATVID", 6) == 0 &&
            ed.data.size() >= 816) {
            ImGui::TextColored({1.f,0.7f,0.2f,1.f},
                "Paired .FBC frame index not found — showing header only. "
                "Open the movie from its LIB so the index is available.");
            ImGui::Separator();
            DrawVdoInfo(ed.data.data(), ed.data.size(), u16le(ed.data.data() + 16));
        } else {
            ImGui::TextColored({1.f,0.4f,0.4f,1.f}, "Not a RATVID .VDO.");
        }
        return;
    }

    float dur = s_fps ? (float)s_frameCount / s_fps : 0.f;
    ImGui::TextDisabled("%d x %d  |  %u frames  |  %u fps  |  %.1f s  |  %s",
                        s_w, s_h, s_frameCount, s_fps, dur,
                        s_hasAudio ? "audio: paired .11K" : "audio: none (silent group)");
    ImGui::Separator();

    // ---- advance the playhead ---------------------------------------------
    if (s_playing && s_frameCount > 0) {
        double t;
        if (s_hasAudio && s_player.IsPlaying()) {
            s_player.Update();
            t = (double)s_player.Position() / s_audioHz;   // sync video to audio
        } else {
            s_clock += ImGui::GetIO().DeltaTime;
            t = s_clock;
        }
        uint32_t target = (uint32_t)(t * s_fps);
        if (target >= s_frameCount) {          // reached the end
            target = s_frameCount - 1;
            s_playing = false;
            s_player.Stop();
        }
        if (target != s_cur) { s_cur = target; DecodeCurrentToTexture(); }
    }

    // ---- transport --------------------------------------------------------
    if (ImGui::Button(s_playing ? "Pause" : "Play", ImVec2(72, 0))) {
        s_playing = !s_playing;
        if (s_playing) {
            if (s_cur + 1 >= s_frameCount) { s_cur = 0; DecodeCurrentToTexture(); }
            s_clock = (double)s_cur / s_fps;
            StartAudioFromFrame();
        } else {
            s_player.Pause();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop", ImVec2(56, 0))) {
        s_playing = false; s_player.Stop();
        s_cur = 0; s_clock = 0.0; DecodeCurrentToTexture();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    int frame = (int)s_cur;
    if (s_frameCount > 1 &&
        ImGui::SliderInt("##frame", &frame, 0, (int)s_frameCount - 1, "Frame %d")) {
        s_cur = (uint32_t)frame;
        s_clock = (double)s_cur / s_fps;
        DecodeCurrentToTexture();
        if (s_playing) StartAudioFromFrame();   // re-seek the audio
    }

    ImGui::Separator();

    // ---- frame display ----------------------------------------------------
    if (s_tex.id && s_tex.width > 0) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float scale = avail.x / (float)s_tex.width;
        if (scale > 3.0f) scale = 3.0f;
        if (scale < 0.1f) scale = 0.1f;
        ImGui::Image((ImTextureID)(intptr_t)s_tex.id,
                     ImVec2(s_tex.width * scale, s_tex.height * scale));
    }

    ImGui::Spacing();
    DrawVdoInfo(s_vdo.data(), s_vdo.size(), s_frameCount);
}

// ---- dispatch -------------------------------------------------------------

void DrawVdoEditor(App& app) {
    auto& ed = app.editor;
    if (ed.ext == "fbc") { DrawFbc(ed.data); return; }

    if (ed.libIdx != s_lastLib || ed.entryIdx != s_lastEntry) {
        s_lastLib = ed.libIdx;
        s_lastEntry = ed.entryIdx;
        LoadVdo(app);
    }
    DrawVdoPlayer(app);
}
