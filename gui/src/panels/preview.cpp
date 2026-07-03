#include "preview.h"
#include "../app.h"
#include "../platform/math3d.h"
#include "../platform/texture.h"
#include "imgui.h"
#include "fx/pic.h"
#include "fx/pal.h"
#include "fx/ealib.h"
#include "fx/raw.h"
#include "fx/sh.h"

#include <glad/gl.h>
#include <algorithm>
#include <cmath>
#include <cstring>
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
// SH 3D preview — offscreen GL 3.3 FBO pipeline
// ---------------------------------------------------------------------------

struct Vtx3D { float x, y, z, r, g, b; };

static const char* kVS = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aCol;
uniform mat4 uMvp;
out vec3 vCol;
void main() {
    gl_Position = uMvp * vec4(aPos, 1.0);
    vCol = aCol;
}
)glsl";

static const char* kFS = R"glsl(
#version 330 core
in vec3 vCol;
uniform bool uWire;
out vec4 FragColor;
void main() {
    FragColor = uWire ? vec4(0.7, 0.7, 0.7, 1.0) : vec4(vCol, 1.0);
}
)glsl";

struct ShPreview {
    GLuint fbo      = 0;
    GLuint colorTex = 0;
    GLuint depthRbo = 0;
    GLuint prog     = 0;
    GLuint vaoMesh  = 0, vboMesh = 0;
    GLuint vaoGrid  = 0, vboGrid = 0;
    GLint  locMvp   = -1;
    GLint  locWire  = -1;
    int vtx_count      = 0;
    int grid_vtx_count = 0;
    int rt_w = 0, rt_h = 0;
    int cached_lib   = -2;
    int cached_entry = -2;
    float azimuth    = 170.0f;
    float elevation  = 20.0f;
    float distance   = 100.0f;
    float model_span = 1.0f;
    float target[3]  = {};
    bool  prog_ok    = false;

    void ReleaseRT() {
        if (fbo)      { glDeleteFramebuffers(1, &fbo);   fbo = 0; }
        if (colorTex) { glDeleteTextures(1, &colorTex);  colorTex = 0; }
        if (depthRbo) { glDeleteRenderbuffers(1, &depthRbo); depthRbo = 0; }
        rt_w = rt_h = 0;
    }
    void ReleaseMesh() {
        if (vaoMesh) { glDeleteVertexArrays(1, &vaoMesh); vaoMesh = 0; }
        if (vboMesh) { glDeleteBuffers(1, &vboMesh);      vboMesh = 0; }
        if (vaoGrid) { glDeleteVertexArrays(1, &vaoGrid); vaoGrid = 0; }
        if (vboGrid) { glDeleteBuffers(1, &vboGrid);      vboGrid = 0; }
        vtx_count = grid_vtx_count = 0;
    }
} s_sh;

static GLuint CompileShader(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    GLint ok = GL_FALSE;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) { glDeleteShader(sh); return 0; }
    return sh;
}

static bool EnsureProgram() {
    if (s_sh.prog_ok) return true;

    GLuint vs = CompileShader(GL_VERTEX_SHADER, kVS);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFS);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }
    s_sh.prog = glCreateProgram();
    glAttachShader(s_sh.prog, vs);
    glAttachShader(s_sh.prog, fs);
    glLinkProgram(s_sh.prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint linked = GL_FALSE;
    glGetProgramiv(s_sh.prog, GL_LINK_STATUS, &linked);
    if (!linked) {
        glDeleteProgram(s_sh.prog);
        s_sh.prog = 0;
        return false;
    }
    s_sh.locMvp  = glGetUniformLocation(s_sh.prog, "uMvp");
    s_sh.locWire = glGetUniformLocation(s_sh.prog, "uWire");
    s_sh.prog_ok = true;
    return true;
}

static bool EnsureRT(int w, int h) {
    if (s_sh.rt_w == w && s_sh.rt_h == h && s_sh.fbo) return true;
    s_sh.ReleaseRT();
    if (w <= 0 || h <= 0) return false;

    glGenTextures(1, &s_sh.colorTex);
    glBindTexture(GL_TEXTURE_2D, s_sh.colorTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenRenderbuffers(1, &s_sh.depthRbo);
    glBindRenderbuffer(GL_RENDERBUFFER, s_sh.depthRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glGenFramebuffers(1, &s_sh.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, s_sh.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, s_sh.colorTex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, s_sh.depthRbo);
    bool complete =
        glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!complete) { s_sh.ReleaseRT(); return false; }

    s_sh.rt_w = w;
    s_sh.rt_h = h;
    return true;
}

// Upload interleaved pos+colour vertices into a fresh VAO/VBO pair.
static void UploadVertexBuffer(GLuint* vao, GLuint* vbo,
                               const std::vector<Vtx3D>& verts) {
    glGenVertexArrays(1, vao);
    glGenBuffers(1, vbo);
    glBindVertexArray(*vao);
    glBindBuffer(GL_ARRAY_BUFFER, *vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(verts.size() * sizeof(Vtx3D)),
                 verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx3D),
                          (const void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx3D),
                          (const void*)(3 * sizeof(float)));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void BuildMeshVB(const fx::ShMesh& mesh) {
    s_sh.ReleaseMesh();
    if (mesh.vertices.empty() || mesh.faces.empty()) return;

    std::vector<Vtx3D> verts;
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

            // Compute face normal in render space (SH Y/Z swapped: render Y=SH Z, render Z=SH Y)
            float ax = v1.x - v0.x, ay = v1.z - v0.z, az = v1.y - v0.y;
            float bx = v2.x - v0.x, by = v2.z - v0.z, bz = v2.y - v0.y;
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

            // SH uses Z-up (X=right, Y=forward, Z=up); remap to render Y-up
            verts.push_back({v0.x, v0.z, v0.y, shade, shade, shade});
            verts.push_back({v1.x, v1.z, v1.y, shade, shade, shade});
            verts.push_back({v2.x, v2.z, v2.y, shade, shade, shade});
        }
    }

    if (verts.empty()) return;
    UploadVertexBuffer(&s_sh.vaoMesh, &s_sh.vboMesh, verts);
    s_sh.vtx_count = (int)verts.size();
}

