// fx_render — OpenGL 3.3 core backend.
//
// The GPU path, extracted from the fxs SH preview so the OpenGL and software
// renderers share one contract. Requires a current context with glad loaded
// (the host provides both). Shaders and draw state match the original preview
// so its output is unchanged.
#include <glad/gl.h>

#include <algorithm>
#include <vector>

#include "fx_render/gl.h"
#include "fx_render/render.h"

namespace fx_render {
namespace {

const char* kVS = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aCol;
layout(location = 2) in vec2 aUV;
uniform mat4 uMvp;
out vec3 vCol;
out vec2 vUV;
void main() {
    gl_Position = uMvp * vec4(aPos, 1.0);
    vCol = aCol;
    vUV = aUV;
}
)glsl";

const char* kFS = R"glsl(
#version 330 core
in vec3 vCol;
in vec2 vUV;
uniform bool uWire;
uniform bool uTextured;
uniform sampler2D uTex;
out vec4 FragColor;
void main() {
    if (uWire) {
        FragColor = vec4(0.7, 0.7, 0.7, 1.0);
    } else if (uTextured) {
        // FA skins are atlases with a transparent key (PIC index 0xFF ->
        // alpha 0). A textured face with a real flat base colour shows that
        // colour through the transparent texels; a face whose base colour is
        // index 0 (black) is a pure decal overlay — its transparent texels
        // must show the geometry behind, not a black fill, so discard them.
        vec4 t = texture(uTex, vUV);
        if (t.a < 0.5) {
            if (dot(vCol, vCol) < 0.0001) discard;   // colour-0 decal: see-through
            FragColor = vec4(vCol, 1.0);
        } else {
            FragColor = vec4(t.rgb, 1.0);
        }
    } else {
        FragColor = vec4(vCol, 1.0);
    }
}
)glsl";

GLuint Compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glDeleteShader(s);
        return 0;
    }
    return s;
}

