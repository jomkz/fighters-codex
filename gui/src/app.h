#pragma once
#include "imgui.h"
#include "fx/ealib.h"
#include <d3d11.h>
#include <string>
#include <vector>
#include <functional>

enum class ThemePreference { Auto = 0, Dark = 1, Light = 2 };

// A single open LIB file, or a standalone loose file.
struct LibSession {
    std::string              path;           // full path to the file
    std::vector<uint8_t>     data;           // raw file bytes
    std::vector<fx::Entry>   entries;        // parsed directory (1 entry if standalone)
    bool                     dirty      = false;
    bool                     standalone = false; // true = loose file, not a LIB
    float                    tableHeight = 0.0f; // browser panel row height (0 = uninitialised)
    int                      forceOpen  = -1;    // -1=none, 0=collapse, 1=expand (consumed after one frame)
};

// What is currently open in the editor center panel.
enum class EditorKind {
    None,
    Brf,        // OT/NT/PT/JT/SEE/ECM/GAS
    Pic,
    Audio,
    Mission,
    Seq,
    Inf,
    Plt,
    Raw,
    Sh,
    Txt,        // TXT/WRI/HLP/INI plain text
    Bin,        // BIN/SMS binary hex viewer
    Lay,        // LAY atmosphere
    Hud,        // HUD cockpit DLL
    Mus,        // MUS music bytecode
    Fnt,        // FNT font DLL
    Cb8,        // CB8 FMV frame scrubber
    Ai,         // AI script + BI compile
    Xmi,        // XMI MIDI metadata
    Vdo,        // VDO/FBC video metadata
    Cam,        // CAM campaign
};

struct EditorState {
    EditorKind   kind     = EditorKind::None;
    int          libIdx   = -1;    // index into App::sessions
    int          entryIdx = -1;    // index into LibSession::entries
    std::string  ext;              // lowercase extension
    std::vector<uint8_t> data;     // decompressed record bytes
    bool         modified = false;
};

// Texture handle for image preview.
struct GpuTexture {
    ID3D11ShaderResourceView* srv = nullptr;
    int width  = 0;
    int height = 0;
    void Release() { if (srv) { srv->Release(); srv = nullptr; } }
};

class App {
public:
    App(ID3D11Device* device, ID3D11DeviceContext* ctx);
    ~App();
    void Draw();

    // Called by panels/editors to open a record for editing.
    void OpenEntry(int libIdx, int entryIdx);

    // Save modified record back into the session (patches in memory).
    void CommitEntry(const std::vector<uint8_t>& newData);

    // Write the session's in-memory LIB to FA_0.LIB in the configured install dir.
    void InstallToGame(int libIdx);

    // Close one session by index, or all sessions.
    void CloseSession(int idx);
    void CloseAllSessions();

    // Upload RGBA pixels to a DX11 texture for display in ImGui.
    GpuTexture UploadTexture(const uint8_t* rgba, int w, int h);

    ID3D11Device*        GetDevice() const { return m_device; }
    ID3D11DeviceContext* GetCtx()    const { return m_ctx; }

    enum class StatusKind { Info, Warning, Error };

    // ---------- public state ----------
    std::vector<LibSession> sessions;
    EditorState             editor;
    std::string             installDir;      // FA game directory
    std::string             statusMsg;
    StatusKind              statusKind      = StatusKind::Info;
    int                     selectedSession = -1;
    ThemePreference         themePref       = ThemePreference::Auto;

private:
    void DrawMenuBar();

    void OpenLibDialog();
    void OpenFileDialog();
    void OpenLib(const std::string& path);  // shared by dialog + recent files
    void OpenStandaloneFile(const std::string& path);
    void SaveSessionDialog(int libIdx);
    void ChooseInstallDir();
    void AddRecentFile(const std::string& path);

    ID3D11Device*        m_device;
    ID3D11DeviceContext* m_ctx;
    std::string          m_dupLibPath;
    std::vector<std::string> m_recentFiles; // up to 5, most recent first
};
