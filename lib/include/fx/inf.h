#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// Parser for FA .INF aircraft tech sheet files.
// Format: plain text with dot-command directives (.body .right, .title .center, etc.)
// and a structured footer with measurement key-value pairs (LENGTH (m): 16.26, etc.).

namespace fx {

struct InfSection {
    std::string directive;  // e.g. ".body .right", ".title .center", "" for leading text
    std::string text;
};

struct InfFile {
    bool valid = false;
    std::vector<InfSection>          sections;
    std::map<std::string, std::string> stats;   // measurement footer: "LENGTH (m)" -> "16.26"
};

InfFile inf_parse(const uint8_t* data, size_t size);

} // namespace fx
