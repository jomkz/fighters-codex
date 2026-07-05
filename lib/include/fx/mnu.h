#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "fx/pe.h"

// MNU — menu screen layout DLL (see MNU.md): the same MZ + Phar Lap "PL"
// container family as CAM, importing the _Draw* renderers from the game
// executable. The menu-tree/control encoding inside CODE is still an open
// gap (#54), so the codec surfaces the container geometry and the embedded
// label strings — the parts the spec has confirmed.

namespace fx {

struct MnuInfo {
    bool        valid;  // MZ + PL signature with a usable CODE section
    CodeSection code;
};

MnuInfo mnu_info(const uint8_t* data, size_t size);

// Printable-ASCII runs of at least min_len characters — the embedded menu
// label strings.
std::vector<std::string> mnu_strings(const uint8_t* data, size_t size,
                                     size_t min_len = 3);

} // namespace fx
