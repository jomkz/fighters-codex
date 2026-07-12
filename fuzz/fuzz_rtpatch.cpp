// Fuzz target: RTPatch container parsing, 0xB59C decompression, and §9 opcode
// application over untrusted bytes — the exact path `fx patch apply` runs.

#include <cstddef>
#include <cstdint>
#include <vector>

#include <fx/rtpatch.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Container walk + per-record reconstruct. The input doubles as the (bogus)
    // source for MODIFY records; verify=false forces the apply path to run.
    fx::RtpPatch p = fx::rtp_read(data, size);
    for (const auto& rec : p.records) {
        if (rec.dst_size > (4u << 20)) continue;   // keep the fuzzer's outputs small
        fx::rtp_reconstruct(data, size, rec, data, size, /*verify=*/false);
    }

    // Direct codec + opcode paths, independent of the container walk.
    if (size > 4) {
        auto out = fx::rtp_decompress(data, size, 0, 1u << 16);
        if (!out.empty())
            fx::rtp_apply(data, size, out.data(), out.size(), out.size(), 1);
    }
    fx::rtp_checksum(data, size, 31);
    fx::rtp_checksum(data, size, 30);
    return 0;
}
