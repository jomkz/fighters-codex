#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "fx/pe.h"

// DLG — menu dialog layout DLL (see DLG.md): the MZ + Phar Lap "PL"
// container family (CAM/MNU/PTS/MC/HGR). The CODE section is a dispatch
// table of variable-size control records followed by packed label strings.
//
// This is the container-level surface: validate the container, expose the
// CODE geometry, and extract the embedded label strings. The structural
// per-record decode (record types, positions, control fields) is a larger
// reverse-engineering task — the on-disk record layout differs from the
// in-memory layout DLG.md documents — and is tracked under #54.

namespace fx {

struct DlgInfo {
    bool        valid;  // MZ + PL signature with a usable CODE section
    CodeSection code;
};

DlgInfo dlg_info(const uint8_t* data, size_t size);

// Printable-ASCII runs of at least min_len characters — the dialog's
// embedded control label strings.
std::vector<std::string> dlg_strings(const uint8_t* data, size_t size,
                                     size_t min_len = 3);

} // namespace fx
