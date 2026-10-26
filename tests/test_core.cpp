// Unit tests for the Geode-free core. Plain asserts, no framework.
#include "mock.hpp"

#include "Engine.hpp"

#include <pbuf/Presets.hpp>
#include <pbuf/TrajectorySearch.hpp>

#include <cstdio>
#include <functional>

using namespace pbt;
using pbuf::detail::Engine;

static int g_checks = 0, g_fail = 0;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_fail; std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define TEST(name) static void name(); struct name##_reg { name##_reg() { tests().push_back({#name, &name}); } } name##_inst; static void name()
static std::vector<std::pair<const char*, void (*)()>>& tests() { static std::vector<std::pair<const char*, void (*)()>> t; return t; }

namespace {

/// Always delays by a fixed number of ticks: makes buffering decisions deterministic.
struct FixedDelay final : IBufferStrategy {
    Tick d; int calls = 0;
    explicit FixedDelay(Tick delay) : d(delay) {}
    std::string_view id() const noexcept override { return "test.fixed"; }
    Decision decide(InputContext const&, IProbe&) override { ++calls; Decision r; r.delay = d; return r; }
};

struct Rig {
    World w;
    MockBackend be{w};
    Engine eng;
    std::vector<std::pair<int, int>> fires;  // (tick, push)
    int tickNo = 0;
    std::shared_ptr<FixedDelay> strat;

    explicit Rig(Config cfg, Tick fixedDelay = 4) : eng(be, cfg) {
        strat = std::make_shared<FixedDelay>(fixedDelay);
        eng.registerStrategy(strat);
        cfg.strategyId = "test.fixed";
        cfg.spam.enabled = false;
        cfg.limits = {16, 16};
        CHECK(eng.setConfig(cfg).empty());
        GameState g; g.inLevel = true; g.p1 = Mode::Wave;   // wave: releases are bufferable
        w.gs = g;
        eng.onLevelEnter();
    }
    void tick() { eng.onTick(); ++tickNo; sync(); }
    void ticks(int n) { while (n--) tick(); }
    bool in(bool push, bool p1 = true) { bool r = eng.onInput(push, p1); sync(); return r; }
    void sync() { for (std::size_t i = fires.size(); i < w.log.size(); ++i) fires.push_back({tickNo, w.log[i].a}); }
    std::string seq() const { std::string s; for (auto [t, p] : fires) s += std::string(p ? "P" : "R") + "@" + std::to_string(t) + " "; return s; }
};

} // namespace

TEST(config_valid_and_invalid) {
    CHECK(pbuf::Config::classic().validate().empty());
    CHECK(pbuf::Config::recommended().validate().empty());
    pbuf::Config c; c.limits.normal = -1; c.horizons.wave = 0; c.spam.streakRequired = 0; c.strategyId.clear();
    auto e = c.validate();
    CHECK(e.size() == 4);
    pbuf::Config d; d.limits.wave = pbuf::Config::kMaxDelayTicks + 1;
    CHECK(d.validate().size() == 1);
}

TEST(classic_differs_from_recommended_only_in_documented_switches) {
    auto a = pbuf::Config::classic(), b = pbuf::Config::recommended();
    CHECK(a.behavior.queue == QueueMode::LegacySingleSlot && b.behavior.queue == QueueMode::OrderedSpacing);
    CHECK(a.behavior.failsafe == FailsafePolicy::MatchPhysical && b.behavior.failsafe == FailsafePolicy::ReleaseOnly);
    CHECK(a.behavior.dualProbe == DualProbe::AlwaysMirror && b.behavior.dualProbe == DualProbe::LinkedOnly);
    CHECK(!a.rules.bypassPlatformer && b.rules.bypassPlatformer);
    b.behavior = a.behavior; b.rules.bypassPlatformer = false;
    CHECK(a == b);
}

TEST(conversions) {
    CHECK(ticksFromFrames60(1.0) == 4);
    CHECK(ticksFromFrames60(0.5) == 2);
    CHECK(ticksFromFrames60(1.0, 480) == 8);
    CHECK(ticksFromMillis(1000.0) == 240);
    CHECK(ticksFromMillis(16.667) == 4);
}

