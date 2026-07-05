#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

#include "fx/pe.h"

// PTS — aircraft screen assets DLL (see PTS.md): the MZ + Phar Lap "PL"
// container family (like CAM/MNU). Each file references exactly one PIC —
// the aircraft's hangar/selection icon.

namespace fx {

struct PtsInfo {
    bool        valid;  // MZ + PL signature with a usable CODE section
    CodeSection code;
    std::string icon;   // the referenced ICON*.PIC name; empty if none found
};

PtsInfo pts_info(const uint8_t* data, size_t size);

} // namespace fx
