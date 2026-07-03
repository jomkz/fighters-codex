#include "pic_editor.h"
#include "../app.h"
#include "../palettes.h"
#include "../platform/dialogs.h"
#include "imgui.h"
#include "fx/pic.h"
#include "fx/pal.h"
#include "fx/ealib.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "stb_image.h"

#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
namespace fs = std::filesystem;

void DrawPicEditor(App& app) {
    auto& ed = app.editor;
    fx::PicInfo info;
    bool valid = fx::pic_info(ed.data.data(), ed.data.size(), &info);

    if (valid && info.format != 0xD8FF) {
        ImGui::Text("Format: %s  |  %u x %u",
            info.format == 0 ? "Dense" : "Sparse",
            info.width, info.height);
        ImGui::Text("Palette: %u colors  |  Pixels: %u bytes",
            info.palette_size / 3, info.pixels_size);

        // Inline palette fragment — overlays the preview's base palette from
        // index 0, same as pic_decode.
        uint32_t count = info.palette_size / 3;
        if (count > 0 && (size_t)info.palette_offset + info.palette_size
                             <= ed.data.size()) {
            if (ImGui::CollapsingHeader("Inline palette")) {
                fx::Palette inlinePal = fx::pal_load(
                    ed.data.data() + info.palette_offset, info.palette_size);
                fxg::DrawPaletteSwatches("pic-inline", inlinePal, (int)count);
            }
        }
    } else if (valid) {
        ImGui::Text("Format: JPEG");
    } else {
        ImGui::TextColored({1,0.4f,0.4f,1}, "Cannot parse PIC header.");
    }

    ImGui::Separator();

    if (ImGui::Button("Export PNG...")) {
        // Decode before the dialog opens — the continuation runs frames
        // later, when the selection may have changed. Uses the preview
        // palette selection, so the export matches what the Preview shows.
        fx::Palette sysPal = fxg::ResolvePreviewPalette(app);
        auto rgba = fx::pic_decode(ed.data.data(), ed.data.size(), &sysPal);
        if (!rgba.empty() && valid) {
            platform::SaveFileDialog(
                {{"PNG image", "png;PNG"}, {"All files", "*"}}, "png", nullptr,
                [rgba = std::move(rgba), w = (int)info.width,
                 h = (int)info.height](std::string path) {
                    if (!path.empty())
                        stbi_write_png(path.c_str(), w, h, 4, rgba.data(), w * 4);
                });
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Import PNG...")) {
        platform::OpenFilesDialog(
            {{"PNG image", "png;PNG"}, {"BMP image", "bmp;BMP"}, {"All files", "*"}},
            false,
            [&app](std::vector<std::string> paths) {
                // Re-validate: the PIC editor must still be the active one.
                if (paths.empty() || app.editor.kind != EditorKind::Pic) return;
                const std::string& path = paths[0];
                int w=0,h=0,ch=0;
                uint8_t* rgba = stbi_load(path.c_str(), &w, &h, &ch, 4);
                if (!rgba) return;
                fx::Palette pal = {};
                auto encoded = fx::pic_encode(rgba, w, h, pal);
                stbi_image_free(rgba);
                if (!encoded.empty()) {
                    app.editor.data     = std::move(encoded);
                    app.editor.modified = true;
                    app.statusMsg  = "Imported " + fs::path(path).filename().string();
                    app.statusKind = App::StatusKind::Info;
                }
            });
    }

    ImGui::TextDisabled("Preview shown in the Preview panel.");
}
