#define NOMINMAX
#include "audio_editor.h"
#include "../app.h"
#include "imgui.h"
#include "fx/audio.h"
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#include <commdlg.h>
#include <algorithm>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
namespace fs = std::filesystem;

static std::string Win32SaveFile(const wchar_t* filter, const wchar_t* defExt) {
    wchar_t buf[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize  = sizeof(ofn);
    ofn.hwndOwner    = (HWND)ImGui::GetMainViewport()->PlatformHandleRaw;
    ofn.lpstrFilter  = filter;
    ofn.lpstrFile    = buf;
    ofn.nMaxFile     = MAX_PATH;
    ofn.lpstrDefExt  = defExt;
    ofn.Flags        = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return {};
    int len = WideCharToMultiByte(CP_UTF8,0,buf,-1,nullptr,0,nullptr,nullptr);
    std::string s(len-1,0); WideCharToMultiByte(CP_UTF8,0,buf,-1,s.data(),len,nullptr,nullptr);
    return s;
}
static std::string Win32OpenFile(const wchar_t* filter, const wchar_t* title) {
    wchar_t buf[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize  = sizeof(ofn);
    ofn.hwndOwner    = (HWND)ImGui::GetMainViewport()->PlatformHandleRaw;
    ofn.lpstrFilter  = filter;
    ofn.lpstrFile    = buf;
    ofn.nMaxFile     = MAX_PATH;
    ofn.lpstrTitle   = title;
    ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return {};
    int len = WideCharToMultiByte(CP_UTF8,0,buf,-1,nullptr,0,nullptr,nullptr);
    std::string s(len-1,0); WideCharToMultiByte(CP_UTF8,0,buf,-1,s.data(),len,nullptr,nullptr);
    return s;
}

static int InferRate(const std::string& ext) {
    if (ext == "5k")  return 5000;
    if (ext == "8k")  return 8000;
    if (ext == "22k") return 22050;
    return 11025;
}

// ---------- playback state ----------

enum class PlayState { Stopped, Playing, Paused };

struct AudioPlayer {
    HWAVEOUT             wo           = nullptr;
    WAVEHDR              hdr          = {};
    std::vector<uint8_t> buf;
    int                  startSample  = 0;
    int                  totalSamples = 0;
    int                  rate         = 0;
    PlayState            state        = PlayState::Stopped;
    int                  resumeFrom   = 0;  // where Play/Resume starts from

    bool IsPlaying() const { return state == PlayState::Playing && wo && !(hdr.dwFlags & WHDR_DONE); }
    bool IsPaused()  const { return state == PlayState::Paused; }

    // Call once per frame to catch natural end-of-playback.
    void Update() {
        if (state == PlayState::Playing && wo && (hdr.dwFlags & WHDR_DONE)) {
            CloseDevice();
            state      = PlayState::Stopped;
            resumeFrom = 0;
        }
    }

    // Current sample for display.
    int Position() const {
        if (state == PlayState::Playing && wo) {
            MMTIME t = {};
            t.wType = TIME_BYTES; // 8-bit mono: bytes == samples
            waveOutGetPosition(wo, &t, sizeof(t));
            return startSample + (int)t.u.cb;
        }
        return resumeFrom;
    }

    bool Play(const uint8_t* pcm, int samples, int rate_, int fromSample) {
        CloseDevice();
        fromSample = std::clamp(fromSample, 0, (std::max)(0, samples - 1));
        int n = samples - fromSample;
        if (n <= 0) { state = PlayState::Stopped; return false; }

        WAVEFORMATEX wfx    = {};
        wfx.wFormatTag      = WAVE_FORMAT_PCM;
        wfx.nChannels       = 1;
        wfx.nSamplesPerSec  = (DWORD)rate_;
        wfx.wBitsPerSample  = 8;
        wfx.nBlockAlign     = 1;
        wfx.nAvgBytesPerSec = (DWORD)rate_;
        if (waveOutOpen(&wo, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
            state = PlayState::Stopped;
            return false;
        }

        buf.assign(pcm + fromSample, pcm + samples);
        hdr               = {};
        hdr.lpData         = (LPSTR)buf.data();
        hdr.dwBufferLength = (DWORD)buf.size();
        waveOutPrepareHeader(wo, &hdr, sizeof(hdr));
        waveOutWrite(wo, &hdr, sizeof(hdr));

        startSample  = fromSample;
        totalSamples = samples;
        rate         = rate_;
        resumeFrom   = fromSample;
        state        = PlayState::Playing;
        return true;
    }

    // Pause at current position.
    void Pause() {
        if (state != PlayState::Playing) return;
        resumeFrom = Position();
        CloseDevice();
        state = PlayState::Paused;
    }

    // Right-click seek: stop audio, park the playhead at a specific sample.
    void PauseAt(int sample) {
        CloseDevice();
        resumeFrom = sample;
        state      = PlayState::Paused;
    }

    void Stop() {
        CloseDevice();
        resumeFrom = 0;
        state      = PlayState::Stopped;
    }

private:
    void CloseDevice() {
        if (wo) {
            waveOutReset(wo);
            waveOutUnprepareHeader(wo, &hdr, sizeof(hdr));
            waveOutClose(wo);
            wo = nullptr;
        }
        buf.clear();
    }
};

static AudioPlayer s_player;

// ---------- waveform cache ----------

static const int DISP = 512;
static float     s_wave[DISP];
static int       s_waveEntry = -2;

static void BuildWaveform(const uint8_t* pcm, int samples) {
    for (int i = 0; i < DISP; i++) {
        int idx  = (int)((float)i / DISP * samples);
        s_wave[i] = (idx < samples) ? ((float)pcm[idx] - 128.0f) / 128.0f : 0.0f;
    }
}

// ---------- editor ----------

void DrawAudioEditor(App& app) {
    auto& ed = app.editor;
    int   rate    = InferRate(ed.ext);
    int   samples = (int)ed.data.size();
    float dur     = samples / (float)rate;

    if (s_waveEntry != ed.entryIdx) {
        s_player.Stop();
        s_waveEntry = ed.entryIdx;
        BuildWaveform(ed.data.data(), samples);
    }

    s_player.Update();

    ImGui::Text("Sample rate: %d Hz  |  Samples: %d  |  Duration: %.2f s",
                rate, samples, dur);
    ImGui::Separator();

    // ---------- waveform ----------
    const ImVec2 waveSize(ImGui::GetContentRegionAvail().x, 80.0f);
    const ImVec2 waveMin = ImGui::GetCursorScreenPos();
    const ImVec2 waveMax(waveMin.x + waveSize.x, waveMin.y + waveSize.y);

    ImGui::InvisibleButton("##wave", waveSize);

    // Left-drag: seek (restarts if playing, moves head if not).
    if (ImGui::IsItemActive()) {
        float t = std::clamp((ImGui::GetIO().MousePos.x - waveMin.x) / waveSize.x, 0.0f, 1.0f);
        int seekSample = (int)(t * samples);
        if (s_player.IsPlaying())
            s_player.Play(ed.data.data(), samples, rate, seekSample);
        else
            s_player.resumeFrom = seekSample;
    }

    // Right-click: park the playhead here (pause).
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        float t = std::clamp((ImGui::GetIO().MousePos.x - waveMin.x) / waveSize.x, 0.0f, 1.0f);
        s_player.PauseAt((int)(t * samples));
    }

    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(waveMin, waveMax, IM_COL32(30, 30, 30, 255));

    const float midY  = waveMin.y + waveSize.y * 0.5f;
    const float halfH = waveSize.y * 0.5f;
    for (int i = 0; i < DISP - 1; i++) {
        float x0 = waveMin.x + (float)i       / DISP * waveSize.x;
        float x1 = waveMin.x + (float)(i + 1) / DISP * waveSize.x;
        float y0 = midY - s_wave[i]     * halfH;
        float y1 = midY - s_wave[i + 1] * halfH;
        dl->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(80, 180, 80, 255));
    }

    // Playhead â€” colour signals state.
    if (samples > 0) {
        ImU32 headCol;
        if      (s_player.IsPlaying()) headCol = IM_COL32(255, 210,  50, 220); // yellow: playing
        else if (s_player.IsPaused())  headCol = IM_COL32( 80, 160, 255, 220); // blue:   paused
        else                           headCol = IM_COL32(160, 160, 160, 120); // grey:   stopped

        float t  = (float)s_player.Position() / (float)samples;
        float px = waveMin.x + t * waveSize.x;
        dl->AddLine(ImVec2(px, waveMin.y), ImVec2(px, waveMax.y), headCol, 1.5f);
    }

    dl->AddRect(waveMin, waveMax, IM_COL32(80, 80, 80, 255));

    ImGui::Separator();

    // Play: enabled when stopped or paused.
    bool canPlay = !s_player.IsPlaying();
    if (!canPlay) ImGui::BeginDisabled();
    if (ImGui::Button("Play"))
        s_player.Play(ed.data.data(), samples, rate, s_player.resumeFrom);
    if (!canPlay) ImGui::EndDisabled();

    ImGui::SameLine();

    // Pause: enabled only while playing.
    bool isPlaying = s_player.IsPlaying();
    if (!isPlaying) ImGui::BeginDisabled();
    if (ImGui::Button("Pause"))
        s_player.Pause();
    if (!isPlaying) ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Stop"))
        s_player.Stop();

    ImGui::Separator();

    if (ImGui::Button("Export WAV...")) {
        std::string path = Win32SaveFile(L"WAV Audio\0*.wav\0All Files\0*.*\0", L"wav");
        if (!path.empty()) {
            auto wav = fx::audio_to_wav(ed.data.data(), ed.data.size(), (uint32_t)rate);
            if (!wav.empty()) {
                std::ofstream f(path, std::ios::binary);
                if (f) f.write((const char*)wav.data(), (std::streamsize)wav.size());
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Import WAV...")) {
        std::string path = Win32OpenFile(L"WAV Audio\0*.wav\0All Files\0*.*\0",
                                         L"Import WAV");
        if (!path.empty()) {
            std::ifstream f(path, std::ios::binary | std::ios::ate);
            if (f) {
                auto sz = f.tellg(); f.seekg(0);
                std::vector<uint8_t> wav((size_t)sz);
                f.read((char*)wav.data(), (std::streamsize)sz);
                uint32_t outRate = 0;
                auto pcm = fx::wav_to_pcm(wav.data(), wav.size(), &outRate);
                if (!pcm.empty()) {
                    s_player.Stop();
                    ed.data     = std::move(pcm);
                    ed.modified = true;
                    BuildWaveform(ed.data.data(), (int)ed.data.size());
                    app.statusMsg  = "Imported " + fs::path(path).filename().string();
                    app.statusKind = App::StatusKind::Info;
                }
            }
        }
    }
}
