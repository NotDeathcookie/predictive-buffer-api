// Unit tests for pbuf/LevelScan.hpp against a synthetic course (no game needed).
#include "pbuf/LevelScan.hpp"

#include <cstdio>
#include <memory>
#include <vector>

using namespace pbuf;

static int g_checks = 0, g_fail = 0;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_fail; std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

namespace {

// require: -1 = must be released, 0 = either, 1 = must be pressed, over [lo, hi).
struct Rule { int lo, hi, require; };

class SyntheticBranch final : public IScanBranch {
public:
    explicit SyntheticBranch(std::vector<Rule> const* r) : rules_(r) {}

    bool step(bool pressed) override {
        for (Rule const& r : *rules_) {
            if (tick_ >= r.lo && tick_ < r.hi) {
                if (r.require > 0 && !pressed) return false;
                if (r.require < 0 && pressed) return false;
            }
        }
        tick_++;
        return true;
    }
    float spreadMetric() const override { return static_cast<float>(tick_); }
    float progress() const override { return static_cast<float>(tick_); }
    float clearance() const override { return 1.f; }
    std::unique_ptr<IScanBranch> clone() const override { return std::make_unique<SyntheticBranch>(*this); }

    int tick() const { return tick_; }

private:
    std::vector<Rule> const* rules_;
    int tick_ = 0;
};

// Replays a route against the rules exactly the way the scanner would have
// applied it (held for `decisionInterval` ticks per entry), to independently
// verify a "reached end" result actually is a valid, non-cheating route.
bool replayIsValid(std::vector<Rule> const& rules, std::vector<ScanInput> const& route,
                    int decisionInterval, int courseLen) {
    SyntheticBranch b(&rules);
    for (ScanInput const& in : route) {
        for (int t = 0; t < decisionInterval; t++) {
            if (!b.step(in.pressed)) return false;
        }
    }
    return b.tick() >= courseLen;
}

} // namespace

static void finds_a_route_through_alternating_press_release_windows() {
    // 0-10 release, 10-20 press, 20-30 either, 30-40 release, 40-50 press, 50-60 either.
    std::vector<Rule> rules = {
        {0, 10, -1}, {10, 20, 1}, {20, 30, 0}, {30, 40, -1}, {40, 50, 1}, {50, 60, 0},
    };
    SyntheticBranch start(&rules);
    ScanConfig cfg; cfg.maxBranches = 64; cfg.decisionInterval = 2; cfg.maxDecisionPoints = 100;
    ScanResult r = runScan(start, cfg, [](IScanBranch const& b) { return b.progress() >= 60.f; });

    CHECK(r.reachedEnd);
    CHECK(r.bestProgress >= 60.f);
    CHECK(!r.route.empty());
    CHECK(replayIsValid(rules, r.route, cfg.decisionInterval, 60));
}

static void an_impossible_course_is_reported_as_not_reached_with_honest_progress() {
    // Tick 20 requires both pressed AND released at once: unsatisfiable.
    std::vector<Rule> rules = {
        {20, 21, 1}, {20, 21, -1},
    };
    SyntheticBranch start(&rules);
    ScanConfig cfg; cfg.maxBranches = 32; cfg.decisionInterval = 2; cfg.maxDecisionPoints = 50;
    ScanResult r = runScan(start, cfg, [](IScanBranch const& b) { return b.progress() >= 60.f; });

    CHECK(!r.reachedEnd);
    CHECK(r.bestProgress >= 19.f && r.bestProgress < 22.f); // dies right at tick 20, every branch
    CHECK(r.branchesTried > 1);
}

static void a_narrow_one_tick_window_is_still_found_despite_a_coarse_branch_cap() {
    // Only tick 7 requires press; everything else is free. With a small branch
    // cap this still has to be found via the spread-preserving prune, not luck.
    std::vector<Rule> rules = { {7, 8, 1} };
    SyntheticBranch start(&rules);
    ScanConfig cfg; cfg.maxBranches = 8; cfg.decisionInterval = 1; cfg.maxDecisionPoints = 30;
    ScanResult r = runScan(start, cfg, [](IScanBranch const& b) { return b.progress() >= 20.f; });

    CHECK(r.reachedEnd);
    CHECK(replayIsValid(rules, r.route, cfg.decisionInterval, 20));
}

static void fewer_branches_are_spawned_once_the_live_set_is_already_at_cap() {
    std::vector<Rule> rules = {}; // trivial course: anything survives
    SyntheticBranch start(&rules);
    ScanConfig cfg; cfg.maxBranches = 4; cfg.decisionInterval = 1; cfg.maxDecisionPoints = 10;
    ScanResult r = runScan(start, cfg, [](IScanBranch const& b) { return b.progress() >= 10.f; });
    CHECK(r.reachedEnd);
    // Branch count should stay bounded rather than doubling every round forever
    // (2^10 would be 1024 branches tried; the cap should keep it far below that).
    CHECK(r.branchesTried < 100);
}

int main() {
    finds_a_route_through_alternating_press_release_windows();
    an_impossible_course_is_reported_as_not_reached_with_honest_progress();
    a_narrow_one_tick_window_is_still_found_despite_a_coarse_branch_cap();
    fewer_branches_are_spawned_once_the_live_set_is_already_at_cap();
    std::printf("%d checks, %d failed\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
