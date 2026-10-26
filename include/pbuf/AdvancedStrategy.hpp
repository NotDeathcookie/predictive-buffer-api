// Predictive Buffer API - optional strategy: "pbuf.trajectory.advanced".
//
// NEW code (not transliterated from either mod): it wires LegacyCore.hpp's
// `searchDelay()` - which is a strict superset of the search in
// TrajectorySearchStrategy (dense scan right after the click, a CBF timing
// guard, an optional safety margin, and "aim" for Extreme-style stages) -
// onto the existing `IProbe` interface, so it works with every backend the
// built-in strategy already works with. See LegacyCore.hpp for provenance of
// the search algorithm itself (derived from GD-Input-Buffer, AlQuerez, used
// with permission).
//
// Select it with `Config::strategyId = pbuf::AdvancedTrajectoryStrategy::kId`
// and register an instance via `registerStrategy()` / `pbuf::Buffer::registerStrategy()`.
// It is opt-in: the default remains "pbuf.trajectory" (TrajectorySearchStrategy),
// so existing consumers see no behavior change unless they ask for this one.
//
// Header-only, Geode-free.
#pragma once

#include "LegacyCore.hpp"
#include "Strategy.hpp"

namespace pbuf {

/// Tuning specific to the advanced strategy. Lives outside `pbuf::Config` for
/// now (that struct's fields are all tied to the original v4.6 defaults);
/// construct the strategy with the tuning you want, or use the defaults.
struct AdvancedTuning {
    /// CBF timing guard: a held click fires on a step boundary, so prefer a
    /// delay whose next tick is ALSO survivable. Matches TrajectorySearchStrategy's
    /// always-on behavior; set false only if your backend fires exactly on tick.
    bool guard = true;

    /// Extra ticks past the earliest safe delay, if still safe (ignored when
    /// `aim` > 0). 0 = earliest safe delay, matching TrajectorySearchStrategy.
    Tick margin = 0;

    /// 0 = earliest safe delay. >0 (up to 1) = fraction of the way into the
    /// safe range found within the window - this is what a "Stage: Extreme"-
    /// style preset wants (ride the input further into the safe window
    /// instead of firing on the very first tick that clears it), matching
    /// GD-Input-Buffer's Extreme stage behavior.
    float aim = 0.f;

    /// Delays right after the click checked one by one, before falling back to
    /// `stride`. Narrow safe gaps close to the click are common and a coarse
    /// stride can step over them entirely; 0 disables the dense scan.
    int dense = 8;

    /// Coarse step used past the dense region (then bisected down to the exact
    /// earliest Good delay). 1 = no coarse step (check every tick).
    int stride = 4;

    friend bool operator==(AdvancedTuning const&, AdvancedTuning const&) = default;
};

/// Adapts an `IProbe` (the engine's existing survival-probe interface) to the
/// `Eval` callable `legacycore::searchDelay` expects: "is delay d Good /
/// Partial / Bad / out of budget?". A `Partial` verdict (prediction cut short
/// by an orb/portal within the window) isn't something today's `IProbe`
/// reports, so this conservatively treats "reached the horizon" as Good and
/// anything else as Bad - still a correct, if slightly less refined, search.
class ProbeEval {
public:
    ProbeEval(IProbe& probe, bool push, Tick horizon, std::int64_t maxSimTicks)
        : probe_(probe), push_(push), horizon_(horizon), maxSimTicks_(maxSimTicks) {}

    legacycore::Verdict operator()(int delay) {
        if (maxSimTicks_ > 0 && probe_.simulatedTicks() >= maxSimTicks_)
            return legacycore::Verdict::Budget;
        Tick const ticks = probe_.survival(static_cast<Tick>(delay), push_, horizon_);
        return ticks >= horizon_ ? legacycore::Verdict::Good : legacycore::Verdict::Bad;
    }

private:
    IProbe& probe_;
    bool push_;
    Tick horizon_;
    std::int64_t maxSimTicks_;
};

class AdvancedTrajectoryStrategy final : public IBufferStrategy {
public:
    static constexpr char const* kId = "pbuf.trajectory.advanced";

    explicit AdvancedTrajectoryStrategy(AdvancedTuning tuning = {}) : tuning_(tuning) {}

    std::string_view id() const noexcept override { return kId; }

    Decision decide(InputContext const& c, IProbe& probe) override {
        Decision d;
        d.frame0Ticks = probe.survival(0, c.push, c.horizon);
        d.bestTicks = d.frame0Ticks;
        if (d.frame0Ticks >= c.horizon) return d;

        legacycore::SearchParams p;
        p.maxDelay = c.maxDelay;
        p.stride   = tuning_.stride;
        p.guard    = tuning_.guard;
        p.margin   = tuning_.margin;
        p.aim      = tuning_.aim;
        p.minDelay = 1;
        p.dense    = tuning_.dense;

        ProbeEval eval(probe, c.push, c.horizon, c.maxSimTicks);
        legacycore::SearchResult const r = legacycore::searchDelay(p, eval);

        if (r.budget) d.budgetExhausted = true;
        if (r.delay > 0) {
            d.delay = static_cast<Tick>(r.delay);
            d.bestTicks = probe.survival(d.delay, c.push, c.horizon);
        }
        return d;
    }

    AdvancedTuning const& tuning() const { return tuning_; }

private:
    AdvancedTuning tuning_;
};

} // namespace pbuf