TEST(presets_match_original_f6_cycle) {
    PresetBook b;
    CHECK(b.current()->id == "classic-1");
    // v4.6: first F6 press goes to stage 2; order 2/2 -> 4/4 -> 8/8 -> 16/8 -> OFF -> 2/2
    CHECK(b.next()->limits.normal == 4);
    CHECK(b.next()->limits.normal == 8);
    auto p4 = b.next(); CHECK(p4->limits.normal == 16 && p4->limits.wave == 8);
    auto off = b.next(); CHECK(!off->enabled);
    CHECK(b.next()->id == "classic-1");
    CHECK(presets::extended().size() == 10);
    CHECK(b.select("extended-5") == nullptr);
    PresetBook e(presets::extended());
    CHECK(e.select("extended-5") && e.current()->limits.normal == 60);
    Config c; e.select("off"); e.current()->applyTo(c); CHECK(!c.enabled);
    e.select("classic-3"); e.current()->applyTo(c); CHECK(c.enabled && c.limits.normal == 8);
}

TEST(legacy_loses_release_in_press_release_press_burst__ordered_keeps_it) {
    // Characterization of the v4.6 defect (docs/ORIGINAL_ANALYSIS.md #7) and of the fix.
    auto burst = [](Rig& r) {
        CHECK(!r.in(true));    // press, held back 4 ticks
        r.ticks(2);
        CHECK(!r.in(false));   // release arrives inside the window
        r.ticks(1);
        CHECK(!r.in(true));    // press again inside the window
        r.ticks(12);
    };
    Config legacy = Config::classic();  legacy.rules.releaseMatters = {Mode::Wave};
    Rig a(legacy); burst(a);
    CHECK(a.seq() == "P@5 ");                 // the release is gone: one long press instead of tap+tap

    Config ordered = Config::recommended();
    Rig b(ordered); burst(b);
    CHECK(b.seq() == "P@5 R@7 P@8 ");         // same order, original 2-tick and 1-tick spacing kept
}

TEST(ordered_delays_every_input_in_burst_by_the_same_amount) {
    Rig r(Config::recommended(), 3);
    CHECK(!r.in(true));  r.ticks(1);
    CHECK(!r.in(false)); r.ticks(5);
    CHECK(r.seq() == "P@4 R@5 ");
    // queue empty again: next input is evaluated afresh (and delayed again by the strategy)
    CHECK(!r.in(true)); r.ticks(5);
    CHECK(r.seq() == "P@4 R@5 P@10 ");
}

TEST(ordered_queue_overflow_flushes_in_order_and_passes_current_input) {
    Rig r(Config::recommended(), 10);
    CHECK(!r.in(true));
    bool pass = false;
    for (int i = 0; i < 40 && !pass; ++i) pass = r.in(i % 2 == 0 ? false : true);
    CHECK(pass);
    CHECK(r.eng.stats().absorbed == 31);          // capacity 32 incl. the first
    CHECK(r.fires.size() == 32);                  // everything queued was delivered, in order
    CHECK(r.fires[0].second == 1 && r.fires[1].second == 0 && r.fires[2].second == 1);
    CHECK(!r.eng.pending());
}

TEST(player2_is_swallowed_only_while_linked_and_busy) {
    Rig r(Config::recommended());
    GameState g = r.w.gs; g.dual = true; g.twoPlayerMode = false; r.w.gs = g;
    CHECK(r.in(false, false));                   // nothing pending: let through
    CHECK(!r.in(true));                          // P1 held back
    CHECK(!r.in(true, false));                   // linked P2 copy is swallowed too
    g.twoPlayerMode = true; r.w.gs = g;
    CHECK(r.in(true, false));                    // independent P2 is never swallowed
}

TEST(platformer_bypass_new_in_recommended_ignored_by_classic) {
    Rig a(Config::recommended()); GameState g = a.w.gs; g.platformer = true; a.w.gs = g;
    CHECK(a.in(true)); CHECK(a.strat->calls == 0);
    Config c = Config::classic(); c.rules.releaseMatters = {Mode::Wave};
    Rig b(c); g = b.w.gs; g.platformer = true; b.w.gs = g;
    CHECK(!b.in(true)); CHECK(b.strat->calls == 1);
}

