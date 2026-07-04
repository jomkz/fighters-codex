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
            // The OpenGL backend is extracted from fx-gui's preview path in
            // fx_render #289; until then callers fall back to Software.
            return nullptr;
    }
    return nullptr;
}

}  // namespace fx_render
