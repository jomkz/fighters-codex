#include "app.h"
#include "fx/version.h"
#include "panels/category_browser.h"
#include "panels/lib_browser.h"
#include "panels/editor_host.h"
#include "panels/preview.h"
#include "platform/dialogs.h"
#include "platform/window.h"
#include "util.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <fstream>
#include <string>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

static constexpr int kMaxRecent = 5;

App::App() {
    // Persist app settings (e.g. installDir) inside fxs.ini.
    ImGuiSettingsHandler h = {};
    h.TypeName   = "FightersToolkit";
    h.TypeHash   = ImHashStr("FightersToolkit");
    h.UserData   = this;
    h.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler*, const char*) -> void* {
        return (void*)1;
    };
    h.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler* handler, void*, const char* line) {
        App* app = static_cast<App*>(handler->UserData);
        auto readVal = [](const char* ln, const char* prefix, std::string& out) -> bool {
            size_t plen = strlen(prefix);
            if (strncmp(ln, prefix, plen) != 0) return false;
            out = ln + plen;
            while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
                out.pop_back();
            return true;
        };
        std::string val;
        if (readVal(line, "InstallDir=", val)) {
            app->installDir = val;
        } else if (readVal(line, "RecentFile=", val)) {
            if (!val.empty() && (int)app->m_recentFiles.size() < kMaxRecent)
                app->m_recentFiles.push_back(val);
        } else if (readVal(line, "Theme=", val)) {
            if (!val.empty()) {
                int v = 0;
                try { v = std::stoi(val); } catch (...) {}
                if (v >= 0 && v <= 2)
                    app->themePref = static_cast<ThemePreference>(v);
            }
        } else if (readVal(line, "WorkspaceOnStart=", val)) {
            app->workspaceOnStart = (val == "1");
        }
    };
    h.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf) {
        App* app = static_cast<App*>(handler->UserData);
        buf->appendf("[%s][Data]\n", handler->TypeName);
        buf->appendf("InstallDir=%s\n", app->installDir.c_str());
        buf->appendf("Theme=%d\n", (int)app->themePref);
        buf->appendf("WorkspaceOnStart=%d\n", app->workspaceOnStart ? 1 : 0);
        for (const auto& p : app->m_recentFiles)
            buf->appendf("RecentFile=%s\n", p.c_str());
        buf->append("\n");
    };
    ImGui::AddSettingsHandler(&h);
}

App::~App() { StopIndexing(); }

