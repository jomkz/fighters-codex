#include "theme.h"
#include "window.h"
#include "imgui.h"
#include <SDL3/SDL.h>

namespace platform {

void ApplyTheme(ThemePreference pref) {
    bool dark = true;
    if (pref == ThemePreference::Auto)
        dark = (SDL_GetSystemTheme() != SDL_SYSTEM_THEME_LIGHT);
    else
        dark = (pref == ThemePreference::Dark);

    // Rebuild from a default-constructed style every time: ScaleAllSizes is
    // cumulative, so scaling must always start from unscaled metrics for
    // this to be idempotent (theme switches, DPI changes mid-session).
    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle();

    if (dark)
        ImGui::StyleColorsDark();
    else
        ImGui::StyleColorsLight();

    style.WindowRounding    = 4.0f;
    style.FrameRounding     = 3.0f;
    style.GrabRounding      = 3.0f;
    style.ScrollbarRounding = 3.0f;

    float scale = DisplayScale();
    style.ScaleAllSizes(scale);
    style.FontScaleDpi = scale; // dynamic fonts: no atlas rebuild needed
}

} // namespace platform
