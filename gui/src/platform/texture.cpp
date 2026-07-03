#include "texture.h"
#include <glad/gl.h>

namespace platform {

void GpuTexture::Release() {
    if (id) {
        GLuint name = id;
        glDeleteTextures(1, &name);
        id = 0;
    }
    width = height = 0;
}

GpuTexture UploadTexture(const uint8_t* rgba, int w, int h) {
    GpuTexture t;
    if (!rgba || w <= 0 || h <= 0) return t;

    GLint prev = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev);

    GLuint name = 0;
    glGenTextures(1, &name);
    glBindTexture(GL_TEXTURE_2D, name);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glBindTexture(GL_TEXTURE_2D, (GLuint)prev);

    t.id     = name;
    t.width  = w;
    t.height = h;
    return t;
}

} // namespace platform
