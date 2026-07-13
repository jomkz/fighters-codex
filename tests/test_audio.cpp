#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <fx/audio.h>
#include <fx/ealib.h>
#include <fx/fbc.h>
#include <fx/vdo.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <cstdint>
#include <vector>

using namespace fx;

TEST_CASE("audio_rate_from_ext recognises all FA extensions") {
    REQUIRE(audio_rate_from_ext(".11k") == 11025u);
    // 5512, not 5000 (#491). The extension is a rounded label; the rate is half of .11K.
    // This assertion USED to demand 5000 — the test agreed with the bug, so the suite was
    // green while every .5K fx emitted played 10% long. Asserting what the code does is
    // not a test. See the real-asset check below for the proof.
    REQUIRE(audio_rate_from_ext(".5k")  == 5512u);
    REQUIRE(audio_rate_from_ext(".8k")  == 8000u);
    REQUIRE(audio_rate_from_ext(".22k") == 22050u);
    // .5K is exactly half of .11K — the relationship the label rounds away.
    REQUIRE(audio_rate_from_ext(".11k") / 2 == audio_rate_from_ext(".5k"));
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

// fuzz_audio (#117): a "fmt " chunk claiming size >= 16 but whose body runs
// past the file end let wav_to_pcm read 16 bytes out of bounds. The read is
// now gated on the body actually fitting.
TEST_CASE("wav_to_pcm rejects a fmt chunk body that runs past the file") {
    std::vector<uint8_t> wav;
    auto put = [&](const char* s, size_t n) { for (size_t i=0;i<n;i++) wav.push_back((uint8_t)s[i]); };
    auto w32 = [&](uint32_t v){ for(int i=0;i<4;i++) wav.push_back((uint8_t)(v>>(8*i))); };
    put("RIFF", 4); w32(0x24); put("WAVE", 4);
    put("fmt ", 4); w32(16);   // claims a 16-byte body...
    wav.push_back(1); wav.push_back(0); wav.push_back(1); wav.push_back(0);  // ...but only 4 follow
    uint32_t rate = 0;
    auto pcm = wav_to_pcm(wav.data(), wav.size(), &rate);
    CHECK(pcm.empty());  // must reject, not read past the buffer
}

// ---------------------------------------------------------------------------
// Real-asset decode census (#491). A byte-identical round-trip proves nothing about
// the sample RATE — the rate is never stored in the file, it is inferred from the
// extension, so no repack can contradict it. Only an independent clock can.
//
// The briefing videos are that clock. VDO.md: audio is shared per 3-character group
// (`AAC.11K` narrates `AACA`…`AACE`), the paired `.FBC` gives one u32 per frame, and
// the header gives the fps. Video duration = frames / fps is derived from bytes we
// never guessed at, so:
//
//     audio_bytes / rate_from_extension  <=  frames / fps
//
// The inequality, not equality: a narration may finish before its clip does (ZAC.11K is
// 8.0 s of speech over a 14.7 s clip, and it is the only such track in the corpus). But
// audio can never OUTLAST the video it narrates — and that is precisely the shape of a
// rate that is too low. At the old .5K rate of 5000, IQC.5K claims 123.8 s of audio over
// 112.3 s of video; at 5512 it lands within 0.1%. The median ratio pins it from the other
// side: 105 of the 106 tracks align exactly, so a systematic rate error moves the median
// off 1.0 even if some individual track were forgiving.
TEST_CASE("audio duration reconciles with the paired video's frame count") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root)
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");
    namespace fs = std::filesystem;
    auto upper = [](std::string s) {
        for (char& c : s) c = (char)std::toupper((unsigned char)c);
        return s;
    };

    // Every entry of every LIB, by upper-cased name.
    std::map<std::string, std::vector<uint8_t>> assets;
    for (const auto& de : fs::directory_iterator(root)) {
        if (!de.is_regular_file()) continue;
        std::string fn = upper(de.path().filename().string());
        if (fn.size() < 4 || fn.substr(fn.size() - 4) != ".LIB") continue;
        std::ifstream f(de.path(), std::ios::binary | std::ios::ate);
        std::vector<uint8_t> lib((size_t)f.tellg());
        f.seekg(0);
        f.read((char*)lib.data(), (std::streamsize)lib.size());
        for (const auto& e : ealib_read_dir(lib.data(), lib.size())) {
            std::string name = upper(e.name);
            const bool want = name.size() > 4 &&
                (name.find(".5K") != std::string::npos ||
                 name.find(".11K") != std::string::npos ||
                 name.find(".FBC") != std::string::npos ||
                 name.find(".VDO") != std::string::npos);
            if (!want) continue;
            auto bytes = ealib_extract(lib.data(), lib.size(), e, true);
            if (!bytes.empty()) assets.emplace(name, std::move(bytes));
        }
    }
    REQUIRE_FALSE(assets.empty());

    int checked = 0;
    std::vector<double> ratios;
    for (const auto& [name, pcm] : assets) {
        const size_t dot = name.rfind('.');
        const std::string stem = name.substr(0, dot), ext = name.substr(dot);
        const uint32_t rate = audio_rate_from_ext(ext);
        if (rate == 0) continue;                    // not an audio track

        // Sum the frames of every video segment in this briefing group (stem + variant).
        uint32_t frames = 0, fps = 0;
        for (char v = 'A'; v <= 'Z'; ++v) {
            auto fbc = assets.find(stem + v + ".FBC");
            auto vdo = assets.find(stem + v + ".VDO");
            if (fbc == assets.end() || vdo == assets.end()) continue;
            bool ok = false;
            auto sizes = fbc_read(fbc->second.data(), fbc->second.size(), &ok);
            if (!ok) continue;
            VdoInfo vi{};
            if (!vdo_info(vdo->second.data(), vdo->second.size(), &vi) || vi.fps == 0) continue;
            frames += (uint32_t)sizes.size();
            fps = vi.fps;
        }
        if (frames == 0 || fps == 0) continue;      // no paired video: nothing to check against

        const double video_s = (double)frames / fps;
        const double audio_s = (double)pcm.size() / rate;
        INFO(name << ": " << pcm.size() << " bytes @ " << rate << " Hz = " << audio_s
                  << " s, against " << frames << " frames @ " << fps << " fps = "
                  << video_s << " s");
        CHECK(audio_s <= video_s * 1.01);   // narration may end early; it may not overrun
        ratios.push_back(audio_s / video_s);
        ++checked;
    }
    REQUIRE(checked > 0);

    // …and the corpus as a whole must sit on the video clock, which no rounding of the
    // extension label can fake: at 5000 Hz the .5K ratio is 1.102, not 1.000.
    std::sort(ratios.begin(), ratios.end());
    const double median = ratios[ratios.size() / 2];
    INFO("median audio/video duration ratio over " << checked << " tracks: " << median);
    CHECK(std::fabs(median - 1.0) < 0.01);
    WARN("audio/video duration census: " << checked << " tracks reconciled");
}
