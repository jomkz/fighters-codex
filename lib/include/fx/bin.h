#pragma once
#include <cstddef>
#include <string>

// BIN — flat lookup tables in FA_2.LIB (see BIN.md). The bytes carry no
// self-describing structure; the entry name identifies which table it is.

namespace fx {

enum class BinKind {
    Insigmap,  // 256 B  — insignia slot map (0x3B = "no insignia" sentinel)
    Mix2,      // 512 B  — half-intensity blend table, gamma-corrected
    Mix2L,     // 512 B  — half-intensity blend table, linear (i/2)
    Mix4,      // 1024 B — quarter-intensity blend table, gamma-corrected
    Mix4L,     // 1024 B — quarter-intensity blend table, linear (i/4)
    VFontPal,  // 48 B   — 16-entry VGA 6-bit palette for video briefing text
    Unknown,
};

// Classify by entry name (case-insensitive; with or without the .BIN
// extension). Unknown names classify as BinKind::Unknown.
BinKind bin_classify(const std::string& entry_name);

// One-line human-readable description of the table (never null).
const char* bin_kind_desc(BinKind kind);

// Documented size of the table in bytes; 0 for Unknown.
size_t bin_expected_size(BinKind kind);

} // namespace fx
