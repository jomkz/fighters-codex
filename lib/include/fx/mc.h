#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "fx/pe.h"

// MC — mission condition DLL (see MC.md): the MZ + Phar Lap "PL" container
// family (CAM/MNU/PTS). Each polls world state through the imported
// condition API (OBJAlias/Dist/MISSIONSuccess/...); the codec surfaces the
// container geometry and the embedded strings, including those imports.

namespace fx {

struct McInfo {
    bool        valid;  // MZ + PL signature with a usable CODE section
    CodeSection code;
};

McInfo mc_info(const uint8_t* data, size_t size);

std::vector<std::string> mc_strings(const uint8_t* data, size_t size,
                                    size_t min_len = 3);

} // namespace fx
