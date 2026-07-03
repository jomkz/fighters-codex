// The single translation unit carrying the glad GL 3.3 core loader
// implementation. Never combine glad with the ImGui backend's private
// embedded loader (imgui_impl_opengl3_loader.h) in one TU.
#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>
