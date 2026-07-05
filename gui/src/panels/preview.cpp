#include "preview.h"
#include "../app.h"
#include "../palettes.h"
#include "../platform/math3d.h"
#include "../platform/texture.h"
#include "imgui.h"
#include "fx/pic.h"
#include "fx/pal.h"
#include "fx/ealib.h"
#include "fx/raw.h"
#include "fx/sh.h"
#include "fx_render/render.h"
#include "fx_render/gl.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

// ---------------------------------------------------------------------------
// Image preview (PIC / RAW)
// ---------------------------------------------------------------------------

static GpuTexture s_preview;
static int        s_previewLib    = -2;
static int        s_previewEntry  = -2;
static int        s_previewPalGen = -1;

// ---------------------------------------------------------------------------
// SH 3D preview — geometry is built here and rendered through the shared
// fx_render module (OpenGL backend); see render/ and fx_render #281.
// ---------------------------------------------------------------------------

struct ShPreview {
    std::unique_ptr<fx_render::Renderer>     renderer;
    std::unique_ptr<fx_render::RenderTarget> rt;
    fx_render::Mesh mesh;   // lit triangles
    fx_render::Mesh grid;   // room line segments
    int rt_w = 0, rt_h = 0;
    int cached_lib   = -2;
    int cached_entry = -2;
    bool destroyed        = false;   // show the damaged sub-model (ShState)
    bool cached_destroyed = false;
    float azimuth    = 170.0f;
    float elevation  = 20.0f;
    float distance   = 100.0f;
    float model_span = 1.0f;
    float target[3]  = {};   // orbit centre, render Y-up space
} s_sh;

// Triangulate + flat-light the SH mesh into render-space triangles for
// fx_render (Vertex = interleaved position + colour, same layout).
static void BuildMeshVB(const fx::ShMesh& mesh) {
    s_sh.mesh.vertices.clear();
    if (mesh.vertices.empty() || mesh.faces.empty()) return;

    std::vector<fx_render::Vertex> verts;
    verts.reserve(mesh.faces.size() * 6);

    // Fixed directional light — upper-left-front in render space (X=right, Y=up, Z=fwd)
    const float Lx = 0.577f, Ly = 0.577f, Lz = -0.577f;
    const float ambient = 0.15f, diffuse = 0.85f;

    for (const auto& face : mesh.faces) {
        if (face.indices.size() < 3) continue;
        for (size_t i = 1; i + 1 < face.indices.size(); i++) {
            uint32_t i0 = face.indices[0];
            uint32_t i1 = face.indices[i];
            uint32_t i2 = face.indices[i + 1];
            // Skip entire triangle rather than pushing partial vertices (which misaligns the buffer)
            if (i0 >= mesh.vertices.size() || i1 >= mesh.vertices.size() || i2 >= mesh.vertices.size())
                continue;
            auto& v0 = mesh.vertices[i0];
            auto& v1 = mesh.vertices[i1];
            auto& v2 = mesh.vertices[i2];

            // Remap SH (Z-up, right-handed) into render Y-up space with a
            // proper rotation; a plain Y/Z swap here mirrored the model.
            float r0[3], r1[3], r2[3];
            const float p0[3] = {v0.x, v0.y, v0.z};
            const float p1[3] = {v1.x, v1.y, v1.z};
            const float p2[3] = {v2.x, v2.y, v2.z};
            platform::sh_to_render(p0, r0);
            platform::sh_to_render(p1, r1);
            platform::sh_to_render(p2, r2);

            // Face normal in render space (from the remapped positions).
            float ax = r1[0]-r0[0], ay = r1[1]-r0[1], az = r1[2]-r0[2];
            float bx = r2[0]-r0[0], by = r2[1]-r0[1], bz = r2[2]-r0[2];
            float nx = ay * bz - az * by;
            float ny = az * bx - ax * bz;
            float nz = ax * by - ay * bx;
            float len = sqrtf(nx*nx + ny*ny + nz*nz);
            float shade;
            if (len > 0.0f) {
                nx /= len; ny /= len; nz /= len;
                float diff = nx * Lx + ny * Ly + nz * Lz;
                shade = ambient + diffuse * std::max(0.0f, diff);
            } else {
                shade = ambient + diffuse * 0.3f;
            }

            verts.push_back({r0[0], r0[1], r0[2], shade, shade, shade});
            verts.push_back({r1[0], r1[1], r1[2], shade, shade, shade});
            verts.push_back({r2[0], r2[1], r2[2], shade, shade, shade});
        }
    }

    s_sh.mesh.vertices = std::move(verts);
}

