#pragma once
#include "imgui.h"
#include "fx/ealib.h"
#include "workspace.h"
#include "asset_index.h"
#include "thumbnails.h"
#include "assets/icons_baked.h"
#include "platform/texture.h"
#include "platform/theme.h"
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
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

    // Open a workspace namespace entry (category browsers, #364): ensures a
    // session for its source LIB/loose file is open, then routes to the editor.
    void OpenWorkspaceEntry(int node);

    // Select an object (#365): scope the editor area to its file cluster
    // (asset_cluster over the index graph) and open its primary record. The
    // scope persists while cluster files are opened; a raw Archives open or a
    // re-mount clears it (ClearObjectScope).
    void SelectObject(int node);
    void ClearObjectScope();

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

    // Left-panel navigation (#364): the icon bar selects a category browser, or
    // Archives for the raw per-LIB picker. selectedNode is the workspace name
    // index of the last-opened object (survives category switches).
    // Default to Archives so the classic open-a-LIB flow works out of the box;
    // mounting a workspace switches to the category browsers.
    fxs::icons::Id          leftView   = fxs::icons::Id::Archives;
    int                     selectedNode = -1;

    // Object scope (#365): the selected object's node and its cached file
    // cluster; the editor host draws the cluster as a file strip while set.
    int                     clusterRoot  = -1;
    std::vector<int>        cluster;
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

    // SH thumbnails for the category browsers (#366): the async service plus
    // the UI-side texture cache it fills. Browsers Request() nodes as their
    // cells become visible; Draw() drains finished renders into GL textures.
    // thumbMissing remembers nodes with no renderable shape so the grid stops
    // asking. thumbCacheDir is set at startup (the per-user pref path).
    fxg::ThumbnailService               thumbs;
    std::string                         thumbCacheDir;
    std::unordered_map<int, GpuTexture> thumbTex;
    std::unordered_set<int>             thumbMissing;

private:
    void DrawMenuBar();

    void OpenLibDialog();
    void OpenFileDialog();
    void OpenStandaloneFile(const std::string& path);
    void ChooseInstallDir();
    void AddRecentFile(const std::string& path);
    int  FindSessionByPath(const std::string& path) const; // -1 if not open

    // Background asset-index build (#362): the worker parses records off the UI
    // thread; Draw() polls progress and swaps the finished index in.
    void StartIndexing();
    void StopIndexing();   // cooperative cancel + join; safe to call any time
    void PollIndexing();   // main thread: update status, adopt a finished index
    void PollThumbnails(); // main thread: upload finished thumbnails (#366)
    void ResetThumbnails(); // stop the service and release the GL textures

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
