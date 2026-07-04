// fx_render — OpenGL backend factory.
//
// Separate header (and separate build target) because the GL backend needs a
// current OpenGL 3.3 core context and the glad loader, which the dependency-free
// core deliberately does not. The host (fx-gui, fxc) creates the context and
// loads glad before calling this.
#pragma once

#include <memory>

#include "fx_render/render.h"

namespace fx_render {

// Construct the OpenGL renderer. A current GL 3.3 core context with glad loaded
// must exist on the calling thread. Targets are framebuffer objects;
// RenderTarget::native_texture() returns the colour attachment for zero-copy
// display (e.g. as an ImGui image).
std::unique_ptr<Renderer> MakeOpenGLRenderer();

}  // namespace fx_render
