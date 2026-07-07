// Fuzz target: the AI script codec — ai_decompile (compiled BI → source text)
// and ai_compile (source text → BI). ai_decompile walks an untrusted MZ/PL BI
// container; ai_compile parses untrusted source text. Both are exercised: the
// fuzz bytes are fed to ai_decompile as a BI image and to ai_compile as source,
// and a decompiled result is recompiled (round-trip through the toolchain).

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <fx/ai.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::vector<fx::AiCompileError> errors;

    // BI → source, then recompile the recovered source (round-trip; no equality
    // assertion since arbitrary bytes aren't a fx-compiled BI).
    std::string src = fx::ai_decompile(data, size);
    if (!src.empty()) {
        std::vector<uint8_t> bi = fx::ai_compile(src, errors);
        (void)bi.size();
    }

    // The compiler's text front-end, fuzzed directly.
    errors.clear();
    std::string text((const char*)data, size);
    std::vector<uint8_t> out = fx::ai_compile(text, errors);
    (void)out.size();
    return 0;
}
