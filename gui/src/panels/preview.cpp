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
#include "fx/t2.h"
#include "fx_render/render.h"
#include "fx_render/gl.h"
#include "fx_render/fa_backend.h"
#include "../editors/terrain_preview.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
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
    fx_render::Mesh mesh;      // lit, untextured faces
    fx_render::Mesh mesh_tex;  // textured faces (carry u,v; shade colour as fallback)
    fx_render::Mesh grid;      // room line segments
    std::shared_ptr<fx_render::Image> tex_image;  // decoded PIC, or null
    int tex_w = 0, tex_h = 0;  // texture dims for UV normalization
    bool show_texture = true;  // texture toggle (only meaningful when tex_image)
    fx::Palette palette{};     // preview palette for untextured face colours
    bool software          = false;  // FA-faithful software backend (#290/#334)
    bool renderer_software = false;  // backend the current renderer was built as
    bool renderer_pal_dirty = false; // rebuild the fa renderer on palette change
    platform::GpuTexture sw_tex;     // display upload for the software target
    int rt_w = 0, rt_h = 0;
    int cached_lib   = -2;
    int cached_entry = -2;
    bool destroyed        = false;   // show the damaged sub-model (ShState)
    int  lod              = 0;       // JumpToLOD level (ShState); 0 = finest
    bool low_detail       = false;   // JumpToDetail preference (ShState detail=0)
    std::string wreck_name;          // non-empty: Destroyed shows this _A sibling
    bool cached_destroyed = false;
    int  frame            = 0;       // JumpToFrame animation index (ShState)
    int  cached_frame     = -1;
    int  cached_lod       = 0;
    bool cached_lowdetail = false;
    int  frame_count      = 0;       // exposed by the last parse; 0 = static
    int  lod_count        = 1;       // exposed by the last parse; 1 = no LODs
    bool has_detail       = false;   // exposed by the last parse
    int  cached_palgen    = -1;      // reload the texture when the palette changes
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
    s_sh.mesh_tex.vertices.clear();
    if (mesh.vertices.empty() || mesh.faces.empty()) return;

    // Faces split by whether they carry texture coordinates (and a texture is
    // loaded): textured faces go into mesh_tex with normalized u,v; the rest
    // stay flat-shaded in mesh. One texture per model (the common SH case).
    const bool has_tex = s_sh.tex_w > 0 && s_sh.tex_h > 0;
    const float inv_tw = has_tex ? 1.0f / s_sh.tex_w : 0.0f;
    const float inv_th = has_tex ? 1.0f / s_sh.tex_h : 0.0f;

    std::vector<fx_render::Vertex> verts;
    std::vector<fx_render::Vertex> tverts;
    verts.reserve(mesh.faces.size() * 6);

    // Fixed directional light — upper-left-front in render space (X=right, Y=up, Z=fwd)
    const float Lx = 0.577f, Ly = 0.577f, Lz = -0.577f;
    const float ambient = 0.15f, diffuse = 0.85f;

    for (const auto& face : mesh.faces) {
        if (face.indices.size() < 3) continue;
        const bool textured = has_tex && face.texcoords.size() == face.indices.size();
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

            if (textured) {
                // Fan corners 0, i, i+1 map to the parallel texcoord entries.
                // SH texel t is bottom-left origin, so flip V for the top-left
                // decoded PIC (verified against _a10.PIC — camo maps correctly).
                const auto& t0 = face.texcoords[0];
                const auto& t1 = face.texcoords[i];
                const auto& t2 = face.texcoords[i + 1];
                auto V = [&](float t){ return 1.0f - t * inv_th; };
                tverts.push_back({r0[0], r0[1], r0[2], shade, shade, shade, t0.s * inv_tw, V(t0.t)});
                tverts.push_back({r1[0], r1[1], r1[2], shade, shade, shade, t1.s * inv_tw, V(t1.t)});
                tverts.push_back({r2[0], r2[1], r2[2], shade, shade, shade, t2.s * inv_tw, V(t2.t)});
            } else {
                // Untextured faces carry a palette colour index (ShFace::color);
                // shade the model colour rather than rendering flat grey, so
                // flat-coloured surfaces (e.g. an aircraft's wings) match the
                // textured body instead of appearing white.
                float cr = s_sh.palette.r[face.color] / 255.0f * shade;
                float cg = s_sh.palette.g[face.color] / 255.0f * shade;
                float cb = s_sh.palette.b[face.color] / 255.0f * shade;
                verts.push_back({r0[0], r0[1], r0[2], cr, cg, cb, 0.0f, 0.0f});
                verts.push_back({r1[0], r1[1], r1[2], cr, cg, cb, 0.0f, 0.0f});
                verts.push_back({r2[0], r2[1], r2[2], cr, cg, cb, 0.0f, 0.0f});
            }
        }
    }

    s_sh.mesh.vertices     = std::move(verts);
    s_sh.mesh_tex.vertices = std::move(tverts);
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

