#define NOMINMAX
#include "app.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <dxgi.h>
#include <tchar.h>
#include <algorithm>
#include <string>

// Forward declaration of ImGui Win32 message handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// ---------- DX11 globals ----------
static ID3D11Device*            g_device      = nullptr;
static ID3D11DeviceContext*     g_context     = nullptr;
static IDXGISwapChain*          g_swapChain   = nullptr;
static ID3D11RenderTargetView*  g_mainRTV     = nullptr;
static App*                     g_app         = nullptr;
static HWND                     g_hwnd        = nullptr;

static bool CreateDeviceD3D(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount                        = 2;
    sd.BufferDesc.Width                   = 0;
    sd.BufferDesc.Height                  = 0;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = hwnd;
    sd.SampleDesc.Count                   = 1;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;
    sd.Windowed                           = TRUE;

    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        featureLevels, 2, D3D11_SDK_VERSION,
        &sd, &g_swapChain, &g_device, nullptr, &g_context);
    if (FAILED(hr)) return false;

    ID3D11Texture2D* backBuffer = nullptr;
    g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    g_device->CreateRenderTargetView(backBuffer, nullptr, &g_mainRTV);
    backBuffer->Release();
    return true;
}

static void CleanupDeviceD3D() {
    if (g_mainRTV) { g_mainRTV->Release(); g_mainRTV = nullptr; }
    if (g_swapChain) { g_swapChain->Release(); g_swapChain = nullptr; }
    if (g_context)  { g_context->Release();  g_context  = nullptr; }
    if (g_device)   { g_device->Release();   g_device   = nullptr; }
}

static void CreateRenderTarget() {
    ID3D11Texture2D* backBuffer = nullptr;
    g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    g_device->CreateRenderTargetView(backBuffer, nullptr, &g_mainRTV);
    backBuffer->Release();
}

static void CleanupRenderTarget() {
    if (g_mainRTV) { g_mainRTV->Release(); g_mainRTV = nullptr; }
}

