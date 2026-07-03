#include <catch2/catch_test_macros.hpp>
#include "platform/audio_player.h"

#include <chrono>
#include <thread>
#include <vector>

// gui_tests runs with FX_AUDIO_NULL=1 (set on the ctest), so the player
// uses miniaudio's null backend: no audio hardware, realtime consumption
// simulated — the full state machine is exercised in CI.

using platform::AudioPlayer;

static std::vector<uint8_t> MakeClip(int samples) {
    std::vector<uint8_t> pcm(samples);
    for (int i = 0; i < samples; ++i)
        pcm[i] = (uint8_t)(0x80 + (i & 0x3F));
    return pcm;
}

// Poll Update() like the frame loop does, until the player leaves the
// Playing state or the deadline passes.
static bool PumpUntilStopped(AudioPlayer& p, int timeoutMs) {
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        p.Update();
        if (!p.IsPlaying()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

TEST_CASE("Play starts playback from the requested sample", "[gui][audio]") {
    auto pcm = MakeClip(8000);
    AudioPlayer p;
    REQUIRE(p.Play(pcm.data(), (int)pcm.size(), 8000, 2000));
    CHECK(p.IsPlaying());
    CHECK_FALSE(p.IsPaused());
    CHECK(p.Position() >= 2000);
    CHECK(p.resumeFrom == 2000);
    p.Stop();
    CHECK_FALSE(p.IsPlaying());
    CHECK(p.Position() == 0);
}

TEST_CASE("natural end of clip resets to Stopped via Update", "[gui][audio]") {
    auto pcm = MakeClip(400);          // 50 ms at 8 kHz
    AudioPlayer p;
    REQUIRE(p.Play(pcm.data(), (int)pcm.size(), 8000, 0));
    REQUIRE(PumpUntilStopped(p, 5000));
    CHECK_FALSE(p.IsPaused());
    CHECK(p.resumeFrom == 0);          // playhead reset for the next Play
    CHECK(p.Position() == 0);
}

TEST_CASE("Pause parks at the current position and resumes", "[gui][audio]") {
    auto pcm = MakeClip(22050);        // 1 s at 22.05 kHz
    AudioPlayer p;
    REQUIRE(p.Play(pcm.data(), (int)pcm.size(), 22050, 0));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    p.Pause();
    CHECK(p.IsPaused());
    CHECK_FALSE(p.IsPlaying());
    int parked = p.Position();
    CHECK(parked == p.resumeFrom);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    CHECK(p.Position() == parked);     // position is stable while paused

    REQUIRE(p.Play(pcm.data(), (int)pcm.size(), 22050, p.resumeFrom));
    CHECK(p.IsPlaying());
    CHECK(p.Position() >= parked);
    p.Stop();
}

TEST_CASE("PauseAt parks the playhead without playing", "[gui][audio]") {
    AudioPlayer p;
    p.PauseAt(1234);
    CHECK(p.IsPaused());
    CHECK(p.Position() == 1234);
    CHECK(p.resumeFrom == 1234);
    p.Stop();
    CHECK(p.Position() == 0);
}

TEST_CASE("Play rejects empty input and out-of-range seeks", "[gui][audio]") {
    AudioPlayer p;
    CHECK_FALSE(p.Play(nullptr, 0, 11025, 0));
    CHECK_FALSE(p.IsPlaying());

    auto pcm = MakeClip(100);
    // fromSample past the end clamps to the last sample — still playable.
    CHECK(p.Play(pcm.data(), (int)pcm.size(), 11025, 5000));
    REQUIRE(PumpUntilStopped(p, 5000));
}

TEST_CASE("all FA sample rates initialise a device", "[gui][audio]") {
    AudioPlayer p;
    for (int rate : {5000, 8000, 11025, 22050}) {
        // Half a second per rate — long enough that the device's start-up
        // prefill cannot consume the whole clip before IsPlaying is checked
        // (real backends prefill several periods synchronously on start).
        auto pcm = MakeClip(rate / 2);
        REQUIRE(p.Play(pcm.data(), (int)pcm.size(), rate, 0));
        CHECK(p.IsPlaying());
        p.Stop();
    }
}