void App::Draw() {
    // Host window â€” fills the OS window's client area exactly.
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGuiWindowFlags hostFlags =
        ImGuiWindowFlags_NoTitleBar   | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize     | ImGuiWindowFlags_NoMove     |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_MenuBar;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("##Host", nullptr, hostFlags);
    ImGui::PopStyleVar(3);

    PollIndexing();
    DrawMenuBar();

    // Three-panel layout with draggable splitters.
    // leftW / rightW are the user's preferred sizes in pixels.
    // Effective sizes scale down proportionally if the window is too narrow,
    // then snap back to preferred when space allows.
    static float leftW  = 280.0f;
    static float rightW = 280.0f;
    const  float splitterW  = 4.0f;
    const  float minPane    = 120.0f;
    // Name(~80) + Type(72) + Size(60) + cell padding(24) + borders + window padding
    const  float minBrowserW = 260.0f;

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float  h     = avail.y;

    float effLeft  = leftW;
    float effRight = rightW;
    float budget   = avail.x - splitterW * 2.0f - minPane;
    if (effLeft + effRight > budget && budget > 0.0f) {
        float scale = budget / (effLeft + effRight);
        effLeft  = effLeft  * scale;
        effRight = effRight * scale;
    }
    effLeft  = effLeft  < minBrowserW ? minBrowserW : effLeft;
    effRight = effRight < minPane ? minPane : effRight;
    float midW = avail.x - effLeft - effRight - splitterW * 2.0f;
    if (midW < minPane) midW = minPane;

    // Left panel
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 4.0f));
    ImGui::BeginChild("##Browser", ImVec2(effLeft, h), false);
    DrawLeftPanel(*this);
    ImGui::EndChild();
    ImGui::PopStyleVar();

    // Left splitter
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImGui::GetStyleColorVec4(ImGuiCol_Separator));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_SeparatorHovered));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImGui::GetStyleColorVec4(ImGuiCol_SeparatorActive));
    ImGui::Button("##split0", ImVec2(splitterW, h));
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemActive()) {
        leftW += ImGui::GetIO().MouseDelta.x;
        if (leftW < minBrowserW) leftW = minBrowserW;
        if (leftW > avail.x - rightW - splitterW * 2.0f - minPane)
            leftW = avail.x - rightW - splitterW * 2.0f - minPane;
    }
    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

    // Center panel
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 4.0f));
    ImGui::BeginChild("##Editor", ImVec2(midW, h), false);
    DrawEditorHost(*this);
    ImGui::EndChild();
    ImGui::PopStyleVar();

    // Right splitter
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImGui::GetStyleColorVec4(ImGuiCol_Separator));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_SeparatorHovered));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImGui::GetStyleColorVec4(ImGuiCol_SeparatorActive));
    ImGui::Button("##split1", ImVec2(splitterW, h));
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemActive()) {
        rightW -= ImGui::GetIO().MouseDelta.x;
        if (rightW < minPane) rightW = minPane;
        if (rightW > avail.x - leftW - splitterW * 2.0f - minPane)
            rightW = avail.x - leftW - splitterW * 2.0f - minPane;
    }
    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

    // Right panel
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 4.0f));
    ImGui::BeginChild("##Preview", ImVec2(effRight, h), false);
    DrawPreview(*this);
    ImGui::EndChild();
    ImGui::PopStyleVar();

    ImGui::End();
}

// ---------- Menu bar ----------

