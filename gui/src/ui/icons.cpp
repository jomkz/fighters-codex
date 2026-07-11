#include "ui/icons.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace fxs::icons {

platform::GpuTexture LoadIcon(Id id, int px) {
    const Coverage& c = Get(id, px);
    std::vector<std::uint8_t> rgba((std::size_t)c.size * c.size * 4);
    ToRgba(c, rgba.data());
    return platform::UploadTexture(rgba.data(), c.size, c.size);
}

} // namespace fxs::icons
