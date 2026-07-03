#pragma once

struct SDL_Window;

namespace platform {

struct WindowConfig {
    const char* title;
    int width;
    int height;
    int minWidth;
    int minHeight;
    bool quiet = false; // report failures to stderr only (headless smoke runs)
};

// Create the (hidden) SDL window with a GL 3.3 core context, vsync on, and
// load GL entry points through glad. On failure a message box reports the
// error (the WIN32 subsystem has no console) and false is returned.
bool CreateWindowGL(const WindowConfig& cfg);
void DestroyWindowGL();

SDL_Window* Window();

// Push SDL_EVENT_QUIT — replaces PostQuitMessage for menu-driven exit.
void RequestQuit();

// Content scale of the display the window is on (1.0 when unknown).
float DisplayScale();

} // namespace platform
