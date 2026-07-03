#include "theme.h"
#include "imgui.h"
#include <SDL3/SDL.h>

namespace platform {

void ApplyTheme(ThemePreference pref) {
    bool dark = true;
    if (pref == ThemePreference::Auto)
        dark = (SDL_GetSystemTheme() != SDL_SYSTEM_THEME_LIGHT);
    else
        dark = (pref == ThemePreference::Dark);

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

} // namespace platform
