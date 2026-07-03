#include "fonts.h"
#include "imgui.h"
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace platform {

void LoadFonts(float sizePx) {
    ImGuiIO& io = ImGui::GetIO();

    std::vector<std::string> candidates;
    if (const char* windir = std::getenv("WINDIR")) {
        candidates.push_back(std::string(windir) + "\\Fonts\\consola.ttf");
        candidates.push_back(std::string(windir) + "\\Fonts\\tahoma.ttf");
    }
    // Fedora
    candidates.push_back("/usr/share/fonts/dejavu-sans-mono-fonts/DejaVuSansMono.ttf");
    candidates.push_back("/usr/share/fonts/liberation-mono/LiberationMono-Regular.ttf");
    // Debian/Ubuntu (CI images)
    candidates.push_back("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf");
    candidates.push_back("/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf");

    for (const auto& path : candidates) {
        std::error_code ec;
        if (!fs::exists(path, ec)) continue;
        if (io.Fonts->AddFontFromFileTTF(path.c_str(), sizePx))
            return;
    }
    io.Fonts->AddFontDefault();
}

} // namespace platform
