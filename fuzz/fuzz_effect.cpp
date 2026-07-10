// Fuzz target: the GRAPHIC effect-data interpreter (read-only). No accepted
// input should crash the record/table/spawn parsers or read out of bounds
// (ASan-checked). There is no round-trip to assert — the invariant is simply
// that arbitrary bytes parse without a fault.

#include <cstddef>
#include <cstdint>
#include <vector>

#include <fx/effect.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // The first byte steers how many records the table walk is asked for, so
    // the fuzzer exercises both exhaustion and in-bounds paths.
    int count = size ? data[0] : 0;
    fx::effect_parse_table(data, size, count);

    // Every classified type index, against a buffer of any length.
    for (int t = 0; t <= 0x2B; t++) {
        fx::EffectParams p;
        fx::effect_parse_record(data, size, t, p);
        fx::effect_shape_for_type(t);
        fx::effect_class_name(fx::effect_class_for_type(t));
    }

    fx::EffectSpawn s;
    fx::effect_parse_spawn(data, size, s);
    return 0;
}
