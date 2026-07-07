// Fuzz target: the XMI (Extended MIDI) parser and XMI→SMF converter. xmi_parse
// walks the IFF/CAT chunk tree; xmi_to_smf then walks the EVNT stream of one
// sequence and emits a Standard MIDI File. Both run over untrusted bytes.

#include <cstddef>
#include <cstdint>

#include <fx/xmi.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    fx::XmiFile xf = fx::xmi_parse(data, size);
    (void)xf;
    // Convert the first couple of sequences (out-of-range indices return empty).
    for (size_t i = 0; i < 2; ++i) {
        std::vector<uint8_t> smf = fx::xmi_to_smf(data, size, i);
        (void)smf.size();
    }
    return 0;
}