TEST(ship_bypass_delivers_pending_inputs_in_ordered_mode) {
    Rig r(Config::recommended(), 6);
    CHECK(!r.in(true));
    GameState g = r.w.gs; g.p1 = Mode::Ship; r.w.gs = g;   // portal into ship while a press is held back
    r.tick();
    CHECK(r.seq() == "P@1 ");
    CHECK(!r.eng.pending());
    CHECK(r.in(false));                                    // ship: passes straight through
}

TEST(ship_bypass_in_legacy_drops_then_matches_physical_like_v46) {
    Config c = Config::classic(); c.rules.releaseMatters = {Mode::Wave};
    Rig r(c, 6);
    CHECK(!r.in(true));
    GameState g = r.w.gs; g.p1 = Mode::Ship; r.w.gs = g;
    r.tick();
    CHECK(r.seq() == "P@1 ");       // physical is still held => resynced by one press
}

TEST(death_clears_pending_and_streak) {
    Rig r(Config::recommended(), 6);
    CHECK(!r.in(true));
    r.eng.onPlayerDied();
    CHECK(!r.eng.pending());
    r.ticks(10);
    CHECK(r.fires.empty());
}

TEST(failsafe_policies_after_respawn_while_still_holding) {
    // The player keeps holding through a death. The game side is reset by the death,
    // the finger is still down: only MatchPhysical (v4.6) injects a phantom press.
    auto phantomPresses = [](FailsafePolicy pol) {
        Config c = Config::recommended(); c.behavior.failsafe = pol;
        Rig r(c, 0);
        GameState g = r.w.gs; g.p1 = Mode::Other; r.w.gs = g;
        r.in(true);                // delay 0: passes through, game holding
        r.eng.onPlayerDied();      // game reset, finger still down
        r.ticks(3);
        return r.fires.size();
    };
    CHECK(phantomPresses(FailsafePolicy::None) == 0);
    CHECK(phantomPresses(FailsafePolicy::ReleaseOnly) == 0);
    CHECK(phantomPresses(FailsafePolicy::MatchPhysical) == 1);
}

TEST(strategy_returning_invalid_delay_is_rejected) {
    Rig r(Config::recommended(), 99);            // limit is 16
    CHECK(r.in(true));
    CHECK(r.eng.stats().strategyErrors == 1);
    CHECK(!r.eng.pending());
}

TEST(setConfig_errors_do_not_change_state) {
    Rig r(Config::recommended());
    Config before = r.eng.config();
    Config bad = before; bad.limits.normal = -5;
    CHECK(!r.eng.setConfig(bad).empty());
    Config unk = before; unk.strategyId = "nope.nothing";
    auto e = r.eng.setConfig(unk);
    CHECK(e.find("nope.nothing") != std::string::npos && e.find("pbuf.trajectory") != std::string::npos);
    CHECK(r.eng.config() == before);
}

TEST(setConfig_delivers_pending_input_instead_of_dropping_it) {
    Rig r(Config::recommended(), 8);
    CHECK(!r.in(true));
    Config c = r.eng.config(); c.limits = {4, 4};
    CHECK(r.eng.setConfig(c).empty());
    r.sync();
    CHECK(r.seq() == "P@0 ");
    CHECK(!r.eng.pending());
}

TEST(trajectory_strategy_branches) {
    struct Fake : IProbe {
        std::vector<Tick> t; mutable int calls = 0; std::int64_t sim = 0; Tick per = 10;
        Tick survival(Tick delay, bool, Tick) override { ++calls; sim += per; return t[static_cast<std::size_t>(delay)]; }
        std::int64_t simulatedTicks() const override { return sim; }
    };
    TrajectorySearchStrategy s;
    InputContext c; c.push = true; c.maxDelay = 4; c.horizon = 160;
    { Fake f; f.t = {160, 0, 0, 0, 0}; auto d = s.decide(c, f); CHECK(d.delay == 0 && f.calls == 1); }       // already safe
    { Fake f; f.t = {20, 30, 160, 160, 160}; auto d = s.decide(c, f); CHECK(d.delay == 2 && f.calls == 3); }   // earliest full clear
    { Fake f; f.t = {20, 25, 40, 30, 20}; auto d = s.decide(c, f); CHECK(d.delay == 2 && d.bestTicks == 40); } // partial: beats best by >12
    { Fake f; f.t = {20, 25, 30, 32, 20}; auto d = s.decide(c, f); CHECK(d.delay == 0); }                      // no meaningful gain
    // wave: gaining only what a flat corridor gives for free (2*delay) must not count
    InputContext w = c; w.wave = true; w.horizon = 65;
    { Fake f; f.t = {10, 12, 14, 16, 18}; auto d = s.decide(w, f); CHECK(d.delay == 0); }
    { Fake f; f.t = {10, 12, 65, 65, 65}; auto d = s.decide(w, f); CHECK(d.delay == 2); }                      // 65-10 >= 2*2+6
    // budget
    InputContext b = c; b.maxSimTicks = 25;
    { Fake f; f.t = {20, 20, 20, 160, 160}; auto d = s.decide(b, f); CHECK(d.budgetExhausted && d.delay == 0 && f.calls == 3); }
}

