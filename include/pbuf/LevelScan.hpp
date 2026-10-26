// Predictive Buffer API - optional module: level-scan search.
//
// NEW code. Modeled on the behavior GD-Input-Buffer (AlQuerez, used with
// permission - see NOTICE.md) documents for its "Level scan" feature, NOT
// transliterated from its source: in that mod the scan is implemented as
// methods on the same $modify(PlayLayer) class as everything else, reading
// GameObject/PlayerObject state directly, so there is no self-contained
// "scan algorithm" file to port the way LegacyCore.hpp ports BufferCore.hpp.
// What follows is an independent implementation of the algorithm its
// changelog describes:
//
//   "It follows up to N different versions of the run at the same time.
//    Every k ticks each version tries both 'keep going' and 'switch'
//    (press / let go); versions that crash are dropped, and the survivors
//    are kept spread over the whole height range. The version that gets
//    furthest with the fewest inputs, staying away from spikes, becomes
//    the route."
//
// This module only does the search over an abstract branch - it has no idea
// what a spike, a portal or a gamemode switch is. A real integration backs
// `IScanBranch` with a GeodeBackend type that can clone a simulated player's
// full state (the thing GD-Input-Buffer calls its "hidden copy of your
// player") and step it forward one tick; that part is NOT included here (see
// "What's still missing" in docs/GD_INPUT_BUFFER_INTEGRATION.md) because it
// needs real Geode/PlayLayer access to write and verify, which this
// environment doesn't have.
//
// Geode-free, header-only, independently unit-tested (tests/level_scan_test.cpp)
// against a synthetic course - it proves the search strategy (branch, prune,
// keep spread, pick furthest/fewest-inputs/most-clearance) works, not that any
// particular game's physics are modeled correctly.
#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

namespace pbuf {

/// One branch of the scan: a clonable, steppable simulated run. Implemented by
/// whatever can actually fork and advance a hidden player (a GeodeBackend, or
/// a synthetic course in tests).
class IScanBranch {
public:
    virtual ~IScanBranch() = default;

    /// Advance exactly one scan tick with the button held in state `pressed`.
    /// Returns false if the branch died on this tick (it is dropped either way
    /// after returning false - do not call step() again on a dead branch).
    virtual bool step(bool pressed) = 0;

    /// Used only to keep prune survivors spread out (e.g. current height),
    /// never to rank branches against each other directly - two branches at
    /// very different heights may both be worth keeping even if one is
    /// "further along", because the course may reward either.
    virtual float spreadMetric() const = 0;

    /// Monotonic-ish measure of how far through the course this branch has
    /// gotten (e.g. x position). Used to pick the winner and to report how
    /// far the best attempt reached when every branch dies.
    virtual float progress() const = 0;

    /// Distance from the nearest hazard right now (bigger = safer). Used only
    /// as a tie-break between otherwise-equal routes ("staying away from
    /// spikes"), never to decide survival.
    virtual float clearance() const = 0;

    /// Independent copy with its own state, able to step on its own from here.
    virtual std::unique_ptr<IScanBranch> clone() const = 0;
};

struct ScanConfig {
    /// Upper bound on how many branches are kept alive at once ("up to 64
    /// different versions" in the original).
    int maxBranches = 64;

    /// Scan ticks between decision points, i.e. how often a branch tries both
    /// "keep" and "switch". The original uses 2 normally and 1 in wave - pass
    /// 1 for a continuous-input gamemode.
    int decisionInterval = 2;

