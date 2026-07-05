#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "fx/pe.h"

// CAM — campaign definition DLL (see CAM.md): MZ + Phar Lap "PL\0\0" image
// with CODE/.idata/.reloc sections. The campaign configuration is embedded
// in the CODE section as flat null-terminated string tables (mission list,
// aircraft types, weapon pool, state keys).

namespace fx {

struct CamInfo {
    bool        valid;  // MZ + PL signature with a usable CODE section
    CodeSection code;   // geometry of the CODE section (see pe.h)
};

// Validate the container and locate the CODE section.
CamInfo cam_info(const uint8_t* data, size_t size);

// Printable-ASCII runs of at least min_len characters — the campaign's
// embedded string tables read exactly the way the GUI viewer shows them.
std::vector<std::string> cam_strings(const uint8_t* data, size_t size,
                                     size_t min_len = 3);

} // namespace fx