void App::DrawMenuBar() {
    bool openAbout = false;
    bool openPrefs = false;

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open LIB...",  "Ctrl+L")) OpenLibDialog();
            if (ImGui::MenuItem("Open File...", "Ctrl+F")) OpenFileDialog();
            if (ImGui::BeginMenu("Recent Files", !m_recentFiles.empty())) {
                for (const auto& p : m_recentFiles) {
                    std::string label = fs::path(p).filename().string();
                    if (ImGui::MenuItem(label.c_str())) {
                        OpenLib(p);
                        leftView = fxs::icons::Id::Archives;
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Clear")) {
                    m_recentFiles.clear();
                    ImGui::MarkIniSettingsDirty();
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Mount FA Workspace", nullptr, false, !installDir.empty()))
                MountWorkspace();
            ImGui::Separator();
            {
                bool hasSelected = selectedSession >= 0 &&
                                   selectedSession < (int)sessions.size();
                std::string closeName = hasSelected
                    ? "Close " + fs::path(sessions[selectedSession].path).filename().string()
                    : "Close";
                if (ImGui::MenuItem(closeName.c_str(), nullptr, false, hasSelected))
                    CloseSession(selectedSession);
                if (ImGui::MenuItem("Close All", nullptr, false, !sessions.empty()))
                    CloseAllSessions();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Preferences...")) openPrefs = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4"))
                platform::RequestQuit();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Tools")) {
            bool anyLib = false;
            for (int i = 0; i < (int)sessions.size(); i++) {
                if (sessions[i].standalone) continue;
                anyLib = true;
                std::string label = "Install " +
                    fs::path(sessions[i].path).filename().string() +
                    " as FA_0.LIB";
                if (ImGui::MenuItem(label.c_str(), nullptr, false,
                                    !installDir.empty()))
                    InstallToGame(i);
            }
            if (!anyLib)
                ImGui::TextDisabled("(no LIB open)");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            bool any = !sessions.empty();
            if (ImGui::MenuItem("Expand All", nullptr, false, any))
                for (auto& s : sessions) s.forceOpen = 1;
            if (ImGui::MenuItem("Collapse All", nullptr, false, any))
                for (auto& s : sessions) s.forceOpen = 0;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) openAbout = true;
            ImGui::EndMenu();
        }

        // Status message on the right
        if (!statusMsg.empty()) {
            float w = ImGui::CalcTextSize(statusMsg.c_str()).x + 16.0f;
            ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - w);
            if      (statusKind == StatusKind::Error)
                ImGui::TextColored({1.0f, 0.35f, 0.35f, 1.0f}, "%s", statusMsg.c_str());
            else if (statusKind == StatusKind::Warning)
                ImGui::TextColored({1.0f, 0.85f, 0.0f,  1.0f}, "%s", statusMsg.c_str());
            else
                ImGui::TextDisabled("%s", statusMsg.c_str());
        }
        ImGui::EndMenuBar();
    }

    // Keyboard shortcuts
    if (ImGui::IsKeyPressed(ImGuiKey_L, false) && ImGui::GetIO().KeyCtrl)
        OpenLibDialog();
    if (ImGui::IsKeyPressed(ImGuiKey_F, false) && ImGui::GetIO().KeyCtrl)
        OpenFileDialog();

    // Open popups after EndMenuBar so they register in the host window context.
    if (openAbout)             ImGui::OpenPopup("##About");
    if (openPrefs)             ImGui::OpenPopup("##Prefs");
    if (!m_dupLibPath.empty()) ImGui::OpenPopup("##DupLib");

    // Preferences popup
    if (ImGui::BeginPopupModal("##Prefs", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char        dirBuf[1024] = {};
        static std::string dirApplied;
        // Refresh the edit buffer when installDir changes underneath it —
        // the folder dialog completes asynchronously, frames later.
        if (ImGui::IsWindowAppearing() ||
            (dirApplied != installDir && !ImGui::IsAnyItemActive())) {
            fxg::copy_str(dirBuf, sizeof(dirBuf), installDir);
            dirApplied = installDir;
        }

        ImGui::Text("FA Install Directory");
        ImGui::SetNextItemWidth(360.0f);
        if (ImGui::InputText("##installDir", dirBuf, sizeof(dirBuf))) {
            installDir = dirBuf;
            dirApplied = installDir;
            ImGui::MarkIniSettingsDirty();
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse..."))
            ChooseInstallDir();

        ImGui::Separator();
        ImGui::Text("Theme");
        int tp = (int)themePref;
        bool themeChanged = false;
        themeChanged |= ImGui::RadioButton("Auto (system setting)", &tp, (int)ThemePreference::Auto);
        ImGui::SameLine();
        themeChanged |= ImGui::RadioButton("Dark",  &tp, (int)ThemePreference::Dark);
        ImGui::SameLine();
        themeChanged |= ImGui::RadioButton("Light", &tp, (int)ThemePreference::Light);
        if (themeChanged) {
            themePref = static_cast<ThemePreference>(tp);
            platform::ApplyTheme(themePref);
            ImGui::MarkIniSettingsDirty();
        }

        ImGui::Separator();
        if (ImGui::Button("Close", ImVec2(120, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // Duplicate LIB popup
    if (ImGui::BeginPopupModal("##DupLib", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Already open: %s", m_dupLibPath.c_str());
        ImGui::Spacing();
        if (ImGui::Button("OK")) { ImGui::CloseCurrentPopup(); m_dupLibPath.clear(); }
        ImGui::EndPopup();
    }

    // About popup
    if (ImGui::BeginPopupModal("##About", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Fighters Codex GUI  v" FX_VERSION_STRING);
        ImGui::Text("Modern replacement for FATK (DuoSoft 1998)");
        ImGui::Separator();
        ImGui::Text("Backend: fx_lib (C++17)");
        ImGui::Text("GUI: Dear ImGui + SDL3 / OpenGL 3.3");
        if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

// ---------- File dialogs ----------
// Async: the continuation runs frames later on the main thread. `this` is
// safe to capture — the single App outlives the dialog system (dialogs shut
// down before App teardown in main).

void App::OpenLibDialog() {
    platform::OpenFilesDialog(
        {{"LIB archives", "LIB;lib"}, {"All files", "*"}}, true,
        [this](std::vector<std::string> paths) {
            for (const auto& path : paths)
                OpenLib(path);
            if (!paths.empty()) leftView = fxs::icons::Id::Archives;
        });
}

void App::OpenFileDialog() {
    platform::OpenFilesDialog(
        {{"Game files",
          "P;p;RAW;raw;PIC;pic;SH;sh;11K;11k;5K;5k;8K;8k;22K;22k"},
         {"All files", "*"}},
        true,
        [this](std::vector<std::string> paths) {
            for (const auto& path : paths)
                OpenStandaloneFile(path);
            if (!paths.empty()) leftView = fxs::icons::Id::Archives;
        });
}

static const char* kStandaloneExts[] = {
    "p","raw","pic","11k","5k","8k","22k",
    "ot","nt","pt","jt","see","ecm","gas",
    "m","mm","mt","seq","inf","sh","pal", nullptr
};

void App::OpenStandaloneFile(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
    for (auto& c : ext) c = (char)tolower((unsigned char)c);
    bool known = false;
    for (int i = 0; kStandaloneExts[i]; i++)
        if (ext == kStandaloneExts[i]) { known = true; break; }
    if (!known) {
        statusMsg  = "Unsupported file type: ." + ext;
        statusKind = StatusKind::Warning;
        return;
    }

    for (const auto& s : sessions) {
        // error_code overload: fs::equivalent throws when either path does
        // not exist (e.g. a stale recent-files entry from another OS).
        std::error_code ec;
        if (fs::equivalent(fs::path(s.path), fs::path(path), ec)) {
            m_dupLibPath = fs::path(path).filename().string();
            return;
        }
    }

    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) { statusMsg = "Cannot open: " + path; statusKind = StatusKind::Error; return; }
    auto sz = f.tellg(); f.seekg(0);

    LibSession s;
    s.path       = path;
    s.standalone = true;
    s.data.resize((size_t)sz);
    f.read((char*)s.data.data(), sz);

    // Synthetic single-entry directory for the editor pipeline. The name is
    // sanitized like a real archive entry so downstream code can trust it.
    fx::Entry e = {};
    std::string fname = fs::path(path).filename().string();
    fxg::copy_str(e.name, sizeof(e.name), fx::ealib_safe_name(fname.c_str()));
    e.flags  = 0;
    e.offset = 0;
    e.size   = (uint32_t)sz;
    s.entries.push_back(e);

    sessions.insert(sessions.begin(), std::move(s));
    if (editor.libIdx >= 0) editor.libIdx++;
    statusMsg  = "Opened " + fname;
    statusKind = StatusKind::Info;
    AddRecentFile(path);
}

void App::OpenLib(const std::string& path) {
    for (const auto& s : sessions) {
        std::error_code ec;
        if (fs::equivalent(fs::path(s.path), fs::path(path), ec)) {
            m_dupLibPath = fs::path(path).filename().string();
            return;
        }
    }

    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) { statusMsg = "Cannot open: " + path; statusKind = StatusKind::Error; return; }
    auto sz = f.tellg(); f.seekg(0);
    LibSession s;
    s.path = path;
    s.data.resize((size_t)sz);
    f.read((char*)s.data.data(), sz);
    s.entries = fx::ealib_read_dir(s.data.data(), s.data.size());
    if (s.entries.empty()) { statusMsg = "Not a valid LIB: " + path; statusKind = StatusKind::Error; return; }
    sessions.insert(sessions.begin(), std::move(s));
    if (editor.libIdx >= 0) editor.libIdx++;
    statusMsg  = "Opened " + fs::path(path).filename().string() +
                 " (" + std::to_string(sessions.front().entries.size()) + " entries)";
    statusKind = StatusKind::Info;
    AddRecentFile(path);
}

void App::AddRecentFile(const std::string& path) {
    auto it = std::find(m_recentFiles.begin(), m_recentFiles.end(), path);
    if (it != m_recentFiles.end()) m_recentFiles.erase(it);
    m_recentFiles.insert(m_recentFiles.begin(), path);
    if ((int)m_recentFiles.size() > kMaxRecent)
        m_recentFiles.resize(kMaxRecent);
    ImGui::MarkIniSettingsDirty();
}


void App::ChooseInstallDir() {
    platform::ChooseFolderDialog([this](std::string dir) {
        if (dir.empty()) return;
        installDir = std::move(dir);
        ImGui::MarkIniSettingsDirty();
        statusMsg  = "Install dir: " + installDir;
        statusKind = StatusKind::Info;
    });
}

// ---------- Entry open / commit ----------

int App::FindSessionByPath(const std::string& path) const {
    for (int i = 0; i < (int)sessions.size(); ++i) {
        std::error_code ec;
        if (fs::equivalent(fs::path(sessions[i].path), fs::path(path), ec))
            return i;
    }
    return -1;
}

void App::OpenWorkspaceEntry(int node) {
    if (node < 0 || node >= (int)workspace.names.size()) return;
    const fxg::WorkspaceEntry& we = workspace.names[node];
    const fxg::MountSource& src = workspace.sources[we.sourceIdx];

    // Reuse the whole session/editor pipeline: ensure the source file is open
    // as a session, then route through OpenEntry.
    int si = FindSessionByPath(src.path);
    if (si < 0) {
        if (src.isLib) OpenLib(src.path);
        else           OpenStandaloneFile(src.path);
        si = FindSessionByPath(src.path);
        if (si < 0) return; // open failed (status already set)
    }

    int ei = -1;
    for (int i = 0; i < (int)sessions[si].entries.size(); ++i)
        if (fxg::ci_equal(sessions[si].entries[i].name, we.name.c_str())) { ei = i; break; }
    if (ei < 0) return;

    selectedNode = node;
    OpenEntry(si, ei);
}

void App::SelectObject(int node) {
    if (node < 0 || node >= (int)workspace.names.size()) return;
    // The cluster needs the graph; browsers only offer objects once the index
    // is built, but guard anyway so a plain open still works without it.
    if (assetIndex.built && (int)assetIndex.nodes.size() == (int)workspace.names.size()) {
        clusterRoot = node;
        cluster     = fxg::asset_cluster(assetIndex, workspace, node);
    }
    OpenWorkspaceEntry(node);
}

void App::ClearObjectScope() {
    clusterRoot = -1;
    cluster.clear();
    selectedNode = -1;
}

void App::OpenEntry(int libIdx, int entryIdx) {
    if (libIdx < 0 || libIdx >= (int)sessions.size()) return;
    const auto& s = sessions[libIdx];
    if (entryIdx < 0 || entryIdx >= (int)s.entries.size()) return;

    EditorState es;
    es.libIdx   = libIdx;
    es.entryIdx = entryIdx;
    es.data     = s.standalone
        ? s.data
        : fx::ealib_extract(s.data.data(), s.data.size(), s.entries[entryIdx], true);

    std::string name = s.entries[entryIdx].name;
    auto dot = name.rfind('.');
    es.ext = (dot != std::string::npos) ? name.substr(dot + 1) : "";
    for (auto& c : es.ext) c = (char)tolower(c);

    const char* brfExts[] = { "ot","nt","pt","jt","see","ecm","gas" };
    for (auto* x : brfExts)
        if (es.ext == x) { es.kind = EditorKind::Brf; break; }
    if      (es.ext == "pic")                      es.kind = EditorKind::Pic;
    else if (es.ext == "11k" || es.ext == "5k" ||
             es.ext == "8k"  || es.ext == "22k")   es.kind = EditorKind::Audio;
    else if (es.ext == "m"   || es.ext == "mm" ||
             es.ext == "mt")                       es.kind = EditorKind::Mission;
    else if (es.ext == "seq")                      es.kind = EditorKind::Seq;
    else if (es.ext == "inf")                      es.kind = EditorKind::Inf;
    else if (es.ext == "raw")                      es.kind = EditorKind::Raw;
    else if (es.ext == "sh")                       es.kind = EditorKind::Sh;
    else if (es.ext == "p")                        es.kind = EditorKind::Plt;
    else if (es.ext == "txt" || es.ext == "wri" ||
             es.ext == "hlp" || es.ext == "cnt" ||
             es.ext == "ini")                      es.kind = EditorKind::Txt;
    else if (es.ext == "bin" || es.ext == "sms")   es.kind = EditorKind::Bin;
    else if (es.ext == "lay")                      es.kind = EditorKind::Lay;
    else if (es.ext == "hud")                      es.kind = EditorKind::Hud;
    else if (es.ext == "mus")                      es.kind = EditorKind::Mus;
    else if (es.ext == "fnt")                      es.kind = EditorKind::Fnt;
    else if (es.ext == "cb8")                      es.kind = EditorKind::Cb8;
    else if (es.ext == "ai")                       es.kind = EditorKind::Ai;
    else if (es.ext == "xmi")                      es.kind = EditorKind::Xmi;
    else if (es.ext == "vdo" || es.ext == "fbc")   es.kind = EditorKind::Vdo;
    else if (es.ext == "cam")                      es.kind = EditorKind::Cam;
    else if (es.ext == "pal")                      es.kind = EditorKind::Pal;
    else if (es.ext == "t2")                       es.kind = EditorKind::T2;

    if (es.kind == EditorKind::None) {
        statusMsg  = "No editor for ." + es.ext + " files";
        statusKind = StatusKind::Warning;
        return;
    }

    editor = std::move(es);
}

void App::CommitEntry(const std::vector<uint8_t>& newData) {
    if (editor.libIdx < 0) return;
    auto& s = sessions[editor.libIdx];
    std::string name = s.entries[editor.entryIdx].name;

    if (s.standalone) {
        std::ofstream f(s.path, std::ios::binary);
        if (!f) { statusMsg = "Cannot write: " + s.path; statusKind = StatusKind::Error; return; }
        f.write((const char*)newData.data(), (std::streamsize)newData.size());
        s.data            = newData;
        s.entries[0].size = (uint32_t)newData.size();
        s.dirty           = false;
        editor.modified   = false;
        statusMsg         = "Saved " + name;
        statusKind        = StatusKind::Info;
    } else {
        s.data    = fx::ealib_patch(s.data.data(), s.data.size(), name, newData);
        s.entries = fx::ealib_read_dir(s.data.data(), s.data.size());
        s.dirty         = true;
        editor.modified = false;
        statusMsg       = std::string("Patched ") + name;
        statusKind      = StatusKind::Info;
    }
}

void App::CloseSession(int idx) {
    if (idx < 0 || idx >= (int)sessions.size()) return;
    sessions.erase(sessions.begin() + idx);
    if (editor.libIdx == idx)
        editor = EditorState{};
    else if (editor.libIdx > idx)
        editor.libIdx--;
    if (selectedSession == idx)
        selectedSession = -1;
    else if (selectedSession > idx)
        selectedSession--;
    if (palLib == idx) {
        palLib   = -1; // back to Auto
        palEntry = -1;
        palGen++;
    } else if (palLib > idx) {
        palLib--;
    }
}

void App::CloseAllSessions() {
    sessions.clear();
    editor          = EditorState{};
    selectedSession = -1;
    palLib          = -1;
    palEntry        = -1;
    palGen++;
}

void App::MountWorkspace() {
    if (installDir.empty()) {
        statusMsg  = "Set FA install dir first (Preferences).";
        statusKind = StatusKind::Warning;
        return;
    }
    // Join any in-flight index build before replacing the workspace it reads.
    StopIndexing();
    ClearObjectScope(); // node indices are about to change
    workspace = fxg::workspace_scan(installDir);
    if (!workspace.mounted()) {
        statusMsg  = "Not a directory: " + installDir;
        statusKind = StatusKind::Error;
        return;
    }
    // Mounting opts into re-mounting the same root on the next launch, and
    // reveals the object-category browsers (the object-centric default).
    workspaceOnStart = true;
    if (leftView == fxs::icons::Id::Archives) leftView = fxs::icons::Id::Aircraft;
    ImGui::MarkIniSettingsDirty();
    statusMsg = "Mounted " + std::to_string(workspace.libCount) + " LIBs + " +
                std::to_string(workspace.looseCount) + " loose files: " +
                std::to_string(workspace.names.size()) + " names";
    if (!workspace.collisions.empty())
        statusMsg += ", " + std::to_string(workspace.collisions.size()) + " collisions";
    statusKind = StatusKind::Info;

    StartIndexing(); // build the asset-graph index in the background (#362)
}

void App::StartIndexing() {
    StopIndexing();                 // cancel + join any prior build
    if (!workspace.mounted()) return;
    m_indexCancel.stop.store(false);
    m_indexReady.store(false);
    m_indexDone.store(0);
    m_indexTotal.store((int)workspace.sources.size());
    m_indexRunning.store(true);
    // workspace is not mutated while a build runs (MountWorkspace joins first),
    // so the worker may hold a const reference to it.
    const fxg::Workspace* ws = &workspace;
    m_indexThread = std::thread([this, ws] {
        fxg::AssetIndex result = fxg::asset_index_build(
            *ws,
            [this](int done, int total, const std::string&) {
                m_indexDone.store(done);
                m_indexTotal.store(total);
            },
            &m_indexCancel);
        {
            std::lock_guard<std::mutex> lk(m_indexMutex);
            m_indexResult = std::move(result);
        }
        m_indexRunning.store(false);
        m_indexReady.store(true);
    });
}

void App::StopIndexing() {
    if (m_indexThread.joinable()) {
        m_indexCancel.stop.store(true);
        m_indexThread.join();
    }
    m_indexRunning.store(false);
    m_indexReady.store(false);
}

void App::PollIndexing() {
    if (m_indexReady.exchange(false)) {
        std::lock_guard<std::mutex> lk(m_indexMutex);
        assetIndex = std::move(m_indexResult);
        if (assetIndex.built) {
            size_t unassigned =
                assetIndex.byCategory[(int)fxg::Category::Unassigned].size();
            statusMsg = "Indexed " + std::to_string(assetIndex.nodes.size()) +
                        " assets (" + std::to_string(unassigned) + " unassigned)";
            statusKind = StatusKind::Info;
        }
    } else if (m_indexRunning.load()) {
        statusMsg = "Indexing assets... " + std::to_string(m_indexDone.load()) +
                    "/" + std::to_string(m_indexTotal.load()) + " archives";
        statusKind = StatusKind::Info;
    }
}

void App::InstallToGame(int libIdx) {
    if (libIdx < 0 || libIdx >= (int)sessions.size()) return;
    if (installDir.empty()) { statusMsg = "Set FA install dir first."; statusKind = StatusKind::Warning; return; }
    std::string dest = (fs::path(installDir) / "FA_0.LIB").string();
    std::ofstream f(dest, std::ios::binary);
    if (!f) { statusMsg = "Cannot write: " + dest; statusKind = StatusKind::Error; return; }
    const auto& data = sessions[libIdx].data;
    f.write((const char*)data.data(), (std::streamsize)data.size());
    statusMsg  = "Installed to " + dest;
    statusKind = StatusKind::Info;
}

