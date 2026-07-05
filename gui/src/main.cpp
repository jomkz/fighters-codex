#include "app.h"
#include "panels/preview.h"
#include "platform/dialogs.h"
#include "platform/fonts.h"
#include "platform/theme.h"
#include "platform/window.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
#include "stb_image_write.h" // implementation lives in pic_editor.cpp
#include <glad/gl.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static App* g_app = nullptr;

static constexpr int kDefaultW = 1400;
static constexpr int kDefaultH = 900;
static constexpr int kMinW     = 800;
static constexpr int kMinH     = 500;

// ---------- Render ----------

// Guarded against re-entry: the resize event watch can fire while a frame is
// already being built (live-resize on Windows runs a modal loop).
static bool s_inFrame = false;

static void RenderFrame() {
    if (!g_app || s_inFrame) return;
    s_inFrame = true;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    g_app->Draw();
    ImGui::Render();

    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(platform::Window(), &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(platform::Window());

    s_inFrame = false;
}

// Keep rendering while SDL is blocked in a modal resize/expose loop
// (replaces the WM_PAINT redraw path of the old Win32 host).
static bool SDLCALL ResizeWatch(void*, SDL_Event* ev) {
    if (ev->type == SDL_EVENT_WINDOW_EXPOSED ||
        ev->type == SDL_EVENT_WINDOW_RESIZED)
        RenderFrame();
    return true;
}

// ---------- Window placement (via ini handler) ----------

// Placement read back from the ini at startup.
struct SavedPlacement {
    int  x = 0, y = 0, w = 0, h = 0;
    bool maximized = false;
    bool valid     = false;
};
static SavedPlacement s_saved;

// Last known non-maximized rect, tracked from MOVED/RESIZED events — SDL
// reports the maximized geometry from SDL_GetWindowPosition/Size, so the
// normal rect has to be cached while the window is unmaximized.
struct TrackedRect {
    int  x = 0, y = 0, w = kDefaultW, h = kDefaultH;
    bool valid = false;
};
static TrackedRect s_normalRect;

static bool ApplySavedPlacement() {
    if (!s_saved.valid) return false;
    if (s_saved.w < 400 || s_saved.h < 300 || s_saved.w > 7680 || s_saved.h > 4320)
        return false;
    SDL_Point center = {s_saved.x + s_saved.w / 2, s_saved.y + s_saved.h / 2};
    if (SDL_GetDisplayForPoint(&center) == 0) return false; // display went away

    SDL_Window* win = platform::Window();
    SDL_SetWindowSize(win, s_saved.w, s_saved.h);
    SDL_SetWindowPosition(win, s_saved.x, s_saved.y);
    if (s_saved.maximized) SDL_MaximizeWindow(win);
    return true;
}

static void RegisterWindowSettingsHandler() {
    ImGuiSettingsHandler wh = {};
    wh.TypeName   = "FightersToolkitWindow";
    wh.TypeHash   = ImHashStr("FightersToolkitWindow");
    wh.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler*, const char*) -> void* {
        return (void*)1;
    };
    wh.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler*, void*, const char* line) {
        int v = 0;
        if      (sscanf(line, "X=%d",   &v) == 1) s_saved.x = v;
        else if (sscanf(line, "Y=%d",   &v) == 1) s_saved.y = v;
        else if (sscanf(line, "W=%d",   &v) == 1) s_saved.w = v;
        else if (sscanf(line, "H=%d",   &v) == 1) s_saved.h = v;
        else if (sscanf(line, "Max=%d", &v) == 1) { s_saved.maximized = v != 0; s_saved.valid = true; }
    };
    wh.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* h, ImGuiTextBuffer* buf) {
        SDL_Window* win = platform::Window();
        if (!win) return;
        bool maxed = (SDL_GetWindowFlags(win) & SDL_WINDOW_MAXIMIZED) != 0;
        int x, y, w, hgt;
        if (s_normalRect.valid) {
            x = s_normalRect.x; y = s_normalRect.y;
            w = s_normalRect.w; hgt = s_normalRect.h;
        } else {
            SDL_GetWindowPosition(win, &x, &y);
            SDL_GetWindowSize(win, &w, &hgt);
        }
        buf->appendf("[%s][Data]\n", h->TypeName);
        buf->appendf("X=%d\nY=%d\nW=%d\nH=%d\nMax=%d\n", x, y, w, hgt, maxed ? 1 : 0);
        buf->append("\n");
    };
    ImGui::AddSettingsHandler(&wh);
}

// ---------- Smoke sweep ----------