// The preview palette as the fa path's 192-entry 6-bit palette: fx::Palette
// stores VGA components widened to 8-bit, so >>2 recovers them exactly.
static fx_render::fa::Palette ToFaPalette(const fx::Palette& pal) {
    fx_render::fa::Palette out;
    for (int i = 0; i < fx_render::fa::Palette::kEntries; ++i) {
        out.entries[static_cast<std::size_t>(i)] = {
            static_cast<std::uint8_t>(pal.r[i] >> 2),
            static_cast<std::uint8_t>(pal.g[i] >> 2),
            static_cast<std::uint8_t>(pal.b[i] >> 2)};
    }
    return out;
}

void PreviewForceSoftwareBackend(bool on) { s_sh.software = on; }

static void RenderSh(int w, int h) {
    if (w <= 0 || h <= 0) return;
    // Rebuild the renderer when the backend toggles (or, for the fa path,
    // when the palette it quantizes against changes).
    if (s_sh.renderer && (s_sh.renderer_software != s_sh.software ||
                          (s_sh.software && s_sh.renderer_pal_dirty))) {
        s_sh.rt.reset();
        s_sh.renderer.reset();
        s_sh.rt_w = s_sh.rt_h = 0;
    }
    if (!s_sh.renderer) {
        s_sh.renderer = s_sh.software
                            ? fx_render::MakeFaRenderer(ToFaPalette(s_sh.palette))
                            : fx_render::MakeOpenGLRenderer();
        s_sh.renderer_software = s_sh.software;
        s_sh.renderer_pal_dirty = false;
    }
    if (!s_sh.renderer) return;
    if (!s_sh.rt || s_sh.rt_w != w || s_sh.rt_h != h) {
        s_sh.rt = s_sh.renderer->MakeTarget(w, h);
        s_sh.rt_w = w;
        s_sh.rt_h = h;
    }

    // Dark background; draw only when there's a model (as the old path did).
    s_sh.renderer->Begin(*s_sh.rt, {20, 20, 31, 255});  // (0.08, 0.08, 0.12)
    if (s_sh.mesh.vertices.empty() && s_sh.mesh_tex.vertices.empty()) {
        s_sh.renderer->End();
        return;
    }

    // Bind the texture only when the toggle is on; otherwise the textured faces
    // fall back to their flat-shade colour (fx_render ignores u,v without one).
    s_sh.mesh_tex.texture = s_sh.show_texture ? s_sh.tex_image : nullptr;

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
    // Solid meshes (untextured + textured passes).
    opts.primitive = fx_render::Primitive::Triangles;
    if (!s_sh.mesh.vertices.empty())     s_sh.renderer->Draw(s_sh.mesh, cam, opts);
    if (!s_sh.mesh_tex.vertices.empty()) s_sh.renderer->Draw(s_sh.mesh_tex, cam, opts);
    // Grey wireframe overlay — depth-biased, no depth write.
    opts.wireframe = true;
    opts.depth_write = false;
    if (!s_sh.mesh.vertices.empty())     s_sh.renderer->Draw(s_sh.mesh, cam, opts);
    if (!s_sh.mesh_tex.vertices.empty()) s_sh.renderer->Draw(s_sh.mesh_tex, cam, opts);

    s_sh.renderer->End();
}

