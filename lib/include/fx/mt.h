#pragma once
#include <string>

#include "fx/txt.h"

// MT — mission briefing text (see MT.md): the same directive engine as
// .TXT, parsed by fx/txt.h (txt_read/txt_write round-trip byte-identically).
// This header adds the MT-specific section semantics: section 1 carries the
// mission identifier line ("--<ID>  (<name>)"), title, and mission type;
// sections 2–5 are briefing and the three debrief outcomes.

namespace fx {

struct MtInfo {
    std::string mission_id;   // "AB01" from the "--AB01  (bextra01)" line
    std::string source_name;  // "bextra01" from the parenthesized part
    std::string title;        // section-1 line after the identifier
    std::string mission_type; // section-1 line after the title
    size_t      sections = 0; // count of .section directives
};

// Extract the MT header facts from a parsed document. Fields that cannot
// be located stay empty; sections is always the .section count.
MtInfo mt_info(const TxtDoc& doc);

} // namespace fx
