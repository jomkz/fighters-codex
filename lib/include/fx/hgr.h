#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "fx/pe.h"

// HGR — hangar screen DLL (see HGR.md): the MZ + Phar Lap "PL" container
// family. The confirmed content is the container itself plus the PIC asset
// references (hangar background, selection icons); the slot-table
// sub-resource layout is only partially mapped (gap #54), so decoding it
// waits for the sub-resource base to be pinned.

namespace fx {

struct HgrInfo {
    bool                     valid;  // MZ + PL signature with a CODE section
    CodeSection              code;
    std::vector<std::string> pics;   // referenced *.PIC assets, in order
};

HgrInfo hgr_info(const uint8_t* data, size_t size);

std::vector<std::string> hgr_strings(const uint8_t* data, size_t size,
                                     size_t min_len = 3);

} // namespace fx
