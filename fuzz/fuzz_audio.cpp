// Fuzz target: the PCM audio helpers — WAV container parse (the real
// attack surface: RIFF chunk walking over untrusted bytes), plus the
// PCM→WAV wrapper and a wrap→parse round-trip on the raw input.

#include <cstddef>
#include <cstdint>
#include <vector>

#include <fx/audio.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    uint32_t rate = 0;
    fx::wav_to_pcm(data, size, &rate);

    fx::audio_info(data, size, 11025);
    std::vector<uint8_t> wav = fx::audio_to_wav(data, size, 11025);
    if (!wav.empty()) fx::wav_to_pcm(wav.data(), wav.size(), &rate);
    return 0;
}
