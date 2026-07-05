#include "window.h"
#include <glad/gl.h>
#include <SDL3/SDL.h>
#include <string>

namespace platform {
namespace {

SDL_Window*   g_window  = nullptr;
SDL_GLContext g_context = nullptr;
bool          g_quiet   = false;

bool Fail(const char* what) {
    std::string msg = std::string(what) + ": " + SDL_GetError();
    if (g_quiet)
        SDL_Log("%s", msg.c_str());
    else
        // The WIN32 subsystem has no console — report fatal init errors
        // visibly. Falls through harmlessly when no display exists.
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "fxs",
                                 msg.c_str(), nullptr);
    return false;
}

} // namespace

bool CreateWindowGL(const WindowConfig& cfg) {
    g_quiet = cfg.quiet;
    if (!SDL_Init(SDL_INIT_VIDEO))
        return Fail("SDL_Init failed");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    g_window = SDL_CreateWindow(cfg.title, cfg.width, cfg.height,
                                SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!g_window)
        return Fail("SDL_CreateWindow failed");

    g_context = SDL_GL_CreateContext(g_window);
    if (!g_context)
        return Fail("SDL_GL_CreateContext (GL 3.3 core) failed");
    SDL_GL_MakeCurrent(g_window, g_context);
    SDL_GL_SetSwapInterval(1);

    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress))
        return Fail("OpenGL function loading failed");

    SDL_SetWindowMinimumSize(g_window, cfg.minWidth, cfg.minHeight);
    return true;
}

void DestroyWindowGL() {
    if (g_context) { SDL_GL_DestroyContext(g_context); g_context = nullptr; }
    if (g_window)  { SDL_DestroyWindow(g_window);      g_window  = nullptr; }
    SDL_Quit();
}

SDL_Window* Window() { return g_window; }

void RequestQuit() {
    SDL_Event ev;
    SDL_zero(ev);
    ev.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&ev);
}

float DisplayScale() {
    float s = g_window ? SDL_GetWindowDisplayScale(g_window) : 1.0f;
    return s > 0.0f ? s : 1.0f;
}

} // namespace platform
