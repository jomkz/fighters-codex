#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// Parser for FA .INF aircraft tech sheet files.
// Format: plain text with dot-command directives (.body .right, .title .center,
// etc.) and a structured footer with measurement key-value pairs
// (LENGTH (m): 16.26, etc.).
//
// Round-trip: sections carry their exact source bytes in raw; inf_serialize
// concatenates them, so parse -> serialize is byte-identical. Editors mutate
// the parsed views and call inf_rebuild_section to recompose raw (CRLF).

namespace fx {

struct InfSection {
    // Exact source bytes of this section: the directive line including its
    // terminator (empty for leading text), followed by the body bytes
    // verbatim. Concatenating every section's raw reproduces the file.
    std::string raw;

    // Parsed views, derived from raw.
    std::string directive;  // e.g. ".body .right"; "" for leading text
    std::string text;       // body lines ('\n'-joined, trailing whitespace trimmed)
};

struct InfFile {
    bool valid = false;
    std::vector<InfSection> sections;

    // Measurement footer as a machine-readable view ("LENGTH (m)" -> "16.26").
    // The source lines also stay in their section's raw/text, so extraction
    // does not break the round-trip.
    std::map<std::string, std::string> stats;
};

InfFile inf_parse(const uint8_t* data, size_t size);

// Concatenate every section's raw bytes back into a file image.
std::vector<uint8_t> inf_serialize(const InfFile& inf);

// Recompose a section's raw from its directive/text views, CRLF line endings.
// A trailing blank line is preserved if the previous raw ended with one (the
// corpus convention between sections), and defaulted for brand-new sections.
void inf_rebuild_section(InfSection& s);

} // namespace fx
