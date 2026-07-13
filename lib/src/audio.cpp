#include "fx/audio.h"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace fx {

// ---- helpers ---------------------------------------------------------

static std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

static uint32_t read_u32le(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t read_u16le(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static void write_u32le(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void write_u16le(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
}

// ---- public API ------------------------------------------------------

uint32_t audio_rate_from_ext(const std::string& ext) {
    std::string e = lower(ext);
    if (e == ".22k") return 22050;
    if (e == ".11k") return 11025;
    // 5512, not 5000 (#491): the extension is a rounded label, and .5K is half of .11K
    // (11025 / 2 = 5512). The assets prove it against an independent clock — IQC.5K is
    // 619,237 bytes and its paired IQCA.VDO runs 1,685 frames at 15 fps = 112.33 s, so
    // 619237 / 112.33 = 5,513 Hz. At 5000 every .5K we emitted played 10% long.
    if (e == ".5k")  return  5512;
    if (e == ".8k")  return  8000;
    return 0;
}

AudioInfo audio_info(const uint8_t* /*data*/, size_t size, uint32_t sample_rate) {
    AudioInfo info;
    info.sample_rate = sample_rate;
    info.num_samples = (uint32_t)size;
    info.duration_s  = sample_rate > 0 ? (double)size / sample_rate : 0.0;
    return info;
}

// Standard WAV header for PCM: RIFF/WAVE + fmt (16 bytes) + data
// Total header size = 44 bytes.
std::vector<uint8_t> audio_to_wav(const uint8_t* pcm, size_t pcm_size,
                                   uint32_t sample_rate) {
    if (pcm_size > 0xFFFF'FFFF - 44) return {}; // overflow guard

    uint32_t data_size  = (uint32_t)pcm_size;
    uint32_t riff_size  = 36 + data_size; // size after "RIFF" + size field itself
    uint16_t num_ch     = 1;
    uint16_t bits       = 8;
    uint32_t byte_rate  = sample_rate * num_ch * (bits / 8); // = sample_rate for mono 8-bit
    uint16_t block_align = (uint16_t)(num_ch * (bits / 8));  // = 1

    std::vector<uint8_t> out(44 + data_size);
    uint8_t* p = out.data();

    // RIFF chunk
    memcpy(p,     "RIFF", 4); p += 4;
    write_u32le(p, riff_size);        p += 4;
    memcpy(p,     "WAVE", 4);        p += 4;

    // fmt  chunk
    memcpy(p,     "fmt ", 4);        p += 4;
    write_u32le(p, 16);              p += 4; // PCM chunk size
    write_u16le(p, 1);               p += 2; // format = PCM
    write_u16le(p, num_ch);          p += 2;
    write_u32le(p, sample_rate);     p += 4;
    write_u32le(p, byte_rate);       p += 4;
    write_u16le(p, block_align);     p += 2;
    write_u16le(p, bits);            p += 2;

    // data chunk
    memcpy(p,     "data", 4);        p += 4;
    write_u32le(p, data_size);       p += 4;
    if (pcm_size > 0) memcpy(p, pcm, pcm_size);

    return out;
}

std::vector<uint8_t> wav_to_pcm(const uint8_t* wav, size_t wav_size,
                                  uint32_t* out_rate) {
    if (out_rate) *out_rate = 0;

    // Minimum WAV size: 12 (RIFF hdr) + 24 (fmt chunk) + 8 (data hdr) = 44
    if (wav_size < 44) return {};

    // Check "RIFF" and "WAVE" signatures
    if (memcmp(wav,     "RIFF", 4) != 0) return {};
    if (memcmp(wav + 8, "WAVE", 4) != 0) return {};

    // Walk chunks to find "fmt " and "data"
    size_t pos = 12;
    uint32_t sample_rate  = 0;
    uint16_t num_channels = 0;
    uint16_t bits_per_sample = 0;
    uint16_t audio_format = 0;
    bool     found_fmt  = false;
    bool     found_data = false;
    const uint8_t* pcm_start = nullptr;
    size_t         pcm_size  = 0;

    while (pos + 8 <= wav_size) {
        char tag[5] = {};
        memcpy(tag, wav + pos, 4);
        uint32_t chunk_size = read_u32le(wav + pos + 4);
        pos += 8;

        if (memcmp(tag, "fmt ", 4) == 0 && chunk_size >= 16 && pos + 16 <= wav_size) {
            audio_format    = read_u16le(wav + pos);
            num_channels    = read_u16le(wav + pos + 2);
            sample_rate     = read_u32le(wav + pos + 4);
            // byte_rate     = read_u32le(wav + pos + 8);
            // block_align   = read_u16le(wav + pos + 12);
            bits_per_sample = read_u16le(wav + pos + 14);
            found_fmt = true;
        } else if (memcmp(tag, "data", 4) == 0) {
            pcm_start  = wav + pos;
            pcm_size   = (chunk_size <= wav_size - pos) ? chunk_size : (wav_size - pos);
            found_data = true;
            break;
        }

        // Chunks are word-aligned (size rounded up to even)
        pos += (chunk_size + 1) & ~1u;
    }

    if (!found_fmt || !found_data)         return {};
    if (audio_format != 1)                 return {}; // must be PCM
    if (num_channels != 1)                 return {}; // must be mono
    if (bits_per_sample != 8)              return {}; // must be 8-bit

    if (out_rate) *out_rate = sample_rate;
    return std::vector<uint8_t>(pcm_start, pcm_start + pcm_size);
}

} // namespace fx
