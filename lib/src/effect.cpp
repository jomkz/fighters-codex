#include "fx/effect.h"

namespace fx {

static uint16_t r16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}
static int16_t s16(const uint8_t* p) {
    return (int16_t)r16(p);
}
static uint32_t r32(const uint8_t* p) {
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) |
                      ((uint32_t)p[3] << 24));
}
static int32_t s32(const uint8_t* p) {
    return (int32_t)r32(p);
}

EffectClass effect_class_for_type(int type) {
    if (type == 0)                       return EffectClass::None;
    if (type >= 1  && type <= 3)         return EffectClass::Crater;
    if (type >= 4  && type <= 6)         return EffectClass::Debris;
    if (type >= 7  && type <= 11)        return EffectClass::Smoke;
    if (type == 12)                      return EffectClass::Chaff;
    if (type == 13)                      return EffectClass::Flare;
    if (type == 14)                      return EffectClass::Fire;
    if (type >= 15 && type <= 0x26)      return EffectClass::Explosion;
    if (type >= 0x28 && type <= 0x2A)    return EffectClass::DustPuff;
    return EffectClass::Unknown;         // e.g. 0x27, or out of range
}

const char* effect_class_name(EffectClass klass) {
    switch (klass) {
        case EffectClass::None:      return "none";
        case EffectClass::Crater:    return "crater";
        case EffectClass::Debris:    return "debris";
        case EffectClass::Smoke:     return "smoke";
        case EffectClass::Chaff:     return "chaff";
        case EffectClass::Flare:     return "flare";
        case EffectClass::Fire:      return "fire";
        case EffectClass::Explosion: return "explosion";
        case EffectClass::DustPuff:  return "dust-puff";
        case EffectClass::Unknown:   break;
    }
    return "unknown";
}

const char* effect_shape_for_type(int type) {
    switch (effect_class_for_type(type)) {
        case EffectClass::Crater:    return "crater.SH";
        case EffectClass::Debris:    return "debris.SH";
        case EffectClass::Smoke:     return "smoke.SH";
        case EffectClass::Chaff:     return "chaff.SH";
        case EffectClass::Flare:     return "flare.SH";
        case EffectClass::Fire:      return "fire.SH";
        case EffectClass::Explosion: return "exp.SH";
        case EffectClass::DustPuff:
            // 0x28/0x29/0x2A = small/medium/large powder-dust puff.
            return type == 0x28 ? "spd.SH" : type == 0x29 ? "mpd.SH" : "lpd.SH";
        case EffectClass::None:
        case EffectClass::Unknown:   break;
    }
    return "";
}

bool effect_parse_record(const uint8_t* data, size_t size, int type, EffectParams& out) {
    if (type < 0) return false;
    size_t base = (size_t)type * EFFECT_RECORD_SIZE;
    // Need the whole 0x30-byte record for this type.
    if (base > size || size - base < EFFECT_RECORD_SIZE) return false;
    const uint8_t* r = data + base;

    out = EffectParams{};
    out.type          = type;
    out.klass         = effect_class_for_type(type);
    out.intensity     = s16(r + 0x04);
    out.frame_count   = s16(r + 0x06);
    out.subtype       = r16(r + 0x08);
    out.ground_burst  = (r[0x08] & 0x04) != 0;
    out.debris_count  = s16(r + 0x0A);
    out.debris_spread = s16(r + 0x0C);
    out.sound_pitch   = s16(r + 0x2E);

    // Sound-name pointer list at +0x0E: up to 8 slots, terminated by the first
    // null (matching the engine's own scan in _GRAPHICAddExp@28).
    out.sound_variants = 0;
    for (int i = 0; i < 8; i++) {
        uint32_t p = r32(r + 0x0E + i * 4);
        out.sound_ptrs[i] = p;
        if (p == 0) break;
        out.sound_variants++;
    }
    return true;
}

std::vector<EffectParams> effect_parse_table(const uint8_t* data, size_t size, int count) {
    std::vector<EffectParams> out;
    if (count < 0) return out;
    out.reserve((size_t)count);
    for (int t = 0; t < count; t++) {
        EffectParams p;
        if (!effect_parse_record(data, size, t, p)) break;  // buffer exhausted
        out.push_back(p);
    }
    return out;
}

bool effect_parse_spawn(const uint8_t* data, size_t size, EffectSpawn& out) {
    if (size < EFFECT_SPAWN_SIZE) return false;
    out.type  = data[0x00];
    out.x     = s32(data + 0x01);
    out.y     = s32(data + 0x05);
    out.z     = s32(data + 0x09);
    out.owner = r16(data + 0x0D);
    out.flag0 = data[0x0F];
    out.flag1 = data[0x10];
    return true;
}

} // namespace fx
