#include "app.h"
#include "panels/lib_browser.h"
#include "panels/editor_host.h"
#include "panels/preview.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_dx11.h"
#include <fstream>
#include <shlobj.h>
#include <commdlg.h>
#include <string>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

static constexpr int kMaxRecent = 5;

App::App(ID3D11Device* device, ID3D11DeviceContext* ctx)
    : m_device(device), m_ctx(ctx) {

    // Persist app settings (e.g. installDir) inside ft-gui.ini.
    ImGuiSettingsHandler h = {};
    h.TypeName   = "FightersToolkit";
    h.TypeHash   = ImHashStr("FightersToolkit");
    h.UserData   = this;
    h.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler*, const char*) -> void* {
        return (void*)1;
    };
    h.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler* handler, void*, const char* line) {
        App* app = static_cast<App*>(handler->UserData);
        auto readVal = [](const char* line, const char* prefix, std::string& out) -> bool {
            size_t plen = strlen(prefix);
            if (strncmp(line, prefix, plen) != 0) return false;
            out = line + plen;
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
        }
    };
    h.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf) {
        App* app = static_cast<App*>(handler->UserData);
        buf->appendf("[%s][Data]\n", handler->TypeName);
        buf->appendf("InstallDir=%s\n", app->installDir.c_str());
        for (const auto& p : app->m_recentFiles)
            buf->appendf("RecentFile=%s\n", p.c_str());
        buf->append("\n");
    };
    ImGui::AddSettingsHandler(&h);
}

App::~App() {}