// Build a 6-wall room around the model.
// Room is a cube of half-size = max_span*2, centred on the model bbox centre.
// Camera orbits at <= max_span*1.6, so it is always INSIDE the room.
// All coordinates are in render space via sh_to_render: X=FA_X, Y=FA_Z(up),
// Z=-FA_Y(forward maps to -Z).
static void BuildBoxGridVB(const fx::ShInfo& info, float max_span) {
    // Room center = model bbox center, mapped SH -> render Y-up.
    float shCenter[3] = {
        (info.bbox[0] + info.bbox[3]) * 0.5f,
        (info.bbox[1] + info.bbox[4]) * 0.5f,
        (info.bbox[2] + info.bbox[5]) * 0.5f,
    };
    float rc[3];
    platform::sh_to_render(shCenter, rc);
    float cx = rc[0], cy = rc[1], cz = rc[2];

    float half = max_span * 2.0f;
    float xlo = cx - half,  xhi = cx + half;
    float ylo = cy - half,  yhi = cy + half;
    float zlo = cz - half,  zhi = cz + half;

    // Step: ~8 divisions across the room so lines aren't too dense or sparse
    float raw  = (half * 2.0f) / 8.0f;
    float mag  = powf(10.0f, floorf(log10f(std::max(raw, 0.1f))));
    float step = ceilf(raw / mag) * mag;
    if (step < 1.0f) step = 1.0f;

    // Snap bounds to grid so corners land on lines
    xlo = floorf(xlo/step)*step;  xhi = ceilf(xhi/step)*step;
    ylo = floorf(ylo/step)*step;  yhi = ceilf(yhi/step)*step;
    zlo = floorf(zlo/step)*step;  zhi = ceilf(zhi/step)*step;

    std::vector<fx_render::Vertex> lines;

    // Grid on one XZ face (constant Y)
    auto faceXZ = [&](float y, float c) {
        for (float x = xlo; x <= xhi+0.001f; x += step) {
            lines.push_back({x,y,zlo, c,c,c}); lines.push_back({x,y,zhi, c,c,c});
        }
        for (float z = zlo; z <= zhi+0.001f; z += step) {
            lines.push_back({xlo,y,z, c,c,c}); lines.push_back({xhi,y,z, c,c,c});
        }
    };
    // Grid on one YZ face (constant X)
    auto faceYZ = [&](float x, float c) {
        for (float y = ylo; y <= yhi+0.001f; y += step) {
            lines.push_back({x,y,zlo, c,c,c}); lines.push_back({x,y,zhi, c,c,c});
        }
        for (float z = zlo; z <= zhi+0.001f; z += step) {
            lines.push_back({x,ylo,z, c,c,c}); lines.push_back({x,yhi,z, c,c,c});
        }
    };
    // Grid on one XY face (constant Z)
    auto faceXY = [&](float z, float c) {
        for (float x = xlo; x <= xhi+0.001f; x += step) {
            lines.push_back({x,ylo,z, c,c,c}); lines.push_back({x,yhi,z, c,c,c});
        }
        for (float y = ylo; y <= yhi+0.001f; y += step) {
            lines.push_back({xlo,y,z, c,c,c}); lines.push_back({xhi,y,z, c,c,c});
        }
    };

    faceXZ(ylo, 0.22f);   // floor  — slightly brighter
    faceXZ(yhi, 0.14f);   // ceiling — dimmer
    faceYZ(xlo, 0.17f);   // left wall
    faceYZ(xhi, 0.17f);   // right wall
    faceXY(zlo, 0.17f);   // rear wall
    faceXY(zhi, 0.17f);   // front wall

    // 12 bright box edges so the outline is crisp
    const float ce = 0.45f;
    auto edge = [&](float ax,float ay,float az, float bx,float by,float bz){
        lines.push_back({ax,ay,az,ce,ce,ce}); lines.push_back({bx,by,bz,ce,ce,ce});
    };
    edge(xlo,ylo,zlo, xhi,ylo,zlo); edge(xhi,ylo,zlo, xhi,ylo,zhi);
    edge(xhi,ylo,zhi, xlo,ylo,zhi); edge(xlo,ylo,zhi, xlo,ylo,zlo);
    edge(xlo,yhi,zlo, xhi,yhi,zlo); edge(xhi,yhi,zlo, xhi,yhi,zhi);
    edge(xhi,yhi,zhi, xlo,yhi,zhi); edge(xlo,yhi,zhi, xlo,yhi,zlo);
    edge(xlo,ylo,zlo, xlo,yhi,zlo); edge(xhi,ylo,zlo, xhi,yhi,zlo);
    edge(xhi,ylo,zhi, xhi,yhi,zhi); edge(xlo,ylo,zhi, xlo,yhi,zhi);

    s_sh.grid.vertices = std::move(lines);
}