    /// Hard stop so a course with no solution (or a bug in the branch) can't
    /// run forever.
    int maxDecisionPoints = 20000;
};

/// One press/release decision in the winning route, in scan-tick order.
struct ScanInput {
    int  atTick;
    bool pressed;
};

struct ScanResult {
    bool reachedEnd = false;
    /// How far the winning (or, on failure, the best-ever) branch got.
    float bestProgress = 0.f;
    /// Scan ticks the best branch survived (reachedEnd: the whole course).
    int   ticksSurvived = 0;
    /// The winning branch's input sequence (only meaningful if reachedEnd).
    std::vector<ScanInput> route;
    /// How many branches were spawned in total, for diagnostics.
    int   branchesTried = 0;
};

namespace detail {

struct Candidate {
    std::unique_ptr<IScanBranch> branch;
    bool lastPressed = false;
    std::vector<ScanInput> route;
    bool diedThisRound = false;
};

/// Keep at most `keep` candidates, preferring to cover the full spread of
/// `spreadMetric()` rather than just the top-progress ones (the original:
/// "keep the versions with the fewest inputs... they all ended up as
/// near-copies... Now the versions stay spread out"). Implementation: sort by
/// spreadMetric, then take an evenly-spaced sample of that order so the kept
/// set spans the whole range; break ties within a bucket by progress.
inline std::vector<std::size_t> selectSpread(std::vector<Candidate> const& cs, int keep) {
    std::vector<std::size_t> order(cs.size());
    for (std::size_t i = 0; i < order.size(); i++) order[i] = i;
    if (static_cast<int>(order.size()) <= keep) return order;
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return cs[a].branch->spreadMetric() < cs[b].branch->spreadMetric();
    });
    std::vector<std::size_t> picked;
    picked.reserve(static_cast<std::size_t>(keep));
    double const n = static_cast<double>(order.size());
    for (int k = 0; k < keep; k++) {
        double const frac = (keep == 1) ? 0.0 : static_cast<double>(k) / (keep - 1);
        std::size_t idx = static_cast<std::size_t>(frac * (n - 1) + 0.5);
        idx = std::min(idx, order.size() - 1);
        picked.push_back(order[idx]);
    }
    std::sort(picked.begin(), picked.end());
    picked.erase(std::unique(picked.begin(), picked.end()), picked.end());
    return picked;
}

} // namespace detail

/// Runs the branch/prune/select search. `atEnd(branch)` reports whether a
/// branch has reached the course's goal (the caller defines "the end" - a
/// level length, a target x position, a fixed tick count, anything).
template <class EndCheck>
ScanResult runScan(IScanBranch const& start, ScanConfig const& cfg, EndCheck&& atEnd) {
    ScanResult result;
    std::vector<detail::Candidate> live;
    live.push_back({start.clone(), false, {}, false});
    result.branchesTried = 1;

    detail::Candidate const* best = &live.front();
    float bestProgress = start.progress();

    for (int point = 0; point < cfg.maxDecisionPoints && !live.empty(); point++) {
        std::vector<detail::Candidate> children;
        children.reserve(live.size() * 2);

        for (auto& c : live) {
            // "Keep going" and "switch" - unless we're already at the branch cap
            // and this candidate is clearly the weakest, try both.
            bool const tryBoth = static_cast<int>(live.size()) * 2 <= cfg.maxBranches * 4;
            bool const options[2] = {c.lastPressed, !c.lastPressed};
            int const nOptions = tryBoth ? 2 : 1;
            for (int oi = 0; oi < nOptions; oi++) {
                bool const pressed = options[oi];
                auto branch = c.branch->clone();
                bool alive = true;
                for (int t = 0; t < cfg.decisionInterval && alive; t++) alive = branch->step(pressed);
                if (!alive) continue;
                detail::Candidate child;
                child.branch = std::move(branch);
                child.lastPressed = pressed;
                child.route = c.route;
                child.route.push_back(ScanInput{point * cfg.decisionInterval, pressed});
                children.push_back(std::move(child));
                result.branchesTried++;
            }
        }

        if (children.empty()) break; // every branch died this round

        for (auto& c : children) {
            if (c.branch->progress() > bestProgress) {
                bestProgress = c.branch->progress();
            }
        }
        for (auto const& c : children) {
            if (atEnd(*c.branch)) {
                result.reachedEnd = true;
                result.bestProgress = c.branch->progress();
                result.ticksSurvived = (point + 1) * cfg.decisionInterval;
                result.route = c.route;
                return result;
            }
        }

        auto keepIdx = detail::selectSpread(children, cfg.maxBranches);
        std::vector<detail::Candidate> kept;
        kept.reserve(keepIdx.size());
        for (std::size_t i : keepIdx) kept.push_back(std::move(children[i]));
        live = std::move(kept);

        // Remember the single best-progress survivor so a dead-end run can
        // still report "how far the best got" (ticksSurvived uses its route length).
        for (auto& c : live) {
            if (c.branch->progress() >= bestProgress - 0.0001f) {
                result.ticksSurvived = (point + 1) * cfg.decisionInterval;
                result.route = c.route;
            }
        }
    }

    result.bestProgress = bestProgress;
    (void)best;
    return result;
}

} // namespace pbuf
