// Fuzz target: the SMS debugger-symbol table parser (fx sms over untrusted
// bytes). sms_parse reads a u32 count then that many VA + null-terminated-name
// records; a hostile count/offset is the surface under test.

#include <cstddef>
#include <cstdint>
#include <vector>

#include <fx/sms.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::vector<fx::SmsSymbol> syms = fx::sms_parse(data, size);
    volatile uint32_t sink = 0;
    for (const auto& s : syms) sink ^= s.va ^ (uint32_t)s.name.size();
    (void)sink;
    return 0;
}
