#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fx {

struct AiCompileError {
    int line;
    std::string message;
};

// Compile an AI source file (plain text) to BI Phar Lap PE DLL bytes.
// Returns empty vector on failure; fills errors with diagnostics.
std::vector<uint8_t> ai_compile(const std::string& source,
                                std::vector<AiCompileError>& errors);

// Decompile a BI Phar Lap PE DLL back to AI source text (the inverse of
// ai_compile). The recovered source recompiles byte-identically to the input
// for any BI this toolchain produced — i.e. ai_compile(ai_decompile(bi)) == bi.
// Synthesizes labels (L####) from bytecode offsets; comments and the original
// identifier spelling are not recoverable. Returns "" on failure.
std::string ai_decompile(const uint8_t* data, size_t size);

} // namespace fx
