// Unit tests for pbuf/AdvancedStrategy.hpp (no game needed).
#include "pbuf/AdvancedStrategy.hpp"

#include <cstdio>
#include <vector>

using namespace pbuf;

static int g_checks = 0, g_fail = 0;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_fail; std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

namespace {

// A fake probe whose survival() is driven by a lookup table indexed by delay.
// Index 0 = frame-0 (no delay).
struct TableProbe final : IProbe {
    std::vector<Tick> table;
    int calls = 0;
    std::int64_t sim = 0;
    Tick survival(Tick delay, bool /*push*/, Tick /*horizon*/) override {
        ++calls;
        sim += 1;
        std::size_t const i = static_cast<std::size_t>(delay);
        return i < table.size() ? table[i] : table.back();
    }
    std::int64_t simulatedTicks() const override { return sim; }
};

} // namespace

static void already_safe_picks_delay_zero() {
    TableProbe p;
    p.table = {160, 0, 0, 0, 0};
    AdvancedTrajectoryStrategy s;
    InputContext c; c.push = true; c.maxDelay = 4; c.horizon = 160;
    Decision d = s.decide(c, p);
    CHECK(d.delay == 0);
    CHECK(d.frame0Ticks == 160);
    CHECK(p.calls == 1); // frame-0 check only; search never runs
}

static void finds_earliest_good_delay_with_default_dense_scan() {
    TableProbe p;
    // Delay 3 is the first that clears the horizon.
    p.table = {10, 10, 20, 160, 160, 160};
    AdvancedTrajectoryStrategy s; // default dense=8 covers every delay here
    InputContext c; c.push = true; c.maxDelay = 5; c.horizon = 160;
    Decision d = s.decide(c, p);
    CHECK(d.delay == 3);
    CHECK(d.bestTicks == 160);
}

static void guard_prefers_a_delay_whose_next_tick_also_survives() {
    TableProbe p;
    // Delay 5 survives, but 6 does not; the next pair that BOTH survive is 8/9.
    p.table.assign(16, 10);
    p.table[5] = 160; p.table[6] = 10;
    for (int d = 8; d < 16; d++) p.table[static_cast<std::size_t>(d)] = 160;
    AdvancedTuning t; t.guard = true; t.dense = 16; t.stride = 1;
    AdvancedTrajectoryStrategy s(t);
    InputContext c; c.push = true; c.maxDelay = 15; c.horizon = 160;
    Decision d = s.decide(c, p);
    CHECK(d.delay == 8);
}

static void margin_steps_past_the_deadly_edge_when_still_safe() {
    TableProbe p;
    p.table.assign(20, 10);
    for (int d = 5; d < 20; d++) p.table[static_cast<std::size_t>(d)] = 160;
    AdvancedTuning t; t.guard = false; t.margin = 3; t.dense = 20; t.stride = 1;
    AdvancedTrajectoryStrategy s(t);
    InputContext c; c.push = true; c.maxDelay = 19; c.horizon = 160;
    Decision d = s.decide(c, p);
    CHECK(d.delay == 8); // earliest (5) + margin (3)
}

static void aim_rides_toward_the_far_edge_of_the_safe_range_extreme_style() {
    TableProbe p;
    // Safe window is delays [4, 14]; aim=1.0 should land at the far edge (minus
    // the implicit guard check at the very end, since guard is off here).
    p.table.assign(20, 10);
    for (int d = 4; d <= 14; d++) p.table[static_cast<std::size_t>(d)] = 160;
    AdvancedTuning t; t.guard = false; t.aim = 1.0f; t.dense = 20; t.stride = 2;
    AdvancedTrajectoryStrategy s(t);
    InputContext c; c.push = true; c.maxDelay = 19; c.horizon = 160;
    Decision d = s.decide(c, p);
    CHECK(d.delay >= 10); // well past the earliest-safe delay (4), riding toward 14
    CHECK(d.bestTicks == 160);
}

static void budget_is_reported_when_simulated_ticks_run_out() {
    TableProbe p;
    p.table = {10, 10, 10, 160, 160};
    AdvancedTrajectoryStrategy s;
    InputContext c; c.push = true; c.maxDelay = 4; c.horizon = 160; c.maxSimTicks = 2;
    Decision d = s.decide(c, p);
    CHECK(d.budgetExhausted);
    CHECK(d.delay == 0); // nothing trustworthy was confirmed before the budget ran out
}

static void id_is_stable() {
    AdvancedTrajectoryStrategy s;
    CHECK(s.id() == "pbuf.trajectory.advanced");
}

int main() {
    already_safe_picks_delay_zero();
    finds_earliest_good_delay_with_default_dense_scan();
    guard_prefers_a_delay_whose_next_tick_also_survives();
    margin_steps_past_the_deadly_edge_when_still_safe();
    aim_rides_toward_the_far_edge_of_the_safe_range_extreme_style();
    budget_is_reported_when_simulated_ticks_run_out();
    id_is_stable();
    std::printf("%d checks, %d failed\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