class GlTarget final : public RenderTarget {
public:
    GlTarget(int w, int h) : w_(w), h_(h) {
        glGenTextures(1, &color_);
        glBindTexture(GL_TEXTURE_2D, color_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        glGenRenderbuffers(1, &depth_);
        glBindRenderbuffer(GL_RENDERBUFFER, depth_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        glGenFramebuffers(1, &fbo_);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    ~GlTarget() override {
        if (fbo_) glDeleteFramebuffers(1, &fbo_);
        if (color_) glDeleteTextures(1, &color_);
        if (depth_) glDeleteRenderbuffers(1, &depth_);
    }

    int width() const override { return w_; }
    int height() const override { return h_; }
    std::uintptr_t native_texture() const override { return color_; }

    void Read(Image& out) const override {
        out.resize(w_, h_);
        std::vector<std::uint8_t> flip(static_cast<std::size_t>(w_) * h_ * 4);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glReadPixels(0, 0, w_, h_, GL_RGBA, GL_UNSIGNED_BYTE, flip.data());
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        // GL is bottom-up; flip rows into the top-left-origin Image.
        const std::size_t row = static_cast<std::size_t>(w_) * 4;
        for (int y = 0; y < h_; ++y) {
            std::copy_n(&flip[static_cast<std::size_t>(h_ - 1 - y) * row], row,
                        &out.pixels[static_cast<std::size_t>(y) * row]);
        }
    }

    GLuint fbo() const { return fbo_; }

private:
    int w_, h_;
    GLuint fbo_ = 0, color_ = 0, depth_ = 0;
};

class GlRenderer final : public Renderer {
public:
    ~GlRenderer() override {
        if (prog_) glDeleteProgram(prog_);
        if (vbo_) glDeleteBuffers(1, &vbo_);
        if (vao_) glDeleteVertexArrays(1, &vao_);
        if (tex_) glDeleteTextures(1, &tex_);
    }

    std::unique_ptr<RenderTarget> MakeTarget(int w, int h) override {
        return std::make_unique<GlTarget>(w, h);
    }

    void Begin(RenderTarget& target, const std::array<std::uint8_t, 4>& clear) override {
        EnsureProgram();
        cur_ = static_cast<GlTarget*>(&target);
        glBindFramebuffer(GL_FRAMEBUFFER, cur_->fbo());
        glViewport(0, 0, cur_->width(), cur_->height());
        glClearColor(clear[0] / 255.0f, clear[1] / 255.0f, clear[2] / 255.0f, clear[3] / 255.0f);
        glClearDepth(1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void Draw(const Mesh& mesh, const Camera& cam, const DrawOptions& opts) override {
        if (!cur_ || !prog_ || mesh.vertices.empty()) return;
        EnsureBuffers();

        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(Vertex)),
                     mesh.vertices.data(), GL_STREAM_DRAW);

        glUseProgram(prog_);
        glUniformMatrix4fv(loc_mvp_, 1, GL_FALSE, cam.mvp.data());

        // Bind the mesh texture (filled triangles only); wireframe/line passes
        // ignore it. Uploaded once per source Image and cached.
        GLuint mesh_tex = 0;
        if (mesh.texture && opts.primitive == Primitive::Triangles && !opts.wireframe)
            mesh_tex = UploadTexture(*mesh.texture);
        if (mesh_tex) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mesh_tex);
            glUniform1i(loc_tex_, 0);
        }
        glUniform1i(loc_textured_, mesh_tex ? 1 : 0);

        if (opts.depth_test) {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
        } else {
            glDisable(GL_DEPTH_TEST);
        }
        glDepthMask(opts.depth_write ? GL_TRUE : GL_FALSE);
        if (opts.backface_cull) {
            glEnable(GL_CULL_FACE);
        } else {
            glDisable(GL_CULL_FACE);  // parity with the preview (cull none)
        }

        const GLsizei n = static_cast<GLsizei>(mesh.vertices.size());
        if (opts.primitive == Primitive::Lines) {
            glUniform1i(loc_wire_, 0);
            glDrawArrays(GL_LINES, 0, n);
        } else if (opts.wireframe) {
            // Grey line overlay with a depth bias — matches the preview's
            // polygon-offset wireframe pass.
            glUniform1i(loc_wire_, 1);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glEnable(GL_POLYGON_OFFSET_LINE);
            glPolygonOffset(-1.0f, -1.0f);
            glDepthFunc(GL_LEQUAL);
            glDrawArrays(GL_TRIANGLES, 0, n);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glDisable(GL_POLYGON_OFFSET_LINE);
        } else {
            glUniform1i(loc_wire_, 0);
            glDrawArrays(GL_TRIANGLES, 0, n);
        }

        glBindVertexArray(0);
    }

    void End() override {
        // Restore the state the ImGui backend expects.
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_POLYGON_OFFSET_LINE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        cur_ = nullptr;
    }

    Backend backend() const override { return Backend::OpenGL; }

private:
    void EnsureProgram() {
        if (prog_) return;
        GLuint vs = Compile(GL_VERTEX_SHADER, kVS);
        GLuint fs = Compile(GL_FRAGMENT_SHADER, kFS);
        if (!vs || !fs) {
            if (vs) glDeleteShader(vs);
            if (fs) glDeleteShader(fs);
            return;
        }
        prog_ = glCreateProgram();
        glAttachShader(prog_, vs);
        glAttachShader(prog_, fs);
        glLinkProgram(prog_);
        glDeleteShader(vs);
        glDeleteShader(fs);
        loc_mvp_ = glGetUniformLocation(prog_, "uMvp");
        loc_wire_ = glGetUniformLocation(prog_, "uWire");
        loc_textured_ = glGetUniformLocation(prog_, "uTextured");
        loc_tex_ = glGetUniformLocation(prog_, "uTex");
    }

    void EnsureBuffers() {
        if (vao_) return;
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<const void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<const void*>(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<const void*>(6 * sizeof(float)));
        glBindVertexArray(0);
    }

    // Upload `img` into a cached GL texture, re-uploading only when the source
    // pointer changes. Returns the texture id, or 0 if `img` is empty.
    GLuint UploadTexture(const Image& img) {
        if (img.width <= 0 || img.height <= 0 || img.pixels.empty()) return 0;
        if (!tex_) {
            glGenTextures(1, &tex_);
            glBindTexture(GL_TEXTURE_2D, tex_);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }
        if (&img != last_img_) {
            glBindTexture(GL_TEXTURE_2D, tex_);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, img.width, img.height, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, img.pixels.data());
            last_img_ = &img;
        }
        return tex_;
    }

    GlTarget* cur_ = nullptr;
    GLuint prog_ = 0, vao_ = 0, vbo_ = 0, tex_ = 0;
    const Image* last_img_ = nullptr;
    GLint loc_mvp_ = -1, loc_wire_ = -1, loc_textured_ = -1, loc_tex_ = -1;
};

}  // namespace

std::unique_ptr<Renderer> MakeOpenGLRenderer() {
    return std::make_unique<GlRenderer>();
}

}  // namespace fx_render
