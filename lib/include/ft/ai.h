#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace ft {

struct AiCompileError {
    int line;
    std::string message;
};

// Compile an AI source file (plain text) to BI Phar Lap PE DLL bytes.
// Returns empty vector on failure; fills errors with diagnostics.
std::vector<uint8_t> ai_compile(const std::string& source,
                                std::vector<AiCompileError>& errors);

} // namespace ft
