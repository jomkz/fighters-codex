#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace ft {

struct BiInstr {
    uint32_t offset;
    std::string text;
};

// Disassemble the bytecode from a BI Phar Lap PE DLL.
// Resolves CALL_DIRECT addresses to function names via the .idata section.
// Returns empty vector on failure.
std::vector<BiInstr> bi_disasm(const uint8_t* data, size_t size);

} // namespace ft