// Headless acceptance sweep (#89): open each LIB, cycle every entry through
// its editor and the preview — one rendered frame per entry — exercising
// extraction, every format parser, and the GL upload paths against real
// game data. Returns the number of LIBs that failed to open.
static int RunSmokeSweep(App& app, const std::vector<std::string>& libs) {
    int failures = 0;
    for (const auto& path : libs) {
        size_t before = app.sessions.size();
        app.OpenLib(path);
        if (app.sessions.size() == before) {
            std::fprintf(stderr, "smoke: FAILED to open %s (%s)\n",
                         path.c_str(), app.statusMsg.c_str());
            failures++;
            continue;
        }
        RenderFrame();
        const bool verbose = std::getenv("FX_SMOKE_VERBOSE") != nullptr;
        int entries = (int)app.sessions[0].entries.size();
        for (int ei = 0; ei < entries; ++ei) {
            if (verbose)
                std::fprintf(stderr, "smoke: entry %d %s\n", ei,
                             app.sessions[0].entries[ei].name);
            app.OpenEntry(0, ei);
            RenderFrame();
        }
        std::printf("smoke: %s — %d entries swept\n", path.c_str(), entries);
        app.CloseAllSessions();
    }
    return failures;
}

// ---------- Headless PNG snapshot ----------