// Resolve the SH's referenced texture PIC (e.g. _a10.PIC) from the same LIB
// session, decode it against the current palette, and stash it as an
// fx_render::Image. Clears the texture when the SH is untextured, standalone,
// or the PIC is missing.
static void LoadShTexture(App& app, const EditorState& ed, const fx::ShMesh& mesh) {
    s_sh.tex_image.reset();
    s_sh.tex_w = s_sh.tex_h = 0;

    std::string tex_name;
    for (const auto& t : mesh.textures)
        if (!t.empty()) { tex_name = t; break; }
    if (tex_name.empty()) return;
    if (ed.libIdx < 0 || ed.libIdx >= (int)app.sessions.size()) return;

    const auto& sess = app.sessions[ed.libIdx];
    const fx::Entry* e = fx::ealib_find(sess.entries, tex_name);
    if (!e) return;
    auto pic = fx::ealib_extract(sess.data.data(), sess.data.size(), *e, true);
    if (pic.empty()) return;

    fx::PicInfo info;
    if (!fx::pic_info(pic.data(), pic.size(), &info)) return;
    fx::Palette sysPal = fxg::ResolvePreviewPalette(app);
    auto rgba = fx::pic_decode(pic.data(), pic.size(), &sysPal);
    if (rgba.empty()) return;

    auto img = std::make_shared<fx_render::Image>();
    img->resize((int)info.width, (int)info.height);
    std::memcpy(img->pixels.data(), rgba.data(), rgba.size());
    s_sh.tex_image = std::move(img);
    s_sh.tex_w = (int)info.width;
    s_sh.tex_h = (int)info.height;
}

// ---------------------------------------------------------------------------
// DrawPreview
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// T2 terrain 3D preview (#285). The heightfield mesh is built in the
// display-free terrain_preview core and drawn through fx_render, the same
// path the SH model preview uses. Texture tiles and the base palette live in
// sibling LIBs (tiles in FA_1, the .T2 in FA_2), so assets are resolved
// across every open session. The orbit camera here is a stand-in for the
// engine's VIEW subsystem framing (docs/fa/view.md); a shared fx_render
// camera from that RE is tracked in #387.
// ---------------------------------------------------------------------------

struct T2Preview {
    std::unique_ptr<fx_render::Renderer>     renderer;
    std::unique_ptr<fx_render::RenderTarget> rt;
    fxg::TerrainScene scene;
    fx::Palette       palette{};       // PALETTE.PAL base + default terrain band
    bool  software          = false;
    bool  renderer_software = false;
    platform::GpuTexture sw_tex;
    int   rt_w = 0, rt_h = 0;
    int   cached_lib = -2, cached_entry = -2, cached_palgen = -1;
    bool  have = false;
    int   tiles_found = 0;
    float azimuth = 40.0f, elevation = 30.0f, distance = 100.0f;
    float target[3] = {};
    float span = 1.0f;
} s_t2;

// Extract an entry by name from whichever open session holds it (tiles and
// the palette live in different LIBs from the .T2).
static std::vector<uint8_t> ExtractAcrossSessions(App& app, const std::string& name) {
    for (const auto& sess : app.sessions) {
        const fx::Entry* e = fx::ealib_find(sess.entries, name);
        if (!e) continue;
        auto d = fx::ealib_extract(sess.data.data(), sess.data.size(), *e, true);
        if (!d.empty()) return d;
    }
    return {};
}

// Base theater name from a .T2 entry name ("APA.T2" -> "APA").
static std::string TheaterStem(const std::string& entry_name) {
    std::string s = entry_name;
    auto dot = s.find_last_of('.');
    if (dot != std::string::npos) s.resize(dot);
    return s;
}

static void LoadT2Scene(App& app, const EditorState& ed) {
    s_t2.have = false;
    s_t2.scene = fxg::TerrainScene{};
    s_t2.tiles_found = 0;

    fx::T2Map map;
    if (!fx::t2_read(ed.data.data(), ed.data.size(), &map)) return;

    // Terrain palette: PALETTE.PAL base (across sessions) + the default
    // terrain band (192..255), the stand-in for the atmosphere/LAY-driven
    // band the engine fills at scene init (docs/fa/formats/T2.md).
    auto palb = ExtractAcrossSessions(app, "PALETTE.PAL");
    s_t2.palette = fx::pal_load(palb.data(), palb.size());
    fxg::FillDefaultTerrainBand(s_t2.palette);

    std::string theater;
    if (ed.entryIdx >= 0 && ed.libIdx >= 0 && ed.libIdx < (int)app.sessions.size()) {
        const auto& es = app.sessions[ed.libIdx].entries;
        if (ed.entryIdx < (int)es.size()) theater = TheaterStem(es[ed.entryIdx].name);
    }

    // Decode <theater><N>.PIC tiles on demand into fx_render images.
    std::unordered_map<uint8_t, std::shared_ptr<const fx_render::Image>> cache;
    auto provider = [&](uint8_t v) -> std::shared_ptr<const fx_render::Image> {
        auto it = cache.find(v); if (it != cache.end()) return it->second;
        std::shared_ptr<const fx_render::Image> img;
        std::string name = theater + std::to_string(v) + ".PIC";
        auto picb = ExtractAcrossSessions(app, name);
        fx::PicInfo info;
        if (!picb.empty() && fx::pic_info(picb.data(), picb.size(), &info)) {
            auto rgba = fx::pic_decode(picb.data(), picb.size(), &s_t2.palette);
            if (!rgba.empty()) {
                auto m = std::make_shared<fx_render::Image>();
                m->resize((int)info.width, (int)info.height);
                std::memcpy(m->pixels.data(), rgba.data(), rgba.size());
                img = m;
                s_t2.tiles_found++;
            }
        }
        cache.emplace(v, img); return img;
    };

    fxg::TerrainBuildOptions opts;  // height_scale 1.0 default suits the coarse bands
    s_t2.scene = fxg::BuildTerrainScene(map, provider, opts);
    s_t2.target[0] = s_t2.scene.center[0];
    s_t2.target[1] = s_t2.scene.center[1];
    s_t2.target[2] = s_t2.scene.center[2];
    s_t2.span      = s_t2.scene.span;
    s_t2.distance  = std::max(s_t2.span * 1.1f, 20.0f);
    s_t2.azimuth   = 40.0f;
    s_t2.elevation = 30.0f;
    s_t2.have = !s_t2.scene.textured.empty() || !s_t2.scene.flat.vertices.empty();
}

