// fx_render — Image helpers and the backend factory.
#include "fx_render/render.h"

namespace fx_render {

void Image::resize(int w, int h) {
    width = w > 0 ? w : 0;
    height = h > 0 ? h : 0;
    pixels.assign(static_cast<std::size_t>(width) * height * 4, 0);
}

std::uint8_t* Image::at(int x, int y) {
    return &pixels[(static_cast<std::size_t>(y) * width + x) * 4];
}

const std::uint8_t* Image::at(int x, int y) const {
    return &pixels[(static_cast<std::size_t>(y) * width + x) * 4];
}

// Defined in software.cpp.
std::unique_ptr<Renderer> MakeSoftwareRenderer();

std::unique_ptr<Renderer> MakeRenderer(Backend backend) {
    switch (backend) {
        case Backend::Software:
            return MakeSoftwareRenderer();
        case Backend::OpenGL:
            // The OpenGL backend needs a current GL context and glad, so it is
            // constructed via fx_render/gl.h (MakeOpenGLRenderer) rather than
            // this dependency-free factory.
            return nullptr;
    }
    return nullptr;
}

void RenderToImage(Renderer& r, const Mesh& mesh, const Camera& cam, Image& out,
                   const std::array<std::uint8_t, 4>& clear, const DrawOptions& opts) {
    auto target = r.MakeTarget(out.width > 0 ? out.width : 1,
                               out.height > 0 ? out.height : 1);
    r.Begin(*target, clear);
    r.Draw(mesh, cam, opts);
    r.End();
    target->Read(out);
}

}  // namespace fx_render