static void RenderSh(int w, int h) {
    if (w <= 0 || h <= 0) return;
    if (!s_sh.renderer) s_sh.renderer = fx_render::MakeOpenGLRenderer();
    if (!s_sh.renderer) return;
    if (!s_sh.rt || s_sh.rt_w != w || s_sh.rt_h != h) {
        s_sh.rt = s_sh.renderer->MakeTarget(w, h);
        s_sh.rt_w = w;
        s_sh.rt_h = h;
    }

    // Dark background; draw only when there's a model (as the old path did).
    s_sh.renderer->Begin(*s_sh.rt, {20, 20, 31, 255});  // (0.08, 0.08, 0.12)
    if (s_sh.mesh.vertices.empty()) {
        s_sh.renderer->End();
        return;
    }

    // Orbit camera in the renderer's right-handed Y-up space; the SH mesh is
    // mapped there by platform::sh_to_render, so look_at/perspective apply as-is.
    const float kPi = 3.14159265358979f;
    float azRad = s_sh.azimuth   * kPi / 180.0f;
    float elRad = s_sh.elevation * kPi / 180.0f;
    float eye[3] = {
        s_sh.target[0] + s_sh.distance * cosf(elRad) * sinf(azRad),
        s_sh.target[1] + s_sh.distance * sinf(elRad),
        s_sh.target[2] + s_sh.distance * cosf(elRad) * cosf(azRad),
    };
    float up[3] = {0.0f, 1.0f, 0.0f};
    platform::Mat4 view = platform::mat4_look_at(eye, s_sh.target, up);
    float near_p = std::max(1.0f, s_sh.distance * 0.01f);
    float far_p  = s_sh.distance + s_sh.model_span * 10.0f;
    platform::Mat4 proj = platform::mat4_perspective(
        60.0f * kPi / 180.0f, (float)w / (float)h, near_p, far_p);
    platform::Mat4 mvp = platform::mat4_mul(proj, view);

    fx_render::Camera cam;
    std::memcpy(cam.mvp.data(), mvp.m, sizeof(float) * 16);

    fx_render::DrawOptions opts;
    // Room grid first so the model's depth writes occlude it naturally.
    opts.primitive = fx_render::Primitive::Lines;
    if (!s_sh.grid.vertices.empty()) s_sh.renderer->Draw(s_sh.grid, cam, opts);
    // Solid mesh.
    opts.primitive = fx_render::Primitive::Triangles;
    s_sh.renderer->Draw(s_sh.mesh, cam, opts);
    // Grey wireframe overlay — depth-biased, no depth write.
    opts.wireframe = true;
    opts.depth_write = false;
    s_sh.renderer->Draw(s_sh.mesh, cam, opts);

    s_sh.renderer->End();
}

// ---------------------------------------------------------------------------
// DrawPreview
// ---------------------------------------------------------------------------