static void RenderT2(int w, int h) {
    if (w <= 0 || h <= 0) return;
    if (s_t2.renderer && s_t2.renderer_software != s_t2.software) {
        s_t2.rt.reset(); s_t2.renderer.reset(); s_t2.rt_w = s_t2.rt_h = 0;
    }
    if (!s_t2.renderer) {
        s_t2.renderer = s_t2.software
                            ? fx_render::MakeFaRenderer(ToFaPalette(s_t2.palette))
                            : fx_render::MakeOpenGLRenderer();
        s_t2.renderer_software = s_t2.software;
    }
    if (!s_t2.renderer) return;
    if (!s_t2.rt || s_t2.rt_w != w || s_t2.rt_h != h) {
        s_t2.rt = s_t2.renderer->MakeTarget(w, h);
        s_t2.rt_w = w; s_t2.rt_h = h;
    }

    s_t2.renderer->Begin(*s_t2.rt, {28, 34, 46, 255});
    if (!s_t2.have) { s_t2.renderer->End(); return; }

    const float kPi = 3.14159265358979f;
    float azRad = s_t2.azimuth * kPi / 180.0f, elRad = s_t2.elevation * kPi / 180.0f;
    float eye[3] = {
        s_t2.target[0] + s_t2.distance * cosf(elRad) * sinf(azRad),
        s_t2.target[1] + s_t2.distance * sinf(elRad),
        s_t2.target[2] + s_t2.distance * cosf(elRad) * cosf(azRad),
    };
    float up[3] = {0.0f, 1.0f, 0.0f};
    platform::Mat4 view = platform::mat4_look_at(eye, s_t2.target, up);
    float near_p = std::max(1.0f, s_t2.distance * 0.01f);
    float far_p  = s_t2.distance + s_t2.span * 6.0f;
    platform::Mat4 proj = platform::mat4_perspective(
        55.0f * kPi / 180.0f, (float)w / (float)h, near_p, far_p);
    platform::Mat4 mvp = platform::mat4_mul(proj, view);

    fx_render::Camera cam;
    std::memcpy(cam.mvp.data(), mvp.m, sizeof(float) * 16);
    fx_render::DrawOptions opts;
    opts.primitive = fx_render::Primitive::Triangles;
    if (!s_t2.scene.flat.vertices.empty()) s_t2.renderer->Draw(s_t2.scene.flat, cam, opts);
    for (const auto& m : s_t2.scene.textured) s_t2.renderer->Draw(m, cam, opts);
    s_t2.renderer->End();
}

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
        // Rebuild on selection change or when a state toggle (destroyed / frame) flips.
        bool sel_changed = (ed.libIdx != s_sh.cached_lib || ed.entryIdx != s_sh.cached_entry);
        if (sel_changed) { s_sh.frame = 0; s_sh.lod = 0; s_sh.low_detail = false; }
        bool pal_changed = app.palGen != s_sh.cached_palgen;
        bool dam_changed = s_sh.destroyed != s_sh.cached_destroyed;
        if (sel_changed || pal_changed || s_sh.destroyed != s_sh.cached_destroyed
                        || s_sh.frame != s_sh.cached_frame
                        || s_sh.lod != s_sh.cached_lod
                        || s_sh.low_detail != s_sh.cached_lowdetail) {
            s_sh.cached_lib       = ed.libIdx;
            s_sh.cached_entry     = ed.entryIdx;
            s_sh.cached_destroyed = s_sh.destroyed;
            s_sh.cached_frame     = s_sh.frame;
            s_sh.cached_lod       = s_sh.lod;
            s_sh.cached_lowdetail = s_sh.low_detail;
            s_sh.cached_palgen    = app.palGen;

            fx::ShInfo  info = fx::sh_parse_info(ed.data.data(), ed.data.size());
            fx::ShState st;  st.destroyed = s_sh.destroyed;  st.frame = s_sh.frame;
            st.lod    = s_sh.lod;
            st.detail = s_sh.low_detail ? 0 : 0xFFFF;
            fx::ShMesh  mesh = fx::sh_parse_mesh(ed.data.data(), ed.data.size(), st);

            // Whole-model damage swap: aircraft wrecks are separate _A.SH
            // siblings picked at render time, not inline 0xAC branches
            // (docs/fa/shape-selection.md). When Destroyed is on and the
            // shape has no inline damage, show the wreck sibling instead.
            s_sh.wreck_name.clear();
            if (s_sh.destroyed && !mesh.has_damage
                && ed.libIdx >= 0 && ed.libIdx < (int)app.sessions.size()
                && ed.entryIdx >= 0) {
                const auto& sess = app.sessions[ed.libIdx];
                if (ed.entryIdx < (int)sess.entries.size()) {
                    std::string wname =
                        fx::sh_variant_name(sess.entries[ed.entryIdx].name, 'a');
                    const fx::Entry* we =
                        wname.empty() ? nullptr : fx::ealib_find(sess.entries, wname);
                    if (we) {
                        auto wdata = fx::ealib_extract(sess.data.data(),
                                                       sess.data.size(), *we, true);
                        fx::ShState wst = st;  wst.destroyed = false;
                        fx::ShMesh  wmesh = wdata.empty()
                            ? fx::ShMesh{}
                            : fx::sh_parse_mesh(wdata.data(), wdata.size(), wst);
                        if (!wmesh.vertices.empty()) {
                            mesh = std::move(wmesh);
                            s_sh.wreck_name = wname;
                        }
                    }
                }
            }

            s_sh.frame_count = mesh.frame_count;
            s_sh.lod_count   = mesh.lod_count;
            s_sh.has_detail  = mesh.has_detail;

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
            // Destroyed can swap to the wreck sibling, which has its own PIC.
            if (sel_changed || pal_changed || dam_changed) LoadShTexture(app, ed, mesh);
            s_sh.palette = fxg::ResolvePreviewPalette(app);  // untextured face colours
            if (pal_changed) s_sh.renderer_pal_dirty = true;  // fa backend requantizes
            BuildMeshVB(mesh);
        }

        ImGui::Checkbox("Destroyed", &s_sh.destroyed);
        if (!s_sh.wreck_name.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("(wreck: %s)", s_sh.wreck_name.c_str());
        }
        ImGui::SameLine();
        ImGui::Checkbox("Software (FA)", &s_sh.software);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Render through the FA-faithful software rasteriser\n"
                              "(fx_render::fa — indexed spans, painter's order; #290)");
        }
        if (s_sh.tex_image) {
            ImGui::SameLine();
            ImGui::Checkbox("Texture", &s_sh.show_texture);
        }
        if (s_sh.frame_count > 1) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(160.0f);
            int last = s_sh.frame_count - 1;
            if (s_sh.frame > last) s_sh.frame = last;   // clamp if model changed
            ImGui::SliderInt("Frame", &s_sh.frame, 0, last);
        }
        if (s_sh.lod_count > 1) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120.0f);
            int coarsest = s_sh.lod_count - 1;
            if (s_sh.lod > coarsest) s_sh.lod = coarsest;
            ImGui::SliderInt("LOD", &s_sh.lod, 0, coarsest);
        }
        if (s_sh.has_detail) {
            ImGui::SameLine();
            ImGui::Checkbox("Low detail", &s_sh.low_detail);
        }

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
            if (ImTextureID gl_tex = (ImTextureID)(intptr_t)s_sh.rt->native_texture()) {
                // FBO content is bottom-up in GL — flip V when displaying.
                ImGui::GetWindowDrawList()->AddImage(
                    gl_tex, pos, {pos.x + canvas.x, pos.y + canvas.y},
                    ImVec2(0, 1), ImVec2(1, 0));
            } else {
                // Software target: present the indexed surface and upload it
                // for display (top-left origin, no flip).
                fx_render::Image img;
                s_sh.rt->Read(img);
                s_sh.sw_tex.Release();
                s_sh.sw_tex = platform::UploadTexture(img.pixels.data(), img.width, img.height);
                ImGui::GetWindowDrawList()->AddImage(
                    (ImTextureID)(intptr_t)s_sh.sw_tex.id,
                    pos, {pos.x + canvas.x, pos.y + canvas.y});
            }

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

    // ---- T2 terrain 3D preview ----------------------------------------------
    if (ed.kind == EditorKind::T2) {
        bool sel_changed = (ed.libIdx != s_t2.cached_lib || ed.entryIdx != s_t2.cached_entry);
        bool pal_changed = app.palGen != s_t2.cached_palgen;
        if (sel_changed || pal_changed) {
            s_t2.cached_lib    = ed.libIdx;
            s_t2.cached_entry  = ed.entryIdx;
            s_t2.cached_palgen = app.palGen;
            LoadT2Scene(app, ed);
        }

        ImGui::Checkbox("Software (FA)", &s_t2.software);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Render through the FA-faithful software rasteriser\n"
                              "(fx_render::fa — indexed spans, painter's order; #290)");
        ImGui::SameLine();
        ImGui::TextDisabled("%d tiles  |  water %d  land %d", s_t2.tiles_found,
                            s_t2.scene.water_leaves, s_t2.scene.land_leaves);

        float  txt_h  = ImGui::GetTextLineHeightWithSpacing() * 2.0f + 4.0f;
        ImVec2 canvas = ImGui::GetContentRegionAvail();
        if (canvas.x < 4) canvas.x = 4;
        canvas.y = std::max(4.0f, canvas.y - txt_h);
        int iw = (int)canvas.x, ih = (int)canvas.y;
        RenderT2(iw, ih);

        if (s_t2.rt && s_t2.have) {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##t23d", canvas);
            bool held    = ImGui::IsItemActive();
            bool hovered = ImGui::IsItemHovered();
            if (ImTextureID gl_tex = (ImTextureID)(intptr_t)s_t2.rt->native_texture()) {
                ImGui::GetWindowDrawList()->AddImage(
                    gl_tex, pos, {pos.x + canvas.x, pos.y + canvas.y},
                    ImVec2(0, 1), ImVec2(1, 0));
            } else {
                fx_render::Image img;
                s_t2.rt->Read(img);
                s_t2.sw_tex.Release();
                s_t2.sw_tex = platform::UploadTexture(img.pixels.data(), img.width, img.height);
                ImGui::GetWindowDrawList()->AddImage(
                    (ImTextureID)(intptr_t)s_t2.sw_tex.id, pos,
                    {pos.x + canvas.x, pos.y + canvas.y});
            }
            if (held) {
                ImVec2 delta = ImGui::GetIO().MouseDelta;
                s_t2.azimuth   += delta.x * 0.4f;
                s_t2.elevation -= delta.y * 0.4f;
                s_t2.elevation  = std::max(5.0f, std::min(89.0f, s_t2.elevation));
                s_t2.azimuth    = fmodf(s_t2.azimuth + 360.0f, 360.0f);
            }
            if (hovered) {
                float wheel = ImGui::GetIO().MouseWheel;
                if (wheel != 0.0f) {
                    s_t2.distance *= powf(0.85f, wheel);
                    s_t2.distance  = std::max(s_t2.span * 0.2f,
                                              std::min(s_t2.span * 3.0f, s_t2.distance));
                    ImGui::SetScrollY(0.0f);
                }
            }
            // Orbit is a stand-in for the engine's VIEW-subsystem framing;
            // the faithful camera is reproduced in fxe and consumed here (#387).
            ImGui::TextDisabled("az=%.0f el=%.0f dist=%.0f  span=%.0f",
                s_t2.azimuth, s_t2.elevation, s_t2.distance, s_t2.span);
            ImGui::TextDisabled("Drag to orbit  |  Scroll to zoom");
        } else {
            ImGui::TextDisabled("No terrain tiles found \xe2\x80\x94 also open the "
                                "theater texture LIB (e.g. FA_1.LIB).");
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
    // guarantees it). Targets before the renderer, then the image textures.
    s_sh.rt.reset();
    s_sh.renderer.reset();
    s_sh.sw_tex.Release();
    s_t2.rt.reset();
    s_t2.renderer.reset();
    s_t2.sw_tex.Release();
    s_preview.Release();
}
