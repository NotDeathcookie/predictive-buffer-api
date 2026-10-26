// Predictive Buffer API - configuration.
//
// Geode-free. Plain data + validation, safe to copy across mod boundaries.
//
// Provenance: structure is new. The DEFAULT VALUES and heuristics constants
// come from Predictive Buffer v4.6 `main.cpp` (author field in its mod.json:
// "lilWC") and are reproduced by Config::classic(). See NOTICE.md.
#pragma once

#include "Types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace pbuf {

/// Max extra delay (in ticks) an input may be held back, per input class.
struct DelayLimits {
    Tick normal = 2;  ///< every mode except wave
    Tick wave   = 2;  ///< wave and mini wave (v4.6 used this for both)
    friend bool operator==(DelayLimits const&, DelayLimits const&) = default;
};

/// How far ahead (ticks) a prediction looks. v4.6: 160 / 65 / 50.
struct Horizons {
    Tick normal   = 160;
    Tick wave     = 65;
    Tick miniWave = 50;
    friend bool operator==(Horizons const&, Horizons const&) = default;
};

/// Heuristic constants of the default "pbuf.trajectory" strategy.
/// v4.6 hard-coded all of these inline in interceptInput().
struct SearchTuning {
    // Wave: a delayed turn in a corridor trivially gains ~2 ticks per tick of delay
    // against the opposite wall, so a delay only counts if it gains MORE than that.
    Tick waveGainPerDelay  = 2;   ///< "2 * delay"
    Tick waveFullClearBase = 6;   ///< reaches horizon and gains >= 2*delay + 6
    Tick wavePartialBase   = 10;  ///< otherwise must gain >= 2*delay + 10 ...
    Tick waveImproveMargin = 2;   ///< ... and beat the best so far by > 2
    // Everything else: accept a delay that reaches the horizon, else one that
    // beats the best so far by more than this.
    Tick improveMargin     = 12;
    friend bool operator==(SearchTuning const&, SearchTuning const&) = default;
};

/// Rapid-input streak guard. If the player performs `streakRequired` inputs in a
/// row, each less than `intervalTicks` after the previous one, buffering is
/// bypassed for those modes (spam is by definition not "a click that was early").
struct SpamGuard {
    bool    enabled        = true;
    Tick    intervalTicks  = 8;
    int     streakRequired = 3;
    ModeSet appliesTo      = {Mode::Wave};
    friend bool operator==(SpamGuard const&, SpamGuard const&) = default;
};

/// Which gamemodes get which treatment. This is the "special-case handling"
/// that v4.6 expressed as scattered if-statements.
struct ModeRules {
    ModeSet bypass         = {Mode::Ship};                ///< never touch (any active player in set)
    ModeSet releaseMatters = {Mode::Wave, Mode::Robot};   ///< releases change physics => bufferable
    ModeSet continuous     = {Mode::Wave, Mode::Ufo, Mode::Swing}; ///< no "settled after landing" early-out
    ModeSet waveLike       = {Mode::Wave};                ///< use wave limits, horizons and heuristics
    /// Platformer levels: horizontal movement depends on left/right buttons the
    /// predictor does not model. v4.6 did NOT special-case this (see ORIGINAL_ANALYSIS.md).
    bool    bypassPlatformer = true;
    friend bool operator==(ModeRules const&, ModeRules const&) = default;
};

/// Behavior switches where this library deliberately differs from v4.6.
/// Config::classic() sets every one of them to the v4.6 behavior.
struct Behavior {
    QueueMode      queue          = QueueMode::OrderedSpacing;
    FailsafePolicy failsafe       = FailsafePolicy::ReleaseOnly;
    DualProbe      dualProbe      = DualProbe::LinkedOnly;
    bool           bufferReleases = true;  ///< false: only presses are ever delayed
    friend bool operator==(Behavior const&, Behavior const&) = default;
};

/// Optional CPU guard. v4.6 had none (and the README of a competing project
/// credits that for working on very heavy levels), so the default is "unlimited".
struct Budget {
    /// Stop scanning further delays once this many ticks were simulated for ONE
    /// input (frame-0 check included). 0 = unlimited.
    std::int64_t maxSimTicksPerInput = 0;
    friend bool operator==(Budget const&, Budget const&) = default;
};

struct Config {
    bool enabled = true;

    DelayLimits  limits;
    Horizons     horizons;
    SearchTuning tuning;
    SpamGuard    spam;
    ModeRules    rules;
    Behavior     behavior;
    Budget       budget;

    /// Used only for conversions/diagnostics; the engine itself counts ticks.
    int ticksPerSecond = 240;

    /// Id of the strategy to use ("pbuf.trajectory" = the original algorithm).
    std::string strategyId = "pbuf.trajectory";

    /// Reproduces Predictive Buffer v4.6 exactly (verified by the differential
    /// test in tests/differential_test.cpp). Includes its known defects.
    static Config classic() {
        Config c;
        c.rules.bypassPlatformer = false;
        c.behavior.queue     = QueueMode::LegacySingleSlot;
        c.behavior.failsafe  = FailsafePolicy::MatchPhysical;
        c.behavior.dualProbe = DualProbe::AlwaysMirror;
        return c;
    }

    /// classic() plus the documented fixes. Recommended for new mods.
    static Config recommended() { return Config{}; }

    /// Returns human-readable problems; empty means valid.
    std::vector<std::string> validate() const {
        std::vector<std::string> e;
        auto bad = [&](std::string s) { e.push_back(std::move(s)); };
        if (limits.normal < 0 || limits.normal > kMaxDelayTicks)
            bad("limits.normal must be in [0, " + std::to_string(kMaxDelayTicks) + "], got " + std::to_string(limits.normal));
        if (limits.wave < 0 || limits.wave > kMaxDelayTicks)
            bad("limits.wave must be in [0, " + std::to_string(kMaxDelayTicks) + "], got " + std::to_string(limits.wave));
        auto checkHorizon = [&](char const* name, Tick v) {
            if (v < 1 || v > kMaxHorizonTicks)
                bad(std::string(name) + " must be in [1, " + std::to_string(kMaxHorizonTicks) + "], got " + std::to_string(v));
        };
        checkHorizon("horizons.normal", horizons.normal);
        checkHorizon("horizons.wave", horizons.wave);
        checkHorizon("horizons.miniWave", horizons.miniWave);
        if (spam.intervalTicks < 0) bad("spam.intervalTicks must be >= 0");
        if (spam.streakRequired < 1) bad("spam.streakRequired must be >= 1");
        if (budget.maxSimTicksPerInput < 0) bad("budget.maxSimTicksPerInput must be >= 0 (0 = unlimited)");
        if (ticksPerSecond < 1) bad("ticksPerSecond must be >= 1");
        if (strategyId.empty()) bad("strategyId must not be empty");
        return e;
    }

    static constexpr Tick kMaxDelayTicks   = 480;  ///< 2 s at 240 TPS; far beyond any sane buffer
    static constexpr Tick kMaxHorizonTicks = 1024; ///< bounds per-input simulation cost

    friend bool operator==(Config const&, Config const&) = default;
};

} // namespace pbuf
