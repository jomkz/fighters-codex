#include "sh_scene.h"
#include "../platform/math3d.h"
#include "fx_render/fa_backend.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace fxg {

void BuildShMeshes(const fx::ShMesh& mesh, const fx::Palette& pal,
                   int tex_w, int tex_h,
                   fx_render::Mesh& flat, fx_render::Mesh& textured) {
    flat.vertices.clear();
    textured.vertices.clear();
    if (mesh.vertices.empty() || mesh.faces.empty()) return;

    // Faces split by whether they carry texture coordinates (and a texture is
    // available): textured faces get normalized u,v; the rest stay flat-shaded.
    const bool has_tex = tex_w > 0 && tex_h > 0;
    const float inv_tw = has_tex ? 1.0f / tex_w : 0.0f;
    const float inv_th = has_tex ? 1.0f / tex_h : 0.0f;

    std::vector<fx_render::Vertex> verts;
    std::vector<fx_render::Vertex> tverts;
    verts.reserve(mesh.faces.size() * 6);

    // FA shape faces carry a *pre-shaded* palette-ramp colour index: the model
    // tool bakes the sun/orientation shading into `ShFace::color` (e.g. the F16
    // walks the grey ramp 145..160 face by face). FA renders those colours
    // directly — painter's order, no runtime per-face relighting — so this
    // path must not re-light them (a dynamic lambert here double-shaded the
    // already-dark ramp entries to near-black). See docs/fa/render-core.md.
    for (const auto& face : mesh.faces) {
        if (face.indices.size() < 3) continue;
        const bool tex = has_tex && face.texcoords.size() == face.indices.size();
        for (size_t i = 1; i + 1 < face.indices.size(); i++) {
            uint32_t i0 = face.indices[0];
            uint32_t i1 = face.indices[i];
            uint32_t i2 = face.indices[i + 1];
            // Skip the whole triangle rather than pushing partial vertices
            // (which misaligns the buffer).
            if (i0 >= mesh.vertices.size() || i1 >= mesh.vertices.size() ||
                i2 >= mesh.vertices.size())
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

            // Every face carries a flat base colour (ShFace::color, pre-shaded
            // above). FA skins are texture atlases where palette index 0xFF is
            // transparent (PIC.md); the engine shows the face's flat colour
            // through those texels. So carry the base colour on the vertices
            // even for textured faces — the renderer falls back to it where
            // the texel is transparent, instead of drawing the atlas's black
            // background.
            float cr = pal.r[face.color] / 255.0f;
            float cg = pal.g[face.color] / 255.0f;
            float cb = pal.b[face.color] / 255.0f;
            if (tex) {
                // Fan corners 0, i, i+1 map to the parallel texcoord entries.
                // SH texel t is bottom-left origin, so flip V for the top-left
                // decoded PIC (verified against _a10.PIC — camo maps correctly).
                const auto& t0 = face.texcoords[0];
                const auto& t1 = face.texcoords[i];
                const auto& t2 = face.texcoords[i + 1];
                auto V = [&](float t) { return 1.0f - t * inv_th; };
                tverts.push_back({r0[0], r0[1], r0[2], cr, cg, cb, t0.s * inv_tw, V(t0.t)});
                tverts.push_back({r1[0], r1[1], r1[2], cr, cg, cb, t1.s * inv_tw, V(t1.t)});
                tverts.push_back({r2[0], r2[1], r2[2], cr, cg, cb, t2.s * inv_tw, V(t2.t)});
            } else {
                // Untextured faces render the shaded base colour directly.
                verts.push_back({r0[0], r0[1], r0[2], cr, cg, cb, 0.0f, 0.0f});
                verts.push_back({r1[0], r1[1], r1[2], cr, cg, cb, 0.0f, 0.0f});
                verts.push_back({r2[0], r2[1], r2[2], cr, cg, cb, 0.0f, 0.0f});
            }
        }
    }

    flat.vertices     = std::move(verts);
    textured.vertices = std::move(tverts);
}

fx_render::fa::Palette ToFaPalette(const fx::Palette& pal) {
    fx_render::fa::Palette out;
    for (int i = 0; i < fx_render::fa::Palette::kEntries; ++i) {
        out.entries[static_cast<std::size_t>(i)] = {
            static_cast<std::uint8_t>(pal.r[i] >> 2),
            static_cast<std::uint8_t>(pal.g[i] >> 2),
            static_cast<std::uint8_t>(pal.b[i] >> 2)};
    }
    return out;
}

fx_render::Image RenderShThumbnail(const uint8_t* sh, size_t size,
                                   const fx::Palette& pal,
                                   std::shared_ptr<const fx_render::Image> texture,
                                   int px) {
    fx_render::Image out;
    if (!sh || size == 0 || px <= 0) return out;

    fx::ShInfo info = fx::sh_parse_info(sh, size);
    fx::ShMesh mesh = fx::sh_parse_mesh(sh, size);  // merged default state

    fx_render::Mesh flat, textured;
    const int tw = texture ? texture->width : 0;
    const int th = texture ? texture->height : 0;
    BuildShMeshes(mesh, pal, tw, th, flat, textured);
    if (flat.vertices.empty() && textured.vertices.empty()) return out;
    textured.texture = std::move(texture);

    // The preview panel's default three-quarter framing: orbit centre at the
    // bbox centre, distance from the largest extent, azimuth 170 / elevation
    // 20 — thumbnails match what the viewer first shows.
    float shCenter[3] = {
        (info.bbox[0] + info.bbox[3]) * 0.5f,
        (info.bbox[1] + info.bbox[4]) * 0.5f,
        (info.bbox[2] + info.bbox[5]) * 0.5f,
    };
    float target[3];
    platform::sh_to_render(shCenter, target);
    float spanX = info.bbox[3] - info.bbox[0];
    float spanY = info.bbox[4] - info.bbox[1];
    float spanZ = info.bbox[5] - info.bbox[2];
    float span  = std::max(std::max(spanX, spanY), std::max(spanZ, 1.0f));
    float dist  = std::max(span * 1.0f, 20.0f);

    const float kPi = 3.14159265358979f;
    float azRad = 170.0f * kPi / 180.0f;
    float elRad = 20.0f * kPi / 180.0f;
    float eye[3] = {
        target[0] + dist * std::cos(elRad) * std::sin(azRad),
        target[1] + dist * std::sin(elRad),
        target[2] + dist * std::cos(elRad) * std::cos(azRad),
    };
    float up[3] = {0.0f, 1.0f, 0.0f};
    platform::Mat4 view = platform::mat4_look_at(eye, target, up);
    float near_p = std::max(1.0f, dist * 0.01f);
    float far_p  = dist + span * 10.0f;
    platform::Mat4 proj =
        platform::mat4_perspective(60.0f * kPi / 180.0f, 1.0f, near_p, far_p);
    platform::Mat4 mvp = platform::mat4_mul(proj, view);

    fx_render::Camera cam;
    std::memcpy(cam.mvp.data(), mvp.m, sizeof(float) * 16);

    auto renderer = fx_render::MakeFaRenderer(ToFaPalette(pal));
    if (!renderer) return out;
    auto rt = renderer->MakeTarget(px, px);
    renderer->Begin(*rt, {20, 20, 31, 255});  // the preview's dark backdrop
    fx_render::DrawOptions opts;
    opts.primitive = fx_render::Primitive::Triangles;
    if (!flat.vertices.empty())     renderer->Draw(flat, cam, opts);
    if (!textured.vertices.empty()) renderer->Draw(textured, cam, opts);
    renderer->End();
    rt->Read(out);
    return out;
}

}  // namespace fxg
