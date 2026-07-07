// Fuzz target: the mission/map file parser (mission_parse_info) and round-trip
// (mission_roundtrip, a verbatim re-emit with CRLF normalization). Byte-identity
// holds only for already-canonical input, so — as with SEQ — the invariant that
// holds for *any* input is idempotency: a second round trip changes nothing.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include <fx/mission.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    fx::MissionInfo info = fx::mission_parse_info(data, size);
    (void)info.map_file.size();  // touch a decoded field

    std::vector<uint8_t> once = fx::mission_roundtrip(data, size);
    std::vector<uint8_t> twice = fx::mission_roundtrip(once.data(), once.size());
    if (once != twice) abort();  // round-trip is not a fixed point
    return 0;
}
