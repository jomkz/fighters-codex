#include "fx/cfg.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static void usage_cfg() {
    puts("Usage:");
    puts("  fx cfg info <EA.CFG>            # dump the 347-byte CONFIG struct");
}

static std::vector<uint8_t> read_all(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return {};
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    std::vector<uint8_t> buf((size_t)sz);
    if (sz > 0 && fread(buf.data(), 1, (size_t)sz, f) != (size_t)sz) buf.clear();
    fclose(f);
    return buf;
}

// Raw fixed-width field -> printable C string (engine semantics: content is
// only meaningful up to the first NUL).
static std::string field_str(const uint8_t* p, size_t n) {
    size_t len = 0;
    while (len < n && p[len]) len++;
    std::string s;
    for (size_t i = 0; i < len; i++)
        s += (p[i] >= 0x20 && p[i] < 0x7F) ? (char)p[i] : '.';
    return s;
}

static int cmd_cfg_info(const char* path) {
    auto data = read_all(path);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", path); return 1; }

    fx::EaCfg c;
    if (!fx::cfg_read(data.data(), data.size(), c)) {
        fprintf(stderr,
                "Not a valid EA.CFG (need 347 bytes, magic 0x24): %s (%zu bytes)\n",
                path, data.size());
        return 1;
    }

    printf("EA.CFG (magic 0x%02X, 347 bytes)\n", c.magic);
    printf("Devices: stick=%u rudder=%u throttle=%u (100%%=%u) midi=%u\n",
           c.stick_device, c.rudder_device, c.throttle_device,
           c.throttle_100, c.midi_device);
    printf("Sound: on=%u stereo_swap=%u separation=%u\n",
           c.sound_on, c.stereo_swap, c.stereo_separation);
    printf("Volumes: overall=%u engine=%u lock=%u rwr=%u stall=%u radio=%u "
           "flight-music=%u other-music=%u\n",
           c.overall_vol, c.engine_vol, c.lock_vol, c.rwr_vol, c.stall_vol,
           c.radio_vol, c.flight_music_vol, c.other_music_vol);
    printf("Prefs: game=0x%08X multi=0x%08X debug=0x%08X hud-brightness=%u "
           "3d-glasses=%u ad-count=%u\n",
           c.game_prefs, c.game_multi_prefs, c.game_debug_prefs,
           c.hud_brightness, c.glasses3d_amount, c.ad_count);
    printf("Pilot: \"%s\"  callsign: \"%s\"  squadron: \"%s\"\n",
           field_str(c.campaign_pilot, 33).c_str(),
           field_str(c.callsign, 32).c_str(),
           field_str(c.squadron, 13).c_str());
    printf("Untraced (#54, passed through): +0x004=0x%08X +0x008=0x%08X +0x0E2=0x%02X\n",
           c.unk_004, c.unk_008, c.unk_0e2);

    auto out = fx::cfg_write(c);
    printf("Round-trip: %s\n",
           (out.size() == data.size() &&
            memcmp(out.data(), data.data(), data.size()) == 0)
               ? "byte-identical" : "MISMATCH (report this)");
    return 0;
}

int cmd_cfg(int argc, char** argv) {
    if (argc < 3) { usage_cfg(); return 1; }
    if (strcmp(argv[1], "info") == 0) return cmd_cfg_info(argv[2]);
    fprintf(stderr, "Unknown subcommand: %s\n", argv[1]);
    usage_cfg();
    return 1;
}