TEST(observers_receive_events_and_are_optional) {
    struct Obs final : IObserver {
        int buffered = 0, passed = 0, fired = 0, cfg = 0; PassReason last{};
        void onBuffered(BufferedEvent const& e) override { ++buffered; CHECK(e.delay == 4); }
        void onPassThrough(bool, PassReason r) override { ++passed; last = r; }
        void onFired(bool) override { ++fired; }
        void onConfigChanged(Config const&) override { ++cfg; }
    };
    Rig r(Config::recommended(), 4);
    auto o = std::make_shared<Obs>(); r.eng.addObserver(o);
    CHECK(!r.in(true)); r.ticks(5);
    CHECK(o->buffered == 1 && o->fired == 1);
    GameState g = r.w.gs; g.p1 = Mode::Ship; r.w.gs = g;
    CHECK(r.in(true)); CHECK(o->passed == 1 && o->last == PassReason::ModeBypass);
    r.eng.setConfig(r.eng.config()); CHECK(o->cfg == 1);
}

TEST(disabled_engine_never_touches_input) {
    Config c = Config::recommended(); c.enabled = false;
    Rig r(c, 4);
    r.eng.setConfig([&] { Config x = r.eng.config(); x.enabled = false; return x; }());
    for (int i = 0; i < 10; ++i) CHECK(r.in(i % 2 == 0));
    r.ticks(10);
    CHECK(r.fires.empty() && r.strat->calls == 0);
}

TEST(reentrant_fire_is_ignored) {
    // While the engine is delivering a held-back input the game calls handleButton again;
    // Engine::onInput must let that through untouched.
    struct Reenter final : pbuf::detail::IBackend {
        World& w; Engine* e = nullptr; MockProbe p; int reentered = 0;
        explicit Reenter(World& world) : w(world) { p.w = &world; }
        bool query(GameState& o) override { o = w.gs; return true; }
        IProbe& beginProbe(InputContext const&, GameState const&) override { return p; }
        void endProbe() override {}
        void fire(bool push) override { if (e->isFiring() && e->onInput(push, true)) ++reentered; }
    };
    World w; w.gs.inLevel = true; w.gs.p1 = Mode::Wave;
    Reenter be(w);
    Config c = Config::recommended(); c.spam.enabled = false;
    Engine eng(be, c); be.e = &eng;
    auto s = std::make_shared<FixedDelay>(1); eng.registerStrategy(s);
    c.strategyId = "test.fixed"; CHECK(eng.setConfig(c).empty());
    eng.onLevelEnter();
    CHECK(!eng.onInput(true, true));
    eng.onTick(); eng.onTick();
    CHECK(be.reentered == 1);
    CHECK(s->calls == 1);
}

TEST(unregister_active_strategy_falls_back_to_builtin) {
    Rig r(Config::recommended());
    CHECK(r.eng.config().strategyId == "test.fixed");
    r.eng.unregisterStrategy("test.fixed");
    CHECK(r.eng.config().strategyId == "pbuf.trajectory");
    r.eng.unregisterStrategy("pbuf.trajectory");          // built-in is permanent
    CHECK(r.eng.hasStrategy("pbuf.trajectory"));
}

int main() {
    for (auto& [name, fn] : tests()) {
        int before = g_fail;
        fn();
        std::printf("%s %s\n", g_fail == before ? "[ ok ]" : "[FAIL]", name);
    }
    std::printf("%d checks, %d failed\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
