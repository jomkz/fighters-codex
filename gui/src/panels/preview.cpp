#include "preview.h"
#include "../app.h"
#include "../platform/texture.h"
#include "imgui.h"
#include "fx/pic.h"
#include "fx/pal.h"
#include "fx/ealib.h"
#include "fx/raw.h"
#include "fx/sh.h"

#include <algorithm>
#include <cmath>
#include <vector>

// ---------------------------------------------------------------------------
// Image preview (PIC / RAW)
// ---------------------------------------------------------------------------

static GpuTexture s_preview;
static int        s_previewLib   = -2;
static int        s_previewEntry = -2;

// Locate PALETTE.PAL across all open sessions. Returns a loaded Palette,
// or a greyscale fallback if not found.
static fx::Palette FindSysPalette(const App& app) {
    for (const auto& sess : app.sessions) {
        if (const fx::Entry* entry = fx::ealib_find(sess.entries, "PALETTE.PAL")) {
            auto raw = fx::ealib_extract(sess.data.data(), sess.data.size(),
                                         *entry);
            if (!raw.empty())
                return fx::pal_load(raw.data(), raw.size());
        }
    }
    return fx::pal_load(nullptr, 0); // greyscale fallback
}

// ---------------------------------------------------------------------------
// DrawPreview
// ---------------------------------------------------------------------------

void DrawPreview(App& app) {

    const auto& ed = app.editor;

    // ---- Image preview (PIC / RAW) -----------------------------------------
    if (ed.libIdx != s_previewLib || ed.entryIdx != s_previewEntry) {
        s_preview.Release();
        s_previewLib   = ed.libIdx;
        s_previewEntry = ed.entryIdx;

        if (!ed.data.empty() && ed.kind != EditorKind::Sh) {
            if (ed.kind == EditorKind::Pic) {
                fx::PicInfo info;
                if (fx::pic_info(ed.data.data(), ed.data.size(), &info)) {
                    fx::Palette sysPal = FindSysPalette(app);
                    auto rgba = fx::pic_decode(ed.data.data(), ed.data.size(), &sysPal);
                    if (!rgba.empty())
                        s_preview = platform::UploadTexture(rgba.data(),
                                                            (int)info.width, (int)info.height);
                }
            } else if (ed.kind == EditorKind::Raw) {
                fx::RawInfo info;
                if (fx::raw_info(ed.data.data(), ed.data.size(), &info)) {
                    auto rgba = fx::raw_decode(ed.data.data(), ed.data.size());
                    if (!rgba.empty())
                        s_preview = platform::UploadTexture(rgba.data(),
                                                            (int)info.width, (int)info.height);
                }
            }
        }
    }

    // ---- SH 3D preview -------------------------------------------------------
    if (ed.kind == EditorKind::Sh) {
        // The GL FBO pipeline lands with the SH preview port (#86).
        ImGui::TextDisabled("3D preview port in progress.");
        return;
    }

    // ---- Static image preview -----------------------------------------------
    if (s_preview.id) {
        float avail = ImGui::GetContentRegionAvail().x;
        float scale = avail / (float)s_preview.width;
        float dispH = s_preview.height * scale;
        ImGui::Image((ImTextureID)(intptr_t)s_preview.id, ImVec2(avail, dispH));
        ImGui::TextDisabled("%dx%d", s_preview.width, s_preview.height);
    } else if (ed.kind != EditorKind::None) {
        ImGui::TextDisabled("No preview for .%s", ed.ext.c_str());
    } else {
        ImGui::TextDisabled("No record selected.");
    }
}
