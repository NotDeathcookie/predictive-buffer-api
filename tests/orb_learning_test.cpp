// Unit tests for pbuf/OrbLearning.hpp (no game needed).
#include "pbuf/OrbLearning.hpp"

#include <cstdio>

using namespace pbuf;

static int g_checks = 0, g_fail = 0;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_fail; std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void unseen_key_is_nullopt() {
    OrbLearning t;
    CHECK(!t.lookup(42).has_value());
    CHECK(t.observationCount(42) == 0);
}

static void records_and_averages() {
    OrbLearning t;
    t.record(1, 4.0, false);
    t.record(1, 6.0, false);
    auto r = t.lookup(1);
    CHECK(r.has_value());
    CHECK(r->vy == 5.0); // average of 4 and 6
    CHECK(r->flips == false);
    CHECK(t.observationCount(1) == 2);
}

static void flip_is_majority_vote() {
    OrbLearning t;
    t.record(2, -10.0, true);
    t.record(2, -10.0, true);
    t.record(2, -10.0, false);
    auto r = t.lookup(2);
    CHECK(r.has_value() && r->flips == true); // 2 of 3 flipped
}

static void fallback_used_only_when_primary_key_unseen() {
    OrbLearning t;
    t.record(100, 4.46, false); // e.g. "pink orb, normal size, normal gravity"
    // key 101 = "pink orb, mini size, normal gravity" never observed directly
    auto r = t.lookupWithFallback(101, 100);
    CHECK(r.has_value() && r->vy == 4.46);

    t.record(101, 3.0, false); // now it HAS been observed directly
    auto r2 = t.lookupWithFallback(101, 100);
    CHECK(r2.has_value() && r2->vy == 3.0); // direct observation wins, not the fallback
}

static void round_trips_through_serialize() {
    OrbLearning a;
    a.record(5, -10.62, true);
    a.record(5, -10.0, true);
    a.record(6, 6.87, false);
    std::string const s = a.serialize();

    OrbLearning b;
    b.deserialize(s);
    CHECK(b.size() == 2);
    auto r5 = b.lookup(5);
    CHECK(r5.has_value());
    CHECK(r5->vy > -10.4 && r5->vy < -10.2);
    CHECK(r5->flips == true);
    auto r6 = b.lookup(6);
    CHECK(r6.has_value() && r6->vy == 6.87 && r6->flips == false);
}

static void deserialize_skips_garbage_lines_without_failing_the_rest() {
    OrbLearning t;
    t.deserialize("PBUFORBS 1\nnot a valid line\n7 1.5 3 1\n\n");
    CHECK(t.size() == 1);
    auto r = t.lookup(7);
    CHECK(r.has_value() && r->vy == 0.5 && r->flips == false); // vySum/n = 1.5/3; 1 flip of 3 is not a majority
}

int main() {
    unseen_key_is_nullopt();
    records_and_averages();
    flip_is_majority_vote();
    fallback_used_only_when_primary_key_unseen();
    round_trips_through_serialize();
    deserialize_skips_garbage_lines_without_failing_the_rest();
    std::printf("%d checks, %d failed\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
