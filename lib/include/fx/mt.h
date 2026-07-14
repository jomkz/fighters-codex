#pragma once
#include <string>

#include "fx/txt.h"

// MT — mission briefing text (see MT.md): the same directive engine as
// .TXT, parsed by fx/txt.h (txt_read/txt_write round-trip byte-identically).
// This header adds the MT-specific section semantics: section 1 carries the
// mission identifier line, the title, and the mission type; sections 2–5 are
// the briefing and the three debrief outcomes.
//
// The identifier line is shaped `<ID>  (<annotation>)`. Any leading dashes are
// DECORATION -- 100 shipped files write `--AB01`, 244 write a bare `AB01`, and one
// writes `-RB12`. Requiring the `--` (which MT.md called, and hedged as, "the engine's
// cue (inferred)") lost the ID on 263 of the 363 files and shifted the other fields up
// by one. The engine settles nothing here: it never parses this line, it RENDERS it.

namespace fx {

struct MtInfo {
    std::string mission_id;   // "AB01" from the "--AB01  (bextra01)" line
    // The parenthesised note. Usually the file's own name, but the designers were not
    // consistent: ~FANOTH.MT writes a theater ("Panama") and EXAMPLE.MT names a DIFFERENT
    // file ("extra01"). It is an annotation, not a key -- do not resolve anything with it.
    std::string source_name;
    std::string title;        // section-1 line after the identifier
    std::string mission_type; // section-1 line after the title
    size_t      sections = 0; // count of .section directives
};

// Extract the MT header facts from a parsed document. Fields that cannot
// be located stay empty; sections is always the .section count.
MtInfo mt_info(const TxtDoc& doc);

} // namespace fx
