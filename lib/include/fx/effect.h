#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

// EFFECT — GRAPHIC effect-spawn data (see EFFECT.md, and objects.md
// § "GRAPHIC effect spawning"). This is a read-only *interpreter*, not an
// on-disk codec: the game executable carries a fixed table of per-type
// effect-tuning records (the 0x30-byte record at FA.EXE 0x4f46c4, indexed by
// effect type) plus a 17-byte network spawn record (MSG 0x8003). Both models
// were recovered clean-room from db/ + docs; this module turns a raw record
// buffer into a semantic form for fx/fxs display. It never transcribes game
// bytes — callers supply the buffer (a synthetic fixture, or the real table
// sliced from the executable's .data at integration time).

namespace fx {

// One effect-parameter record is 0x30 bytes; effect type `t` lives at
// `table + t*EFFECT_RECORD_SIZE`. The network spawn record is 0x11 bytes.
constexpr size_t EFFECT_RECORD_SIZE = 0x30;   // 48
constexpr size_t EFFECT_SPAWN_SIZE  = 0x11;   // 17

// Effect class, derived from the type index via the shape-handle table that
// _GRAPHICInit@0 (0x442c00) fills (objects.md § Effect types and shapes).
enum class EffectClass : uint8_t {
    None,       // type 0 — no shape / mixed
    Crater,     // 1..3    crater.SH
    Debris,     // 4..6    debris.SH
    Smoke,      // 7..11   smoke.SH
    Chaff,      // 12      chaff.SH
    Flare,      // 13      flare.SH
    Fire,       // 14      fire.SH
    Explosion,  // 15..0x26 exp.SH (airbursts, ground bursts, water, ...)
    DustPuff,   // 0x28..0x2A spd/mpd/lpd.SH
    Unknown,    // gaps (e.g. 0x27) / out of range
};

// Semantic form of one 0x30-byte effect-parameter record. Field offsets are
// within the record; unlisted bytes are not yet individually resolved (#54).
struct EffectParams {
    int         type  = 0;                        // record index (effect type)
    EffectClass klass = EffectClass::Unknown;     // from `type`
    int16_t     intensity     = 0;   // +0x04 base brightness/scale
    int16_t     frame_count   = 0;   // +0x06 shape frame count / start frame
    uint16_t    subtype       = 0;   // +0x08 sub-type / shape selector
    bool        ground_burst  = false; // +0x08 low byte, bit 2
    int16_t     debris_count  = 0;   // +0x0A secondary-debris count
    int16_t     debris_spread = 0;   // +0x0C secondary-debris spread
    // +0x0E.. : up to 8 sound-effect name pointers (absolute VAs into the
    // executable's string data). Without the whole image the names can't be
    // resolved, but the count of populated (non-null) slots is meaningful.
    int         sound_variants = 0;
    uint32_t    sound_ptrs[8]  = {};
    int16_t     sound_pitch    = 0;  // +0x2E
};

// Semantic form of the 17-byte MSG 0x8003 network effect-spawn record
// (objects.md § Adders and lifecycle). Positions are F24.8 (feet = raw/256).
struct EffectSpawn {
    uint8_t  type  = 0;   // +0x00 effect type
    int32_t  x = 0, y = 0, z = 0;  // +0x01/+0x05/+0x09 world position (F24.8)
    uint16_t owner = 0;   // +0x0D owner object index (0xffff = none)
    uint8_t  flag0 = 0;   // +0x0F
    uint8_t  flag1 = 0;   // +0x10
};

// Classify an effect type index and name its shape / class.
EffectClass effect_class_for_type(int type);
const char* effect_class_name(EffectClass klass);   // "smoke", "explosion", ...
const char* effect_shape_for_type(int type);        // "exp.SH", ...; "" if none/unknown

// Parse the effect-parameter record for `type` out of a table buffer. Returns
// false if the buffer is too short to hold `table + type*0x30 + 0x30`.
bool effect_parse_record(const uint8_t* data, size_t size, int type, EffectParams& out);

// Parse `count` consecutive records from the head of `data`. Stops early (and
// returns the records parsed so far) if the buffer runs out.
std::vector<EffectParams> effect_parse_table(const uint8_t* data, size_t size, int count);

// Parse one MSG 0x8003 spawn record. false if size < EFFECT_SPAWN_SIZE.
bool effect_parse_spawn(const uint8_t* data, size_t size, EffectSpawn& out);

} // namespace fx
