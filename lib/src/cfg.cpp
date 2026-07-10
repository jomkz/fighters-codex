#include "fx/cfg.h"
#include <cstring>

namespace fx {

static uint16_t r16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}
static uint32_t r32(const uint8_t* p) {
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) |
                      ((uint32_t)p[3] << 24));
}
static void w16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
}
static void w32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v;         p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

bool cfg_read(const uint8_t* d, size_t size, EaCfg& c) {
    if (size != EA_CFG_SIZE) return false;
    c.magic = r32(d + 0x000);
    if (c.magic != EA_CFG_MAGIC) return false;

    c.menu_video_mode         = r32(d + 0x004);
    c.flight_video_mode         = r32(d + 0x008);
    c.stick_device    = r32(d + 0x00C);
    c.rudder_device   = r32(d + 0x010);
    c.throttle_device = r32(d + 0x014);
    c.throttle_100    = r32(d + 0x018);
    for (int i = 0; i < 48; i++) c.axis_map[i] = r32(d + 0x01C + i * 4);
    memcpy(c.window_types, d + 0x0DC, 6);
    c.music_on           = d[0x0E2];
    c.sound_on          = d[0x0E3];
    c.stereo_swap       = d[0x0E4];
    c.overall_vol       = r16(d + 0x0E5);
    c.engine_vol        = r16(d + 0x0E7);
    c.lock_vol          = r16(d + 0x0E9);
    c.rwr_vol           = r16(d + 0x0EB);
    c.stall_vol         = r16(d + 0x0ED);
    c.radio_vol         = r16(d + 0x0EF);
    c.flight_music_vol  = r16(d + 0x0F1);
    c.other_music_vol   = r16(d + 0x0F3);
    c.stereo_separation = r16(d + 0x0F5);
    c.midi_device       = d[0x0F7];
    c.game_prefs        = r32(d + 0x0F8);
    c.game_multi_prefs  = r32(d + 0x0FC);
    c.game_debug_prefs  = r32(d + 0x100);
    c.hud_brightness    = r32(d + 0x104);
    memcpy(c.campaign_pilot, d + 0x108, 33);
    memcpy(c.callsign,       d + 0x129, 32);
    memcpy(c.squadron,       d + 0x149, 13);
    c.glasses3d_amount = r32(d + 0x156);
    c.ad_count         = d[0x15A];
    return true;
}

std::vector<uint8_t> cfg_write(const EaCfg& c) {
    std::vector<uint8_t> out(EA_CFG_SIZE, 0);
    uint8_t* d = out.data();
    w32(d + 0x000, c.magic);
    w32(d + 0x004, c.menu_video_mode);
    w32(d + 0x008, c.flight_video_mode);
    w32(d + 0x00C, c.stick_device);
    w32(d + 0x010, c.rudder_device);
    w32(d + 0x014, c.throttle_device);
    w32(d + 0x018, c.throttle_100);
    for (int i = 0; i < 48; i++) w32(d + 0x01C + i * 4, c.axis_map[i]);
    memcpy(d + 0x0DC, c.window_types, 6);
    d[0x0E2] = c.music_on;
    d[0x0E3] = c.sound_on;
    d[0x0E4] = c.stereo_swap;
    w16(d + 0x0E5, c.overall_vol);
    w16(d + 0x0E7, c.engine_vol);
    w16(d + 0x0E9, c.lock_vol);
    w16(d + 0x0EB, c.rwr_vol);
    w16(d + 0x0ED, c.stall_vol);
    w16(d + 0x0EF, c.radio_vol);
    w16(d + 0x0F1, c.flight_music_vol);
    w16(d + 0x0F3, c.other_music_vol);
    w16(d + 0x0F5, c.stereo_separation);
    d[0x0F7] = c.midi_device;
    w32(d + 0x0F8, c.game_prefs);
    w32(d + 0x0FC, c.game_multi_prefs);
    w32(d + 0x100, c.game_debug_prefs);
    w32(d + 0x104, c.hud_brightness);
    memcpy(d + 0x108, c.campaign_pilot, 33);
    memcpy(d + 0x129, c.callsign, 32);
    memcpy(d + 0x149, c.squadron, 13);
    w32(d + 0x156, c.glasses3d_amount);
    d[0x15A] = c.ad_count;
    return out;
}

} // namespace fx
