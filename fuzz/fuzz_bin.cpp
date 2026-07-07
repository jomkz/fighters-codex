// Fuzz target: the BIN lookup-table identification helpers. bin_classify maps an
// entry name to a BinKind; bin_kind_desc / bin_expected_size read it back. The
// fuzz bytes stand in for an (untrusted) entry name string.

#include <cstddef>
#include <cstdint>
#include <string>

#include <fx/bin.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::string name((const char*)data, size);
    fx::BinKind kind = fx::bin_classify(name);
    volatile size_t sink = fx::bin_expected_size(kind);
    sink ^= (size_t)fx::bin_kind_desc(kind)[0];
    (void)sink;
    return 0;
}
