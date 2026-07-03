#pragma once

enum class ThemePreference { Auto = 0, Dark = 1, Light = 2 };

namespace platform {

// Apply the ImGui colour scheme for the preference and re-apply the app's
// rounding overrides (so it survives mid-session switches). Auto resolves
// via SDL_GetSystemTheme; UNKNOWN (e.g. X11 without a desktop portal) falls
// back to dark. Live switching is driven by SDL_EVENT_SYSTEM_THEME_CHANGED
// in the main loop.
void ApplyTheme(ThemePreference pref);

} // namespace platform