void DrawPreview(App& app) {

    const auto& ed = app.editor;

    // ---- Image preview (PIC / RAW) -----------------------------------------
    if (ed.libIdx != s_previewLib || ed.entryIdx != s_previewEntry ||
        app.palGen != s_previewPalGen) {
        s_preview.Release();
        s_previewLib    = ed.libIdx;
        s_previewEntry  = ed.entryIdx;
        s_previewPalGen = app.palGen;

        if (!ed.data.empty() && ed.kind != EditorKind::Sh) {
            if (ed.kind == EditorKind::Pic) {
                fx::PicInfo info;
                if (fx::pic_info(ed.data.data(), ed.data.size(), &info)) {
                    fx::Palette sysPal = fxg::ResolvePreviewPalette(app);
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
        // Rebuild on selection change or when a state toggle (destroyed) flips.
        bool sel_changed = (ed.libIdx != s_sh.cached_lib || ed.entryIdx != s_sh.cached_entry);
        if (sel_changed || s_sh.destroyed != s_sh.cached_destroyed) {
            s_sh.cached_lib       = ed.libIdx;
            s_sh.cached_entry     = ed.entryIdx;
            s_sh.cached_destroyed = s_sh.destroyed;

            fx::ShInfo  info = fx::sh_parse_info(ed.data.data(), ed.data.size());
            fx::ShState st;  st.destroyed = s_sh.destroyed;
            fx::ShMesh  mesh = fx::sh_parse_mesh(ed.data.data(), ed.data.size(), st);

            if (sel_changed) {
                // Reset camera to fit the model (only on selection change). SH
                // bbox is (X, Y, Z) = (right, forward, up); center -> render Y-up.
                float shCenter[3] = {
                    (info.bbox[0] + info.bbox[3]) * 0.5f,
                    (info.bbox[1] + info.bbox[4]) * 0.5f,
                    (info.bbox[2] + info.bbox[5]) * 0.5f,
                };
                platform::sh_to_render(shCenter, s_sh.target);
                float spanX = info.bbox[3] - info.bbox[0];
                float spanY = info.bbox[4] - info.bbox[1];
                float spanZ = info.bbox[5] - info.bbox[2];
                float maxSpan = std::max(spanX, std::max(spanY, spanZ));
                s_sh.model_span = std::max(maxSpan, 1.0f);
                s_sh.distance   = std::max(maxSpan * 1.0f, 20.0f);
                s_sh.azimuth    = 45.0f;
                s_sh.elevation  = 25.0f;
                BuildBoxGridVB(info, s_sh.model_span);
            }
            BuildMeshVB(mesh);
        }

        ImGui::Checkbox("Destroyed", &s_sh.destroyed);

        float  txt_h   = ImGui::GetTextLineHeightWithSpacing() * 2.0f + 4.0f;
        ImVec2 canvas  = ImGui::GetContentRegionAvail();
        if (canvas.x < 4) canvas.x = 4;
        canvas.y = std::max(4.0f, canvas.y - txt_h);

        int iw = (int)canvas.x, ih = (int)canvas.y;
        RenderSh(iw, ih);

        if (s_sh.rt && !s_sh.mesh.vertices.empty()) {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##sh3d", canvas);
            bool held    = ImGui::IsItemActive();
            bool hovered = ImGui::IsItemHovered();
            // FBO content is bottom-up in GL — flip V when displaying.
            ImGui::GetWindowDrawList()->AddImage(
                (ImTextureID)(intptr_t)s_sh.rt->native_texture(),
                pos, {pos.x + canvas.x, pos.y + canvas.y},
                ImVec2(0, 1), ImVec2(1, 0));

            if (held) {
                ImVec2 delta = ImGui::GetIO().MouseDelta;
                s_sh.azimuth   += delta.x * 0.4f;
                s_sh.elevation -= delta.y * 0.4f;
                s_sh.elevation  = std::max(-89.0f, std::min(89.0f, s_sh.elevation));
                s_sh.azimuth    = fmodf(s_sh.azimuth + 360.0f, 360.0f);
            }
            if (hovered) {
                float wheel = ImGui::GetIO().MouseWheel;
                if (wheel != 0.0f) {
                    s_sh.distance *= powf(0.85f, wheel);
                    float min_d = std::max(s_sh.model_span * 0.8f, 10.0f);
                    float max_d = s_sh.model_span * 1.6f;  // keep camera inside the room
                    s_sh.distance  = std::max(min_d, std::min(max_d, s_sh.distance));
                    ImGui::SetScrollY(0.0f);
                }
            }
            ImGui::TextDisabled("az=%.0f el=%.0f dist=%.0f  tgt=(%.1f,%.1f,%.1f)  span=%.0f",
                s_sh.azimuth, s_sh.elevation, s_sh.distance,
                s_sh.target[0], s_sh.target[1], s_sh.target[2], s_sh.model_span);
            ImGui::TextDisabled("Drag to orbit  |  Scroll to zoom");
        } else {
            ImGui::TextDisabled("x86-only geometry \xe2\x80\x94 no 3D preview.");
        }
        return;
    }

    // ---- Static image preview -----------------------------------------------
    if (s_preview.id) {
        fx::PicInfo pi;
        if (ed.kind == EditorKind::Pic &&
            fx::pic_info(ed.data.data(), ed.data.size(), &pi) &&
            pi.format != 0xD8FF) { // JPEG PICs ignore the palette
            // Switching re-decodes on the next frame; the inline palette
            // fragment (if any) still overlays the chosen base palette.
            ImGui::TextUnformatted("Palette:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-FLT_MIN);
            fxg::DrawPaletteCombo(app, "Auto (PALETTE.PAL)");
        }
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

void PreviewShutdown() {
    // Order matters only in that both need the live GL context (the caller
    // guarantees it). Targets before the renderer, then the image texture.
    s_sh.rt.reset();
    s_sh.renderer.reset();
    s_preview.Release();
}