// Build a 6-wall room around the model.
// Room is a cube of half-size = max_span*2, centred on the model bbox centre.
// Camera orbits at <= max_span*1.6, so it is always INSIDE the room.
// All coordinates are in render space: X=FA_X, Y=FA_Z(up), Z=FA_Y(fwd).
static void BuildBoxGridVB(const fx::ShInfo& info, float max_span) {
    float cx = (info.bbox[0] + info.bbox[3]) * 0.5f;
    float cy = (info.bbox[2] + info.bbox[5]) * 0.5f; // render_Y = FA_Z centre
    float cz = (info.bbox[1] + info.bbox[4]) * 0.5f; // render_Z = FA_Y centre

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

    std::vector<Vtx3D> lines;

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

    if (lines.empty()) return;
    UploadVertexBuffer(&s_sh.vaoGrid, &s_sh.vboGrid, lines);
    s_sh.grid_vtx_count = (int)lines.size();
}

static void RenderSh(int w, int h) {
    if (!EnsureRT(w, h))   return;
    if (!EnsureProgram())  return;

    glBindFramebuffer(GL_FRAMEBUFFER, s_sh.fbo);
    glViewport(0, 0, w, h);
    glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!s_sh.vaoMesh || s_sh.vtx_count == 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return; // empty model — just show dark bg
    }

    // Build MVP. Same orbit math as the DX11 build; the RH look_at may
    // mirror the model relative to the old LH view — verify side-by-side on
    // the Windows bench and negate the azimuth here if it does.
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

    glUseProgram(s_sh.prog);
    glUniformMatrix4fv(s_sh.locMvp, 1, GL_FALSE, mvp.m);
    glUniform1i(s_sh.locWire, 0);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE); // parity with D3D11_CULL_NONE

    // Room grid first so model depth writes occlude it naturally
    if (s_sh.vaoGrid && s_sh.grid_vtx_count > 0) {
        glBindVertexArray(s_sh.vaoGrid);
        glDrawArrays(GL_LINES, 0, s_sh.grid_vtx_count);
    }

    glBindVertexArray(s_sh.vaoMesh);
    glDrawArrays(GL_TRIANGLES, 0, s_sh.vtx_count);

    // Wireframe overlay — grey lines on top of the solid mesh (the DX build
    // used a depth-bias rasterizer state; polygon offset is the GL analogue)
    glUniform1i(s_sh.locWire, 1);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glEnable(GL_POLYGON_OFFSET_LINE);
    glPolygonOffset(-1.0f, -1.0f);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
    glDrawArrays(GL_TRIANGLES, 0, s_sh.vtx_count);

    // Restore state for the ImGui backend
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_POLYGON_OFFSET_LINE);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
        // Rebuild mesh buffers when selection changes
        if (ed.libIdx != s_sh.cached_lib || ed.entryIdx != s_sh.cached_entry) {
            s_sh.cached_lib   = ed.libIdx;
            s_sh.cached_entry = ed.entryIdx;

            fx::ShInfo info = fx::sh_parse_info(ed.data.data(), ed.data.size());
            fx::ShMesh mesh = fx::sh_parse_mesh(ed.data.data(), ed.data.size());

            // Reset camera to fit the model.
            // SH bbox is (X, Y, Z) = (right, forward, up); remap target to render Y-up.
            s_sh.target[0] = (info.bbox[0] + info.bbox[3]) * 0.5f; // X unchanged
            s_sh.target[1] = (info.bbox[2] + info.bbox[5]) * 0.5f; // render Y = model Z (up)
            s_sh.target[2] = (info.bbox[1] + info.bbox[4]) * 0.5f; // render Z = model Y (fwd)
            float spanX = info.bbox[3] - info.bbox[0];
            float spanY = info.bbox[4] - info.bbox[1];
            float spanZ = info.bbox[5] - info.bbox[2];
            float maxSpan = std::max(spanX, std::max(spanY, spanZ));
            s_sh.model_span = std::max(maxSpan, 1.0f);
            // Camera INSIDE the room (room half-size = 2*max_span, orbit = 1*max_span)
            s_sh.distance   = std::max(maxSpan * 1.0f, 20.0f);
            s_sh.azimuth    = 45.0f;
            s_sh.elevation  = 25.0f;

            BuildMeshVB(mesh);
            BuildBoxGridVB(info, s_sh.model_span);
        }

        float  txt_h   = ImGui::GetTextLineHeightWithSpacing() * 2.0f + 4.0f;
        ImVec2 canvas  = ImGui::GetContentRegionAvail();
        if (canvas.x < 4) canvas.x = 4;
        canvas.y = std::max(4.0f, canvas.y - txt_h);

        int iw = (int)canvas.x, ih = (int)canvas.y;
        RenderSh(iw, ih);

        if (s_sh.fbo && s_sh.vtx_count > 0) {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##sh3d", canvas);
            bool held    = ImGui::IsItemActive();
            bool hovered = ImGui::IsItemHovered();
            // FBO content is bottom-up in GL — flip V when displaying.
            ImGui::GetWindowDrawList()->AddImage(
                (ImTextureID)(intptr_t)s_sh.colorTex,
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
