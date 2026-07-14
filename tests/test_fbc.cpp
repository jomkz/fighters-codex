#include <catch2/catch_test_macros.hpp>
#include <fx/fbc.h>
#include <fx/ealib.h>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <map>
#include <string>

using namespace fx;

TEST_CASE("fbc_read parses a flat u32le frame-size array") {
    // Three frames: 100, 256, 65536 bytes.
    std::vector<uint8_t> data = {
        100, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
    };
    bool ok = false;
    auto sizes = fbc_read(data.data(), data.size(), &ok);
    REQUIRE(ok);
    REQUIRE(sizes == std::vector<uint32_t>{ 100, 256, 65536 });
}

TEST_CASE("fbc_read rejects a size that is not a multiple of 4") {
    std::vector<uint8_t> data = { 1, 2, 3 };
    bool ok = true;
    REQUIRE(fbc_read(data.data(), data.size(), &ok).empty());
    REQUIRE_FALSE(ok);
}

TEST_CASE("fbc_read accepts an empty (zero-frame) index") {
    bool ok = false;
    REQUIRE(fbc_read(nullptr, 0, &ok).empty());
    REQUIRE(ok);
}

TEST_CASE("fbc_write is the byte-identical inverse of fbc_read") {
    std::vector<uint8_t> data = {
        0x01, 0x02, 0x03, 0x04,
        0xFF, 0xFF, 0xFF, 0xFF,
        0x00, 0x00, 0x00, 0x00,
    };
    auto sizes = fbc_read(data.data(), data.size());
    REQUIRE(fbc_write(sizes) == data);
}

TEST_CASE("fbc_frame_offset accumulates from the 816-byte VDO header") {
    // FBC.md invariant: frame_data_offset(n) = 816 + sum(FBC[0..n)), and
    // offset(N) equals the paired VDO's file size.
    std::vector<uint32_t> sizes = { 10, 20, 30 };
    REQUIRE(fbc_frame_offset(sizes, 0) == 816);
    REQUIRE(fbc_frame_offset(sizes, 1) == 826);
    REQUIRE(fbc_frame_offset(sizes, 2) == 846);
    REQUIRE(fbc_frame_offset(sizes, 3) == 876);
    REQUIRE(fbc_frame_offset(sizes, 99) == 876); // clamped to N
}

// ---------------------------------------------------------------------------
// The real-asset census (#491 A).
//
// FBC.md says the corpus was "verified against #137 with zero mismatches" -- once, by hand.
// Nothing re-checked it since, which is the exact failure mode #491 is about: a claim that is
// not mechanically checked drifts. The invariant is a genuine cross-format ORACLE -- the index
// must predict the paired video's size to the byte -- so it is worth holding on to.
// ---------------------------------------------------------------------------

TEST_CASE("FBC decode census: every index predicts its video's exact size") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root)
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");
    namespace fs = std::filesystem;

    auto lower = [](std::string s) {
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };

    // Collect both halves of every pair, out of whichever LIB carries them (FA_7).
    std::map<std::string, std::vector<uint8_t>> fbc, vdo;
    for (const auto& de : fs::directory_iterator(root)) {
        if (!de.is_regular_file()) continue;
        if (lower(de.path().extension().string()) != ".lib") continue;
        std::ifstream f(de.path(), std::ios::binary | std::ios::ate);
        std::vector<uint8_t> lib((size_t)f.tellg());
        f.seekg(0);
        f.read((char*)lib.data(), (std::streamsize)lib.size());

        for (const auto& e : ealib_read_dir(lib.data(), lib.size())) {
            const std::string ext  = lower(fs::path(e.name).extension().string());
            const std::string stem = lower(fs::path(e.name).stem().string());
            if (ext == ".fbc") fbc[stem] = ealib_extract(lib.data(), lib.size(), e, true);
            else if (ext == ".vdo") vdo[stem] = ealib_extract(lib.data(), lib.size(), e, true);
        }
    }
    if (fbc.empty())
        SKIP("no .FBC in this install (they live in FA_7.LIB, from disc 1)");

    size_t frames_total = 0;
    for (const auto& [stem, data] : fbc) {
        INFO(stem << ".FBC");
        REQUIRE_FALSE(data.empty());

        bool ok = false;
        std::vector<uint32_t> sizes = fbc_read(data.data(), data.size(), &ok);
        REQUIRE(ok);
        REQUIRE_FALSE(sizes.empty());

        // 1. Round-trip. FBC.md claims it; no test opened a real one.
        REQUIRE(fbc_write(sizes) == data);

        // 2. Exactly one same-stem .VDO -- the pairing rule.
        auto it = vdo.find(stem);
        REQUIRE(it != vdo.end());
        const std::vector<uint8_t>& v = it->second;

        // 3. THE ORACLE: the index must account for every byte of the video after its
        //    816-byte header. An off-by-one in the header size, or a frame the index
        //    mis-sized, and this misses.
        REQUIRE(fbc_frame_offset(sizes, sizes.size()) == v.size());

        // 4. And the frame count agrees with the number the VDO states about itself.
        REQUIRE(v.size() >= 0x12);
        const uint16_t vdo_frames = (uint16_t)(v[0x10] | (v[0x11] << 8));
        REQUIRE(sizes.size() == vdo_frames);

        frames_total += sizes.size();
    }
    WARN("FBC census: " << fbc.size() << " index/video pairs, " << frames_total
         << " frames, every index predicting its video's size to the byte");
}
