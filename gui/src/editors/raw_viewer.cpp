#include "raw_viewer.h"
#include "../app.h"
#include "../platform/dialogs.h"
#include "imgui.h"
#include "fx/raw.h"
#include "stb_image_write.h"
#include <string>
#include <filesystem>
namespace fs = std::filesystem;

void DrawRawViewer(App& app) {
    auto& ed = app.editor;
    fx::RawInfo info;
    if (!fx::raw_info(ed.data.data(), ed.data.size(), &info)) {
        ImGui::TextColored({1,0.4f,0.4f,1}, "Not a valid RAW screenshot.");
        return;
    }

    ImGui::Text("Resolution: %u x %u", info.width, info.height);
    ImGui::Text("File size:  %zu bytes", ed.data.size());
    ImGui::Separator();

    if (ImGui::Button("Export PNG...")) {
        // Build a default save path: same dir/name as source, .png extension
        std::string defaultPath;
        if (ed.libIdx >= 0 && ed.libIdx < (int)app.sessions.size()) {
            const auto& sess = app.sessions[ed.libIdx];
            if (sess.standalone) {
                defaultPath = fs::path(sess.path).replace_extension(".png").string();
            } else if (ed.entryIdx >= 0 && ed.entryIdx < (int)sess.entries.size()) {
                fs::path stem = fs::path(sess.entries[ed.entryIdx].name).stem();
                defaultPath = (fs::path(sess.path).parent_path() / (stem.string() + ".png")).string();
            }
        }

        // Decode before the dialog opens — the continuation runs frames
        // later, when the selection may have changed.
        auto rgba = fx::raw_decode(ed.data.data(), ed.data.size());
        if (!rgba.empty()) {
            platform::SaveFileDialog(
                {{"PNG image", "png;PNG"}, {"All files", "*"}}, "png",
                defaultPath.empty() ? nullptr : defaultPath.c_str(),
                [&app, rgba = std::move(rgba), w = (int)info.width,
                 h = (int)info.height](std::string out) {
                    if (out.empty()) return;
                    if (stbi_write_png(out.c_str(), w, h, 4, rgba.data(), w * 4)) {
                        app.statusMsg  = "Exported " + fs::path(out).filename().string();
                        app.statusKind = App::StatusKind::Info;
                    } else {
                        app.statusMsg  = "PNG write failed: " + out;
                        app.statusKind = App::StatusKind::Error;
                    }
                });
        }
    }

    ImGui::TextDisabled("Preview shown in the Preview panel.");
}
