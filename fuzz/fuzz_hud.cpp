// Fuzz target: the HUD overlay codec (hud_parse + hud_repack), the MZ/LE
// overlay parser behind `fx hud`. hud_repack carries unmodeled bytes over
// verbatim and rewrites only the mapped params, returning empty on invalid
// input. The invariant that holds for any accepted input is idempotency —
// re-packing an already-canonical file changes nothing; a mismatch is a bug.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include <fx/hud.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    fx::HudFile hud = fx::hud_parse(data, size);
    std::vector<uint8_t> once = fx::hud_repack(data, size, hud);
    if (once.empty()) return 0;  // parser rejected or params out of range
    fx::HudFile hud2 = fx::hud_parse(once.data(), once.size());
    std::vector<uint8_t> twice = fx::hud_repack(once.data(), once.size(), hud2);
    if (once != twice) abort();  // repack is not a fixed point of parse
    return 0;
}
