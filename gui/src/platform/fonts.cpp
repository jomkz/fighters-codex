#include "fonts.h"
#include "imgui.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace platform {

void LoadFonts(float sizePx) {
    ImGuiIO& io = ImGui::GetIO();

    // Fixed probe list — non-standard installs simply fall through to the
    // embedded default font.
    static const char* const kCandidates[] = {
        // Windows
        "C:\\Windows\\Fonts\\consola.ttf",
        "C:\\Windows\\Fonts\\tahoma.ttf",
        // Fedora
        "/usr/share/fonts/liberation-mono-fonts/LiberationMono-Regular.ttf",
        "/usr/share/fonts/dejavu-sans-mono-fonts/DejaVuSansMono.ttf",
        // Debian/Ubuntu (CI images)
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    };

    for (const char* path : kCandidates) {
        std::error_code ec;
        if (!fs::exists(path, ec)) continue;
        if (io.Fonts->AddFontFromFileTTF(path, sizePx))
            return;
    }
    io.Fonts->AddFontDefault();
}

} // namespace platform
