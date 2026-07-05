#pragma once
#include <cstddef>
#include <cstring>
#include <string>

// Small portable string helpers shared across fxs sources. Case folding is
// ASCII-only by design — entry names and extensions are 8.3 ASCII per the LIB
// format, and locale-dependent folding (tolower) would misbehave for bytes
// >= 0x80 in e.g. a Turkish locale.
namespace fxg {

inline char ascii_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c;
}

// Case-insensitive equality of two whole strings.
inline bool ci_equal(const char* a, const char* b) {
    for (; *a && *b; ++a, ++b)
        if (ascii_lower(*a) != ascii_lower(*b)) return false;
    return *a == '\0' && *b == '\0';
}

// Case-insensitive "hay starts with needle".
inline bool ci_prefix(const char* hay, const char* needle) {
    for (; *needle; ++hay, ++needle)
        if (ascii_lower(*hay) != ascii_lower(*needle)) return false;
    return true;
}

// Case-insensitive substring search. An empty needle matches everything.
inline bool ci_contains(const char* hay, const char* needle) {
    if (!needle || needle[0] == '\0') return true;
    for (; *hay; ++hay)
        if (ci_prefix(hay, needle)) return true;
    return false;
}

// Bounded copy into a fixed char buffer: truncates to cap-1 and always
// NUL-terminates (cap must be >= 1).
inline void copy_str(char* dst, size_t cap, const std::string& src) {
    if (cap == 0) return;
    size_t n = src.size() < cap - 1 ? src.size() : cap - 1;
    std::memcpy(dst, src.data(), n);
    dst[n] = '\0';
}

} // namespace fxg
