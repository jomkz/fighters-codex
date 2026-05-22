#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <fx/audio.h>
#include <cmath>
#include <cstdint>
#include <vector>

using namespace fx;

TEST_CASE("audio_rate_from_ext recognises all FA extensions") {
    REQUIRE(audio_rate_from_ext(".11k") == 11025u);
    REQUIRE(audio_rate_from_ext(".5k")  == 5000u);
    REQUIRE(audio_rate_from_ext(".8k")  == 8000u);
    REQUIRE(audio_rate_from_ext(".22k") == 22050u);
}

TEST_CASE("audio_rate_from_ext returns 0 for unknown extension") {
    REQUIRE(audio_rate_from_ext(".wav") == 0u);
    REQUIRE(audio_rate_from_ext(".mp3") == 0u);
    REQUIRE(audio_rate_from_ext("")     == 0u);
}

TEST_CASE("audio_info reports correct sample count and rate") {
    uint8_t dummy[100] = {};
    AudioInfo info = audio_info(dummy, 100, 11025);
    REQUIRE(info.num_samples == 100u);
    REQUIRE(info.sample_rate == 11025u);
}

TEST_CASE("audio_info computes duration from sample count and rate") {
    uint8_t dummy[44100] = {};
    AudioInfo info = audio_info(dummy, 44100, 44100);
    REQUIRE(info.duration_s == Catch::Approx(1.0));
}

TEST_CASE("audio_to_wav produces a RIFF/WAVE header") {
    std::vector<uint8_t> pcm(44, 128);
    auto wav = audio_to_wav(pcm.data(), pcm.size(), 11025);
    REQUIRE(wav.size() >= 44u);
    REQUIRE(wav[0] == 'R');
    REQUIRE(wav[1] == 'I');
    REQUIRE(wav[2] == 'F');
    REQUIRE(wav[3] == 'F');
    REQUIRE(wav[8]  == 'W');
    REQUIRE(wav[9]  == 'A');
    REQUIRE(wav[10] == 'V');
    REQUIRE(wav[11] == 'E');
}

TEST_CASE("audio_to_wav / wav_to_pcm round-trip") {
    std::vector<uint8_t> pcm = {10, 20, 30, 40, 128, 200, 50, 75};
    auto wav = audio_to_wav(pcm.data(), pcm.size(), 11025);
    REQUIRE_FALSE(wav.empty());

    uint32_t rate = 0;
    auto recovered = wav_to_pcm(wav.data(), wav.size(), &rate);
    REQUIRE(recovered == pcm);
    REQUIRE(rate == 11025u);
}

TEST_CASE("wav_to_pcm rejects non-WAV data") {
    std::vector<uint8_t> garbage = {0, 1, 2, 3, 4, 5, 6, 7};
    uint32_t rate = 0;
    REQUIRE(wav_to_pcm(garbage.data(), garbage.size(), &rate).empty());
}

TEST_CASE("audio_to_wav round-trip preserves sample rate for 8K audio") {
    std::vector<uint8_t> pcm(80, 64);
    auto wav = audio_to_wav(pcm.data(), pcm.size(), 8000);
    uint32_t rate = 0;
    auto recovered = wav_to_pcm(wav.data(), wav.size(), &rate);
    REQUIRE(rate == 8000u);
    REQUIRE(recovered == pcm);
}
