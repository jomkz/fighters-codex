#pragma once
#include "assets/icons_baked.h"
#include "platform/texture.h"

namespace fxs::icons {

// Upload the baked variant nearest `px` as an RGBA texture (white RGB,
// coverage in alpha) for ImGui — draw it tinted with the theme foreground so
// one baked artifact serves both light and dark. Needs a live GL context;
// returns a zero-id texture on failure. The category browsers (#364) own the
// per-DPI sizing and the tint colour.
platform::GpuTexture LoadIcon(Id id, int px);

} // namespace fxs::icons
