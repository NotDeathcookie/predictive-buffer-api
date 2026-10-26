// Predictive Buffer API - default strategy: "pbuf.trajectory".
//
// DERIVED WORK. The decision procedure below is a re-expression of steps 4 and
// 5 of `TrajectoryBufferSim::interceptInput()` in Predictive Buffer v4.6
// (mod id `ai.predictivebuffer`, developer field "lilWC"). The control flow and
// the numeric heuristics are the original's; the constants are now configurable
// (SearchTuning) and the simulation is behind IProbe. The original archive
// contains no license; see NOTICE.md and docs/LICENSING_REVIEW.md before
// redistributing this file.
//
// Header-only on purpose, so custom strategies can wrap or compose it.
#pragma once

#include "Strategy.hpp"

namespace pbuf {

class TrajectorySearchStrategy final : public IBufferStrategy {
public:
    static constexpr char const* kId = "pbuf.trajectory";

    std::string_view id() const noexcept override { return kId; }

    Decision decide(InputContext const& c, IProbe& probe) override {
        Decision d;
        Tick const horizon = c.horizon;

        // Frame 0: if the input as pressed already survives the whole lookahead, done.
        d.frame0Ticks = probe.survival(0, c.push, horizon);
        d.bestTicks   = d.frame0Ticks;
        if (d.frame0Ticks >= horizon) return d;

        // Otherwise scan +1..maxDelay for the earliest delay that genuinely clears the obstacle.
        SearchTuning const& t = c.tuning;
        for (Tick delay = 1; delay <= c.maxDelay; ++delay) {
            if (c.maxSimTicks > 0 && probe.simulatedTicks() >= c.maxSimTicks) {
                d.budgetExhausted = true;
                break;
            }
            Tick const ticks = probe.survival(delay, c.push, horizon);

            if (c.wave) {
                // Delaying a wave turn in a flat corridor naturally adds ~(2 * delay) ticks against
                // the opposite wall, so require extra gain before believing the delay "cleared" something.
                Tick const gain = delay * t.waveGainPerDelay;
                if (ticks >= horizon && (ticks - d.frame0Ticks) >= gain + t.waveFullClearBase) {
                    d.delay = delay;
                    d.bestTicks = ticks;
                    break;
                }
                if (ticks >= d.frame0Ticks + gain + t.wavePartialBase && ticks > d.bestTicks + t.waveImproveMargin) {
                    d.delay = delay;
                    d.bestTicks = ticks;
                    if (ticks >= horizon) break;
                }
            } else {
                if (ticks >= horizon) {
                    d.delay = delay;
                    d.bestTicks = ticks;
                    break;
                }
                if (ticks > d.bestTicks + t.improveMargin) {
                    d.delay = delay;
                    d.bestTicks = ticks;
                }
            }
        }
        return d;
    }
};

} // namespace pbuf
