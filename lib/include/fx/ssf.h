#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "fx/txt.h"

// SSF — EA installer script (see SSF.md): plain ASCII, '#' comments,
// all-caps keywords with comma-separated arguments. Line storage rides
// fx/txt.h (txt_read/txt_write are byte-identical), with the statements
// extracted as an overlay.

namespace fx {

struct SsfStatement {
    size_t                   line;     // index into the TxtDoc
    std::string              keyword;  // e.g. "INSTALL_FILES"
    std::vector<std::string> args;     // unquoted argument values, in order
};

struct SsfDoc {
    TxtDoc                    text;        // txt_write(text) == input bytes
    std::vector<SsfStatement> statements;
};

// Parse. Never fails; lines that are blank, comments, or don't start with
// an all-caps keyword simply produce no statement.
SsfDoc ssf_read(const uint8_t* data, size_t size);

// Serialize — byte-identical inverse of ssf_read.
std::vector<uint8_t> ssf_write(const SsfDoc& doc);

} // namespace fx