// Build one frame and read the back buffer into a PNG (top-left origin), for
// automated visual review of the preview (SH 3D, PIC/RAW/CB8, editors). Uses
// the same render path as the interactive app, so the image is faithful.
static bool CaptureFrame(const std::string& outPath) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    g_app->Draw();
    ImGui::Render();

    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(platform::Window(), &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    std::vector<unsigned char> px((size_t)w * h * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    SDL_GL_SwapWindow(platform::Window());

    // GL reads bottom-up; PNG is top-down.
    std::vector<unsigned char> flip((size_t)w * h * 4);
    for (int y = 0; y < h; ++y)
        std::memcpy(&flip[(size_t)(h - 1 - y) * w * 4],
                    &px[(size_t)y * w * 4], (size_t)w * 4);

    return stbi_write_png(outPath.c_str(), w, h, 4, flip.data(), w * 4) != 0;
}

// Open a LIB, select an entry (by 8.3 name or numeric index), let the preview
// settle, and write a PNG. Returns 0 on success.
static int RunRender(App& app, const std::string& lib, const std::string& entry,
                     const std::string& out) {
    size_t before = app.sessions.size();
    app.OpenLib(lib);
    if (app.sessions.size() == before) {
        std::fprintf(stderr, "render: FAILED to open %s (%s)\n",
                     lib.c_str(), app.statusMsg.c_str());
        return 1;
    }
    const auto& entries = app.sessions[0].entries;

    int idx = -1;
    bool numeric = !entry.empty();
    for (char c : entry) numeric = numeric && (std::isdigit((unsigned char)c) != 0);
    if (numeric) {
        idx = std::atoi(entry.c_str());
    } else {
        for (int i = 0; i < (int)entries.size(); ++i)
            if (SDL_strcasecmp(entries[i].name, entry.c_str()) == 0) { idx = i; break; }
    }
    if (idx < 0 || idx >= (int)entries.size()) {
        std::fprintf(stderr, "render: entry '%s' not found in %s\n",
                     entry.c_str(), lib.c_str());
        return 1;
    }

    app.OpenEntry(0, idx);
    // Settle: the SH mesh builds on the first frame after selection and the
    // docked layout needs a couple of frames to resolve panel sizes.
    for (int i = 0; i < 4; ++i) RenderFrame();

    if (!CaptureFrame(out)) {
        std::fprintf(stderr, "render: failed to write %s\n", out.c_str());
        return 1;
    }
    std::printf("render: %s [%s] -> %s\n", lib.c_str(), entries[idx].name, out.c_str());
    return 0;
}

// ---------- main ----------

int main(int argc, char** argv) {
    bool smoke = false;
    bool render = false;
    std::string renderOut, renderSize;
    std::vector<std::string> positionals; // LIB paths (smoke) or LIB + ENTRY (render)
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--smoke") == 0)
            smoke = true;
        else if (std::strcmp(argv[i], "--render") == 0)
            render = true;
        else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc)
            renderOut = argv[++i];
        else if (std::strcmp(argv[i], "--size") == 0 && i + 1 < argc)
            renderSize = argv[++i];
        else
            positionals.push_back(argv[i]);
    }
    const bool headless = smoke || render; // hidden window, no ini, offscreen fallback

    int winW = kDefaultW, winH = kDefaultH;
    if (render && !renderSize.empty()) {
        int rw = 0, rh = 0;
        if (std::sscanf(renderSize.c_str(), "%dx%d", &rw, &rh) == 2 &&
            rw >= kMinW && rh >= kMinH) { winW = rw; winH = rh; }
    }

    platform::WindowConfig cfg = {"Fighters Studio", winW, winH,
                                  kMinW, kMinH, headless};
    if (!platform::CreateWindowGL(cfg)) {
        if (!headless) return 1;
        // Headless run without a display server: retry offscreen.
        platform::DestroyWindowGL();
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
        if (!platform::CreateWindowGL(cfg)) return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    // Settings live in the per-user preferences path, not the CWD.
    static std::string iniPath;
    if (!headless) {
        if (char* pref = SDL_GetPrefPath("jomkz", "fxs")) {
            iniPath = std::string(pref) + "fxs.ini";
            SDL_free(pref);
        } else {
            iniPath = "fxs.ini";
        }
        io.IniFilename = iniPath.c_str();
    } else {
        io.IniFilename = nullptr;
    }
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    platform::ApplyTheme(ThemePreference::Auto);
    platform::LoadFonts(14.0f);

    ImGui_ImplSDL3_InitForOpenGL(platform::Window(), SDL_GL_GetCurrentContext());
    ImGui_ImplOpenGL3_Init("#version 330");

    // Register all settings handlers before LoadIniSettingsFromDisk —
    // window placement must be applied before the window is shown.
    RegisterWindowSettingsHandler();
    App app; // registers the FightersToolkit handler (installDir, recent files)
    g_app = &app;
    platform::DialogsInit(platform::Window());

    if (io.IniFilename)
        ImGui::LoadIniSettingsFromDisk(io.IniFilename);

    // Re-apply theme now that App::themePref has been populated from the ini.
    platform::ApplyTheme(app.themePref);

    if (!headless) {
        if (!ApplySavedPlacement())
            SDL_SetWindowPosition(platform::Window(),
                                  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        SDL_ShowWindow(platform::Window());
    }
    SDL_AddEventWatch(ResizeWatch, nullptr);

    int  exitCode    = 0;
    bool done        = false;
    int  smokeFrames = smoke ? 3 : -1;

    if (render) {
        SDL_GL_SetSwapInterval(0);
        std::string lib   = positionals.size() > 0 ? positionals[0] : "";
        std::string entry = positionals.size() > 1 ? positionals[1] : "";
        if (lib.empty() || entry.empty()) {
            std::fprintf(stderr,
                "usage: fxs --render <LIB> <ENTRY> [--out file.png] [--size WxH]\n");
            exitCode = 2;
        } else {
            std::string out = renderOut.empty() ? "render.png" : renderOut;
            exitCode = RunRender(app, lib, entry, out);
        }
        done = true;
    }

    if (smoke && !render) {
        SDL_GL_SetSwapInterval(0); // don't vsync-throttle the sweep
        if (!positionals.empty()) {
            exitCode = RunSmokeSweep(app, positionals) ? 1 : 0;
            done = true; // sweep replaces the interactive loop
        }
    }

    while (!done) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL3_ProcessEvent(&ev);
            switch (ev.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                done = true;
                break;
            case SDL_EVENT_SYSTEM_THEME_CHANGED:
                if (g_app->themePref == ThemePreference::Auto)
                    platform::ApplyTheme(ThemePreference::Auto);
                break;
            case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
                // ApplyTheme rebuilds the style from scratch and re-scales
                // to the new display scale.
                platform::ApplyTheme(g_app->themePref);
                break;
            case SDL_EVENT_WINDOW_MOVED:
                if (!(SDL_GetWindowFlags(platform::Window()) & SDL_WINDOW_MAXIMIZED)) {
                    s_normalRect.x = ev.window.data1;
                    s_normalRect.y = ev.window.data2;
                    s_normalRect.valid = true;
                }
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                if (!(SDL_GetWindowFlags(platform::Window()) & SDL_WINDOW_MAXIMIZED)) {
                    s_normalRect.w = ev.window.data1;
                    s_normalRect.h = ev.window.data2;
                    s_normalRect.valid = true;
                }
                break;
            }
        }
        if (done) break;

        platform::PumpDialogResults();

        if (SDL_GetWindowFlags(platform::Window()) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            continue;
        }
        RenderFrame();

        if (smokeFrames > 0 && --smokeFrames == 0)
            done = true;
    }

    SDL_RemoveEventWatch(ResizeWatch, nullptr);
    platform::DialogsShutdown();

    // Flush final window state to ini before shutdown (App must still be
    // alive — its settings handler runs here too).
    if (io.IniFilename) {
        ImGui::MarkIniSettingsDirty();
        ImGui::SaveIniSettingsToDisk(io.IniFilename);
    }
    g_app = nullptr;

    // Release preview GPU resources while the GL context is still current.
    PreviewShutdown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    platform::DestroyWindowGL();
    return exitCode;
}
