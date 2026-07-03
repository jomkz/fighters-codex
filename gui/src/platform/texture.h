#pragma once
#include <cstdint>

namespace platform {

// Opaque GPU texture handle for image display in ImGui. The id is a GL
// texture name stored as unsigned so this header needs no GL declarations;
// display sites cast it to ImTextureID.
struct GpuTexture {
    unsigned id     = 0;
    int      width  = 0;
    int      height = 0;
    void Release();
};

// Upload RGBA8 pixels to a new texture (linear filtering, clamped edges).
// Returns a zero-id texture on failure.
GpuTexture UploadTexture(const uint8_t* rgba, int w, int h);

} // namespace platform
