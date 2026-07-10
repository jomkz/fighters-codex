#include "fx/effect.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static void usage_effect() {
    puts("Usage:");
    puts("  fx effect types                    # list effect types -> class/shape");
    puts("  fx effect dump  <table.bin> [-n N]  # decode N 0x30-byte param records");
    puts("  fx effect spawn <record.bin>        # decode a 17-byte MSG 0x8003 record");
}

static std::vector<uint8_t> read_all(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return {};
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    std::vector<uint8_t> buf(sz > 0 ? (size_t)sz : 0);
    if (sz > 0 && fread(buf.data(), 1, (size_t)sz, f) != (size_t)sz) buf.clear();
    fclose(f);
    return buf;
}

// Highest classified effect type (dust puffs end at 0x2A).
static constexpr int EFFECT_TYPE_MAX = 0x2A;

static void print_record(const fx::EffectParams& p) {
    const char* shape = fx::effect_shape_for_type(p.type);
    printf("  type 0x%02X  %-9s  %-9s  intensity=%d frames=%d subtype=0x%04X%s "
           "debris=%d/%d sounds=%d pitch=%d\n",
           p.type, fx::effect_class_name(p.klass),
           shape[0] ? shape : "-",
           p.intensity, p.frame_count, p.subtype,
           p.ground_burst ? " ground" : "",
           p.debris_count, p.debris_spread, p.sound_variants, p.sound_pitch);
}

static int cmd_effect_types() {
    puts("Effect types (type -> class / shape; see objects.md GRAPHIC effect spawning):");
    for (int t = 0; t <= EFFECT_TYPE_MAX; t++) {
        fx::EffectClass k = fx::effect_class_for_type(t);
        const char* shape = fx::effect_shape_for_type(t);
        printf("  0x%02X  %-9s  %s\n", t, fx::effect_class_name(k),
               shape[0] ? shape : "-");
    }
    return 0;
}

static int cmd_effect_dump(const char* path, int count) {
    auto data = read_all(path);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", path); return 1; }
    if (count <= 0) count = (int)(data.size() / fx::EFFECT_RECORD_SIZE);
    auto recs = fx::effect_parse_table(data.data(), data.size(), count);
    printf("%zu effect record(s) from %s (%zu bytes):\n",
           recs.size(), path, data.size());
    for (const auto& p : recs) print_record(p);
    if ((int)recs.size() < count)
        fprintf(stderr, "note: buffer held only %zu of %d requested records\n",
                recs.size(), count);
    return 0;
}

static int cmd_effect_spawn(const char* path) {
    auto data = read_all(path);
    fx::EffectSpawn s;
    if (!fx::effect_parse_spawn(data.data(), data.size(), s)) {
        fprintf(stderr, "Not a 17-byte MSG 0x8003 spawn record: %s (%zu bytes)\n",
                path, data.size());
        return 1;
    }
    printf("spawn: type 0x%02X (%s) pos=(%.2f, %.2f, %.2f) ft owner=0x%04X "
           "flags=%02X %02X\n",
           s.type, fx::effect_class_name(fx::effect_class_for_type(s.type)),
           s.x / 256.0, s.y / 256.0, s.z / 256.0, s.owner, s.flag0, s.flag1);
    return 0;
}

int cmd_effect(int argc, char** argv) {
    if (argc < 2) { usage_effect(); return 1; }
    if (strcmp(argv[1], "types") == 0) return cmd_effect_types();
    if (strcmp(argv[1], "dump") == 0) {
        if (argc < 3) { usage_effect(); return 1; }
        int count = 0;
        for (int i = 3; i + 1 < argc; i++)
            if (strcmp(argv[i], "-n") == 0) count = atoi(argv[i + 1]);
        return cmd_effect_dump(argv[2], count);
    }
    if (strcmp(argv[1], "spawn") == 0) {
        if (argc < 3) { usage_effect(); return 1; }
        return cmd_effect_spawn(argv[2]);
    }
    fprintf(stderr, "Unknown subcommand: %s\n", argv[1]);
    usage_effect();
    return 1;
}