void App::Draw() {
    // Host window — fills the OS window's client area exactly.
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
    DrawLibBrowser(*this);
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
                    if (ImGui::MenuItem(label.c_str()))
                        OpenLib(p);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Clear")) {
                    m_recentFiles.clear();
                    ImGui::MarkIniSettingsDirty();
                }
                ImGui::EndMenu();
            }
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
                PostQuitMessage(0);
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
        static char dirBuf[MAX_PATH + 1] = {};
        if (ImGui::IsWindowAppearing())
            strncpy_s(dirBuf, installDir.c_str(), sizeof(dirBuf) - 1);

        ImGui::Text("FA Install Directory");
        ImGui::SetNextItemWidth(360.0f);
        if (ImGui::InputText("##installDir", dirBuf, sizeof(dirBuf))) {
            installDir = dirBuf;
            ImGui::MarkIniSettingsDirty();
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) {
            ChooseInstallDir();
            strncpy_s(dirBuf, installDir.c_str(), sizeof(dirBuf) - 1);
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
        ImGui::Text("Fighters Toolkit GUI");
        ImGui::Text("Modern replacement for FATK (DuoSoft 1998)");
        ImGui::Separator();
        ImGui::Text("Backend: ft_lib (C++17)");
        ImGui::Text("GUI: Dear ImGui + DirectX 11");
        if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

// ---------- File dialogs ----------

static std::string WideToUtf8(const wchar_t* ws) {
    int len = WideCharToMultiByte(CP_UTF8, 0, ws, -1, nullptr, 0, nullptr, nullptr);
    std::string s(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws, -1, s.data(), len, nullptr, nullptr);
    return s;
}

static std::vector<std::string> Win32OpenFiles(const wchar_t* filter,
                                               const wchar_t* title) {
    std::vector<wchar_t> buf(32768, 0);
    OPENFILENAMEW ofn    = {};
    ofn.lStructSize      = sizeof(ofn);
    ofn.hwndOwner        = (HWND)ImGui::GetMainViewport()->PlatformHandleRaw;
    ofn.lpstrFilter      = filter;
    ofn.lpstrFile        = buf.data();
    ofn.nMaxFile         = (DWORD)buf.size();
    ofn.lpstrTitle       = title;
    ofn.Flags            = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
                           OFN_ALLOWMULTISELECT | OFN_EXPLORER;
    if (!GetOpenFileNameW(&ofn)) return {};

    std::vector<std::string> result;
    const wchar_t* p = buf.data();
    std::wstring first(p);
    p += first.size() + 1;

    if (*p == L'\0') {
        // Single selection: first string is the full path.
        result.push_back(WideToUtf8(first.c_str()));
    } else {
        // Multi selection: first string is directory, rest are filenames.
        if (!first.empty() && first.back() != L'\\') first += L'\\';
        while (*p != L'\0') {
            std::wstring fname(p);
            result.push_back(WideToUtf8((first + fname).c_str()));
            p += fname.size() + 1;
        }
    }
    return result;
}

void App::OpenLibDialog() {
    for (const auto& path : Win32OpenFiles(
            L"LIB Files\0*.LIB\0All Files\0*.*\0", L"Open LIB File"))
        OpenLib(path);
}

void App::OpenFileDialog() {
    for (const auto& path : Win32OpenFiles(
            L"Game Files\0*.RAW;*.PLT;*.PIC;*.11K;*.5K;*.8K;*.22K\0",
            L"Open File"))
        OpenStandaloneFile(path);
}

static const char* kStandaloneExts[] = {
    "raw","plt","pic","11k","5k","8k","22k",
    "ot","nt","pt","jt","see","ecm","gas",
    "m","mm","mt","seq","inf", nullptr
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
        if (fs::equivalent(fs::path(s.path), fs::path(path))) {
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

    // Synthetic single-entry directory for the editor pipeline.
    ft::Entry e = {};
    std::string fname = fs::path(path).filename().string();
    strncpy_s(e.name, fname.c_str(), sizeof(e.name) - 1);
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
        if (fs::equivalent(fs::path(s.path), fs::path(path))) {
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
    s.entries = ft::ealib_read_dir(s.data.data(), s.data.size());
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
    wchar_t buf[MAX_PATH] = {};
    BROWSEINFOW bi = {};
    bi.hwndOwner = (HWND)ImGui::GetMainViewport()->PlatformHandleRaw;
    bi.lpszTitle = L"Select FA install directory";
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return;
    SHGetPathFromIDListW(pidl, buf);
    CoTaskMemFree(pidl);
    int len = WideCharToMultiByte(CP_UTF8, 0, buf, -1, nullptr, 0, nullptr, nullptr);
    installDir.assign(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, buf, -1, installDir.data(), len, nullptr, nullptr);
    ImGui::MarkIniSettingsDirty();
    statusMsg  = "Install dir: " + installDir;
    statusKind = StatusKind::Info;
}

// ---------- Entry open / commit ----------

void App::OpenEntry(int libIdx, int entryIdx) {
    if (libIdx < 0 || libIdx >= (int)sessions.size()) return;
    const auto& s = sessions[libIdx];
    if (entryIdx < 0 || entryIdx >= (int)s.entries.size()) return;

    EditorState es;
    es.libIdx   = libIdx;
    es.entryIdx = entryIdx;
    es.data     = s.standalone
        ? s.data
        : ft::ealib_extract(s.data.data(), s.data.size(), s.entries[entryIdx], true);

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
        s.data    = ft::ealib_patch(s.data.data(), s.data.size(), name, newData);
        s.entries = ft::ealib_read_dir(s.data.data(), s.data.size());
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
}

void App::CloseAllSessions() {
    sessions.clear();
    editor          = EditorState{};
    selectedSession = -1;
}

void App::InstallToGame(int libIdx) {
    if (libIdx < 0 || libIdx >= (int)sessions.size()) return;
    if (installDir.empty()) { statusMsg = "Set FA install dir first."; statusKind = StatusKind::Warning; return; }
    std::string dest = installDir + "\\FA_0.LIB";
    std::ofstream f(dest, std::ios::binary);
    if (!f) { statusMsg = "Cannot write: " + dest; statusKind = StatusKind::Error; return; }
    const auto& data = sessions[libIdx].data;
    f.write((const char*)data.data(), (std::streamsize)data.size());
    statusMsg  = "Installed to " + dest;
    statusKind = StatusKind::Info;
}

// ---------- GPU texture upload ----------

GpuTexture App::UploadTexture(const uint8_t* rgba, int w, int h) {
    GpuTexture t;
    if (!rgba || w <= 0 || h <= 0) return t;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width              = (UINT)w;
    desc.Height             = (UINT)h;
    desc.MipLevels          = 1;
    desc.ArraySize          = 1;
    desc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count   = 1;
    desc.Usage              = D3D11_USAGE_DEFAULT;
    desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem     = rgba;
    init.SysMemPitch = (UINT)(w * 4);

    ID3D11Texture2D* tex = nullptr;
    if (FAILED(m_device->CreateTexture2D(&desc, &init, &tex))) return t;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format                    = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels       = 1;
    if (FAILED(m_device->CreateShaderResourceView(tex, &srvDesc, &t.srv))) {
        tex->Release(); return t;
    }
    tex->Release();
    t.width  = w;
    t.height = h;
    return t;
}
