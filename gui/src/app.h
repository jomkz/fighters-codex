#pragma once
#include "imgui.h"
#include "fx/ealib.h"
#include "workspace.h"
#include "asset_index.h"
#include "platform/texture.h"
#include "platform/theme.h"
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Texture handle for image preview (GL texture behind an opaque id).
using GpuTexture = platform::GpuTexture;

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
    Pal,        // PAL palette swatch viewer
    T2,         // T2 terrain 3D preview
};

struct EditorState {
    EditorKind   kind     = EditorKind::None;
    int          libIdx   = -1;    // index into App::sessions
    int          entryIdx = -1;    // index into LibSession::entries
    std::string  ext;              // lowercase extension
    std::vector<uint8_t> data;     // decompressed record bytes
    bool         modified = false;
};

class App {
public:
    App();
    ~App();
    void Draw();

    // Called by panels/editors to open a record for editing.
    void OpenEntry(int libIdx, int entryIdx);

    // Open a LIB from a path (recent-files menu and the --smoke sweep).
    void OpenLib(const std::string& path);

    // Save modified record back into the session (patches in memory).
    void CommitEntry(const std::vector<uint8_t>& newData);

    // Write the session's in-memory LIB to FA_0.LIB in the configured install dir.
    void InstallToGame(int libIdx);

    // Close one session by index, or all sessions.
    void CloseSession(int idx);
    void CloseAllSessions();

    // Mount the configured install dir as one workspace namespace (#361),
    // then kick off the background asset-graph index (#362).
    void MountWorkspace();

    enum class StatusKind { Info, Warning, Error };

    // ---------- public state ----------
    std::vector<LibSession> sessions;
    EditorState             editor;
    std::string             installDir;      // FA game directory (mount source + install target)
    fxg::Workspace          workspace;       // installDir mounted as one namespace (#361)
    fxg::AssetIndex         assetIndex;      // categories + reference graph over workspace (#362)
    bool                    workspaceOnStart = false; // re-mount installDir at launch
    std::string             statusMsg;
    StatusKind              statusKind      = StatusKind::Info;
    int                     selectedSession = -1;
    ThemePreference         themePref       = ThemePreference::Auto;

    // Preview palette selection (palettes.h): palLib is a session index or
    // fxg::kPalAuto / fxg::kPalGreyscale; palGen invalidates palettized
    // preview caches on every change.
    int                     palLib   = -1;
    int                     palEntry = -1;
    int                     palGen   = 0;

private:
    void DrawMenuBar();

    void OpenLibDialog();
    void OpenFileDialog();
    void OpenStandaloneFile(const std::string& path);
    void ChooseInstallDir();
    void AddRecentFile(const std::string& path);

    // Background asset-index build (#362): the worker parses records off the UI
    // thread; Draw() polls progress and swaps the finished index in.
    void StartIndexing();
    void StopIndexing();   // cooperative cancel + join; safe to call any time
    void PollIndexing();   // main thread: update status, adopt a finished index

    std::string          m_dupLibPath;
    std::vector<std::string> m_recentFiles; // up to 5, most recent first

    std::thread          m_indexThread;
    fxg::IndexCancel     m_indexCancel;
    fxg::AssetIndex      m_indexResult;      // handoff buffer, guarded by m_indexMutex
    std::mutex           m_indexMutex;
    std::atomic<int>     m_indexDone{0};
    std::atomic<int>     m_indexTotal{0};
    std::atomic<bool>    m_indexRunning{false};
    std::atomic<bool>    m_indexReady{false};
};
