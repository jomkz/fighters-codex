#pragma once
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

// Committed synthetic fixtures (policy: tests/fixtures/README.md).
// FX_FIXTURES_DIR is injected by tests/CMakeLists.txt. Call from inside a
// test case: a missing or unreadable fixture fails the assertion, not the
// process.
namespace fx_test {

inline std::vector<uint8_t> load_fixture(const std::string& rel) {
    const std::string path = std::string(FX_FIXTURES_DIR "/") + rel;
    std::ifstream in(path, std::ios::binary);
    INFO("fixture: " << path);
    REQUIRE(in.good());
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in),
                                std::istreambuf_iterator<char>());
}

}  // namespace fx_test
