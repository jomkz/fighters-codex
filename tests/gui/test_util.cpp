#include <catch2/catch_test_macros.hpp>
#include "util.h"

TEST_CASE("ci_equal matches ASCII case-insensitively", "[gui][util]") {
    CHECK(fxg::ci_equal("PALETTE.PAL", "palette.pal"));
    CHECK(fxg::ci_equal("f15.bi", "F15.BI"));
    CHECK(fxg::ci_equal("", ""));
    CHECK_FALSE(fxg::ci_equal("PT", "PTS"));
    CHECK_FALSE(fxg::ci_equal("PTS", "PT"));
    CHECK_FALSE(fxg::ci_equal("A", "B"));
}

TEST_CASE("ci_equal folds ASCII only", "[gui][util]") {
    // Bytes >= 0x80 must compare verbatim — no locale-dependent folding.
    CHECK_FALSE(fxg::ci_equal("\xC4", "\xE4"));
    CHECK(fxg::ci_equal("\xC4", "\xC4"));
}

TEST_CASE("ci_prefix matches leading substring", "[gui][util]") {
    CHECK(fxg::ci_prefix("MIX2TABLE.BIN", "mix2"));
    CHECK(fxg::ci_prefix("anything", ""));
    CHECK_FALSE(fxg::ci_prefix("MIX", "MIX2"));   // hay shorter than needle
    CHECK_FALSE(fxg::ci_prefix("XMIX2", "MIX2")); // not at the start
}

TEST_CASE("ci_contains finds substrings anywhere", "[gui][util]") {
    CHECK(fxg::ci_contains("F15EAGLE.PT", "eagle"));
    CHECK(fxg::ci_contains("F15EAGLE.PT", ".pt"));
    CHECK(fxg::ci_contains("abc", ""));
    CHECK(fxg::ci_contains("abc", nullptr));
    CHECK_FALSE(fxg::ci_contains("abc", "abcd"));
    CHECK_FALSE(fxg::ci_contains("", "a"));
}

TEST_CASE("copy_str truncates and always NUL-terminates", "[gui][util]") {
    char buf[8];
    fxg::copy_str(buf, sizeof(buf), "short");
    CHECK(std::string(buf) == "short");

    fxg::copy_str(buf, sizeof(buf), "exactly7");
    CHECK(std::string(buf) == "exactly");   // truncated to cap-1

    fxg::copy_str(buf, sizeof(buf), "");
    CHECK(buf[0] == '\0');

    // cap == 1 writes only the terminator
    char one[1] = {'x'};
    fxg::copy_str(one, sizeof(one), "abc");
    CHECK(one[0] == '\0');
}
