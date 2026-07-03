#include <catch2/catch_test_macros.hpp>
#include "platform/dialog_queue.h"

#include <string>
#include <thread>
#include <vector>

using platform::DialogQueue;
using platform::EnsureExtension;

TEST_CASE("continuation runs on Pump after Complete, not before", "[gui][dialogs]") {
    DialogQueue q;
    std::vector<std::string> got;
    bool ran = false;

    REQUIRE(q.Begin([&](std::vector<std::string> v) { ran = true; got = std::move(v); }));
    CHECK(q.Busy());

    q.Pump();                       // no result yet — must not run
    CHECK_FALSE(ran);

    q.Complete({"/a/b.LIB", "/c/d.LIB"});
    CHECK(q.Busy());                // still pending until pumped
    CHECK_FALSE(ran);

    q.Pump();
    CHECK(ran);
    CHECK(got == std::vector<std::string>{"/a/b.LIB", "/c/d.LIB"});
    CHECK_FALSE(q.Busy());
}

// Test names stay ASCII: catch_discover_tests round-trips them through
// ctest, and non-ASCII bytes mangle under Windows console codepages.
TEST_CASE("one dialog at a time: Begin while busy is refused", "[gui][dialogs]") {
    DialogQueue q;
    REQUIRE(q.Begin([](std::vector<std::string>) {}));
    CHECK_FALSE(q.Begin([](std::vector<std::string>) {}));

    q.Complete({});
    q.Pump();
    CHECK(q.Begin([](std::vector<std::string>) {}));   // free again
}

TEST_CASE("cancellation delivers an empty result", "[gui][dialogs]") {
    DialogQueue q;
    bool ran = false;
    std::vector<std::string> got = {"sentinel"};
    REQUIRE(q.Begin([&](std::vector<std::string> v) { ran = true; got = std::move(v); }));
    q.Complete({});
    q.Pump();
    CHECK(ran);
    CHECK(got.empty());
}

TEST_CASE("Shutdown drops the pending continuation", "[gui][dialogs]") {
    DialogQueue q;
    bool ran = false;
    REQUIRE(q.Begin([&](std::vector<std::string>) { ran = true; }));
    q.Shutdown();
    CHECK_FALSE(q.Busy());
    q.Complete({"/late/result"});   // arrives after shutdown — ignored
    q.Pump();
    CHECK_FALSE(ran);
}

TEST_CASE("Complete without a pending dialog is a no-op", "[gui][dialogs]") {
    DialogQueue q;
    q.Complete({"/stray"});
    q.Pump();                       // nothing to run, nothing to crash
    CHECK_FALSE(q.Busy());
}

TEST_CASE("Complete may arrive from another thread", "[gui][dialogs]") {
    DialogQueue q;
    std::vector<std::string> got;
    REQUIRE(q.Begin([&](std::vector<std::string> v) { got = std::move(v); }));

    std::thread producer([&] { q.Complete({"/from/other/thread"}); });
    producer.join();

    q.Pump();
    CHECK(got == std::vector<std::string>{"/from/other/thread"});
}

TEST_CASE("a continuation may start the next dialog", "[gui][dialogs]") {
    // The old sync flow chained dialogs; the queue must allow Begin from
    // inside a running continuation (busy is cleared before invocation).
    DialogQueue q;
    bool chained = false;
    bool second  = false;
    REQUIRE(q.Begin([&](std::vector<std::string>) {
        chained = q.Begin([&](std::vector<std::string>) { second = true; });
    }));
    q.Complete({"/first"});
    q.Pump();
    CHECK(chained);
    CHECK(q.Busy());                // second dialog now pending
    q.Complete({"/second"});
    q.Pump();
    CHECK(second);
}

TEST_CASE("EnsureExtension appends only when missing", "[gui][dialogs]") {
    CHECK(EnsureExtension("shot", "png") == "shot.png");
    CHECK(EnsureExtension("shot.png", "png") == "shot.png");
    CHECK(EnsureExtension("shot.PNG", "png") == "shot.PNG");    // any ext counts
    CHECK(EnsureExtension("a.dir/shot", "png") == "a.dir/shot.png"); // dot in dir only
    CHECK(EnsureExtension("a.dir\\shot", "png") == "a.dir\\shot.png");
    CHECK(EnsureExtension("", "png").empty());
    CHECK(EnsureExtension("shot", nullptr) == "shot");
    CHECK(EnsureExtension("shot", "") == "shot");
}