// ---------- Render ----------
static void RenderFrame() {
    if (!g_app || !g_swapChain) return;

    // Sync swap chain to current client size before rendering.
    RECT cr = {};
    GetClientRect(g_hwnd, &cr);
    UINT newW = (UINT)(cr.right  - cr.left);
    UINT newH = (UINT)(cr.bottom - cr.top);
    if (newW > 0 && newH > 0) {
        DXGI_SWAP_CHAIN_DESC desc = {};
        g_swapChain->GetDesc(&desc);
        if (desc.BufferDesc.Width != newW || desc.BufferDesc.Height != newH) {
            CleanupRenderTarget();
            g_swapChain->ResizeBuffers(0, newW, newH, DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
    }

    if (!g_mainRTV) return;
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    g_app->Draw();
    ImGui::Render();
    const float clearColor[4] = { 0.12f, 0.12f, 0.12f, 1.0f };
    g_context->OMSetRenderTargets(1, &g_mainRTV, nullptr);
    g_context->ClearRenderTargetView(g_mainRTV, clearColor);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_swapChain->Present(1, 0);
}

// ---------- Theme ----------
// Applies the correct ImGui colour scheme based on App::themePref (when the
// App exists) or falls back to the system setting (Auto behaviour).
// Also re-applies rounding so it survives mid-session theme switches.
// Not static â€” called from app.cpp via forward declaration.
void ApplySystemTheme() {
    ThemePreference pref = g_app ? g_app->themePref : ThemePreference::Auto;
    bool dark = true;
    if (pref == ThemePreference::Auto) {
        DWORD useLightTheme = 0;
        DWORD sz = sizeof(useLightTheme);
        RegGetValueW(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            L"AppsUseLightTheme",
            RRF_RT_DWORD, nullptr, &useLightTheme, &sz);
        dark = (useLightTheme == 0);
    } else {
        dark = (pref == ThemePreference::Dark);
    }
    if (dark)
        ImGui::StyleColorsDark();
    else
        ImGui::StyleColorsLight();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 4.0f;
    style.FrameRounding     = 3.0f;
    style.GrabRounding      = 3.0f;
    style.ScrollbarRounding = 3.0f;
}

// ---------- Win32 window proc ----------
static LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return true;
    switch (msg) {
    case WM_GETMINMAXINFO: {
        RECT work = {};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        int workW = work.right  - work.left;
        int workH = work.bottom - work.top;
        int minW  = std::min(std::max(800, workW * 2 / 5), workW);
        int minH  = std::min(std::max(500, workH * 2 / 5), workH);
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
        mmi->ptMinTrackSize = { minW, minH };
        return 0;
    }
    case WM_SIZE:
        if (wp == SIZE_MINIMIZED) return 0;
        return 0;
    case WM_PAINT:
        RenderFrame();
        ValidateRect(hwnd, nullptr);
        return 0;
    case WM_SYSCOMMAND:
        if ((wp & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_SETTINGCHANGE:
        if (lp && wcscmp(reinterpret_cast<LPCWSTR>(lp), L"ImmersiveColorSet") == 0)
            if (!g_app || g_app->themePref == ThemePreference::Auto)
                ApplySystemTheme();
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------- Window placement (via ini handler) ----------

static constexpr int kDefaultW = 1400;
static constexpr int kDefaultH = 900;

// Populated by the ini handler's ReadLineFn.
static RECT s_winRect  = {};
static int  s_winShow  = SW_SHOWNORMAL;
static bool s_winValid = false;

static void CenterWindow(HWND hwnd) {
    RECT work = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    int x = work.left + ((work.right  - work.left) - kDefaultW) / 2;
    int y = work.top  + ((work.bottom - work.top)  - kDefaultH) / 2;
    SetWindowPos(hwnd, nullptr, x, y, kDefaultW, kDefaultH, SWP_NOZORDER | SWP_NOACTIVATE);
}

static bool ApplyWindowPlacement(HWND hwnd) {
    if (!s_winValid) return false;
    int w = s_winRect.right - s_winRect.left;
    int h = s_winRect.bottom - s_winRect.top;
    if (w < 400 || h < 300 || w > 7680 || h > 4320) return false;
    if (!MonitorFromRect(&s_winRect, MONITOR_DEFAULTTONULL)) return false;
    WINDOWPLACEMENT wp = {};
    wp.length           = sizeof(wp);
    wp.rcNormalPosition = s_winRect;
    wp.showCmd = (s_winShow == SW_SHOWMAXIMIZED) ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
    SetWindowPlacement(hwnd, &wp);
    return true;
}

// ---------- WinMain ----------
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_CLASSDC;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"FT_GUI";
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassExW(&wc);

    // Position is set after ini load via ApplyWindowPlacement / CenterWindow.
    HWND hwnd = CreateWindowW(L"FT_GUI", L"Fighters Toolkit",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, kDefaultW, kDefaultH,
        nullptr, nullptr, hInst, nullptr);

    g_hwnd = hwnd;

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, hInst);
        return 1;
    }

    // Init ImGui before creating App so all handlers are registered before
    // we call LoadIniSettingsFromDisk â€” window placement needs to be read
    // before ShowWindow.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = "fx-gui.ini";
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ApplySystemTheme();

    // Load a crisper font from the Windows system font directory.
    // Consolas (monospace) is ideal for data/code editing; Tahoma as fallback.
    {
        wchar_t winDir[MAX_PATH] = {};
        GetWindowsDirectoryW(winDir, MAX_PATH);
        // Convert to UTF-8
        int len = WideCharToMultiByte(CP_UTF8, 0, winDir, -1, nullptr, 0, nullptr, nullptr);
        std::string winDirA(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, winDir, -1, winDirA.data(), len, nullptr, nullptr);

        const char* candidates[] = {
            "\\Fonts\\consola.ttf",   // Consolas Regular
            "\\Fonts\\tahoma.ttf",    // Tahoma (fallback)
            nullptr
        };
        bool loaded = false;
        for (int ci = 0; candidates[ci] && !loaded; ci++) {
            std::string fontPath = winDirA + candidates[ci];
            if (io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 14.0f))
                loaded = true;
        }
        if (!loaded)
            io.Fonts->AddFontDefault();
    }

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_device, g_context);

    // Register window placement handler.
    {
        ImGuiSettingsHandler wh = {};
        wh.TypeName   = "FightersToolkitWindow";
        wh.TypeHash   = ImHashStr("FightersToolkitWindow");
        wh.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler*, const char*) -> void* {
            return (void*)1;
        };
        wh.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler*, void*, const char* line) {
            int v;
            if      (sscanf(line, "Left=%d",   &v) == 1) s_winRect.left   = v;
            else if (sscanf(line, "Top=%d",    &v) == 1) s_winRect.top    = v;
            else if (sscanf(line, "Right=%d",  &v) == 1) s_winRect.right  = v;
            else if (sscanf(line, "Bottom=%d", &v) == 1) s_winRect.bottom = v;
            else if (sscanf(line, "Show=%d",   &v) == 1) { s_winShow = v; s_winValid = true; }
        };
        wh.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* h, ImGuiTextBuffer* buf) {
            WINDOWPLACEMENT wp = {}; wp.length = sizeof(wp);
            if (!GetWindowPlacement(g_hwnd, &wp)) return;
            const RECT& r = wp.rcNormalPosition;
            buf->appendf("[%s][Data]\n", h->TypeName);
            buf->appendf("Left=%d\nTop=%d\nRight=%d\nBottom=%d\nShow=%d\n",
                         r.left, r.top, r.right, r.bottom, (int)wp.showCmd);
            buf->append("\n");
        };
        ImGui::AddSettingsHandler(&wh);
    }

    // Create App â€” registers the FightersToolkit handler (installDir, recent files).
    App app(g_device, g_context);
    g_app = &app;

    // Load ini now that all handlers are registered.
    ImGui::LoadIniSettingsFromDisk(io.IniFilename);

    // Re-apply theme now that App::themePref has been populated from the ini.
    // (The earlier call at context init used Auto because g_app wasn't set yet.)
    ApplySystemTheme();

    // Position window from saved placement, or center if none / invalid.
    if (!ApplyWindowPlacement(hwnd))
        CenterWindow(hwnd);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    bool done = false;
    while (!done) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;
        RenderFrame();
    }

    g_app = nullptr;

    // Flush final window state to ini before shutdown.
    ImGui::MarkIniSettingsDirty();
    ImGui::SaveIniSettingsToDisk(io.IniFilename);

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, hInst);
    return 0;
}
