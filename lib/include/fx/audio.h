#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// FA raw PCM audio formats.
//
// .11K  -- unsigned 8-bit mono PCM at 11025 Hz
// .5K   -- unsigned 8-bit mono PCM at  5000 Hz
// .8K   -- unsigned 8-bit mono PCM at  8000 Hz
//
// The '&' prefix on filenames is a game convention for looping sounds.
// It is NOT part of the audio data format.
//
// WAV 8-bit is also unsigned (values 0-255, silence = 128), so the raw
// bytes can be copied directly into / out of a standard WAV file.

namespace fx {

struct AudioInfo {
    uint32_t sample_rate;  // Hz (11025, 5000, 8000, ...)
    uint32_t num_samples;  // total sample count
    double   duration_s;   // num_samples / sample_rate
};

// Guess sample rate from file extension (lower-cased ext including dot).
// Returns 0 if unknown.
uint32_t audio_rate_from_ext(const std::string& ext);

// Parse audio metadata from raw PCM bytes + known sample rate.
AudioInfo audio_info(const uint8_t* data, size_t size, uint32_t sample_rate);

// Wrap raw PCM bytes in a standard WAV container.
// sample_rate: e.g. 11025.  Returns empty on error.
std::vector<uint8_t> audio_to_wav(const uint8_t* pcm, size_t pcm_size,
                                   uint32_t sample_rate);

// Extract raw PCM from a WAV file.
// On success, sets out_rate to the WAV's sample rate.
// Returns empty vector on error (wrong format, not 8-bit mono, etc.).
std::vector<uint8_t> wav_to_pcm(const uint8_t* wav, size_t wav_size,
                                  uint32_t* out_rate);

} // namespace fx
