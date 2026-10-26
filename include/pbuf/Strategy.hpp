// Predictive Buffer API - strategy extension point.
//
// A strategy answers ONE question: "the player just pressed/released; how many
// ticks (0..maxDelay) should that input be held back?". It is given a probe it
// can ask "if this input were applied `delay` ticks from now, how many ticks
// would the player survive?". It never touches the game directly, which is what
// makes strategies unit-testable with a fake probe.
//
// Geode-free. Provenance: new interface; the default implementation
// (TrajectorySearch.hpp) is derived from Predictive Buffer v4.6.
#pragma once

#include "Config.hpp"
#include "Types.hpp"

#include <cstdint>
#include <string_view>

namespace pbuf {

/// Everything a strategy may know about the input being decided.
struct InputContext {
    bool push        = true;   ///< true = press, false = release
    bool dual        = false;  ///< dual mode active
    bool wave        = false;  ///< any active player is wave-like (see ModeRules::waveLike)
    bool miniWave    = false;
    Tick maxDelay    = 0;      ///< upper bound for Decision::delay (already resolved from DelayLimits)
    Tick horizon     = 0;      ///< how far ahead a probe may usefully look
    SearchTuning tuning;       ///< heuristic constants from Config
    std::int64_t maxSimTicks = 0; ///< Budget::maxSimTicksPerInput, 0 = unlimited
};

/// Predicts the future of the real player(s). Implemented by the game backend.
class IProbe {
public:
    virtual ~IProbe() = default;

    /// Ticks the player survives after `push` is applied `delay` ticks from now,
    /// counted from the moment the input is applied, capped at `horizon`.
    /// A result >= horizon means "survives the whole lookahead".
    /// Before `delay`, the probe holds the button in the OPPOSITE state of `push`
    /// (v4.6 semantics), so delay 0 and delay N are directly comparable.
    virtual Tick survival(Tick delay, bool push, Tick horizon) = 0;

    /// Total ticks simulated so far for the current input (for budgeting).
    virtual std::int64_t simulatedTicks() const = 0;
};

struct Decision {
    Tick delay       = 0;   ///< 0 = let the input through untouched; else 1..maxDelay
    Tick frame0Ticks = 0;   ///< diagnostics: survival with no delay
    Tick bestTicks   = 0;   ///< diagnostics: survival with the chosen delay
    bool budgetExhausted = false;
};

class IBufferStrategy {
public:
    virtual ~IBufferStrategy() = default;

    /// Stable id used in Config::strategyId. Convention: "<mod-id>.<name>".
    virtual std::string_view id() const noexcept = 0;

    /// Called on the game thread for every input that reaches the decision stage
    /// (i.e. not already bypassed). Must be fast, must not throw, and must not
    /// retain `probe` beyond the call. A returned delay outside [0, maxDelay]
    /// is rejected by the engine and the input is passed through.
    virtual Decision decide(InputContext const& ctx, IProbe& probe) = 0;
};

} // namespace pbuf
