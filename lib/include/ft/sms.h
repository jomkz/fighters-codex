#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// FA.SMS symbol map parser.
//
// File layout:
//   0       4    uint32 LE: symbol count N
//   4    N*8    records: [va: u32 LE, str_off: u32 LE] x N
//   4+N*8   *    null-terminated string table (densely packed)
//
// str_off is a byte offset into the string table.
// String table base = 4 + N * 8.

namespace ft {

struct SmsSymbol {
    uint32_t    va;
    std::string name; // MSVC-mangled C++ name
};

// Parse FA.SMS. Returns empty vector on error (wrong size, truncated, etc.).
std::vector<SmsSymbol> sms_parse(const uint8_t* data, size_t size);

} // namespace ft
