// Predictive Buffer API - observation hooks and counters.
//
// Replaces the original's unconditional printf() calls (and its AllocConsole()
// at load) with an opt-in interface. With no observers registered the engine
// does no formatting and no I/O.
//
// Callbacks run synchronously on the game thread: keep them cheap.
//
// Geode-free. Provenance: new API infrastructure.
#pragma once

#include "Config.hpp"
#include "Types.hpp"

#include <cstdint>

namespace pbuf {

struct BufferedEvent {
    bool push        = true;
    Tick delay       = 0;   ///< ticks the input is held back
    Tick frame0Ticks = 0;   ///< predicted survival without delay
    Tick bestTicks   = 0;   ///< predicted survival with `delay`
    Tick maxDelay    = 0;   ///< limit in force
    bool wave        = false;
};

class IObserver {
public:
    virtual ~IObserver() = default;
    /// An input was held back.
    virtual void onBuffered(BufferedEvent const&) {}
    /// An input went through immediately (reason says why).
    virtual void onPassThrough(bool /*push*/, PassReason) {}
    /// The engine delivered an input to the game (buffered inputs fire later).
    virtual void onFired(bool /*push*/) {}
    /// Configuration or preset changed.
    virtual void onConfigChanged(Config const&) {}
};

/// Cheap monotonic counters; always maintained.
struct Stats {
    std::uint64_t inputs          = 0;  ///< player-1 inputs seen while a level was running
    std::uint64_t buffered        = 0;  ///< inputs that were delayed
    std::uint64_t passed          = 0;  ///< inputs passed through immediately
    std::uint64_t absorbed        = 0;  ///< inputs queued behind an already-delayed input
    std::uint64_t spamBypassed    = 0;
    std::uint64_t fired           = 0;  ///< injections into the game
    std::uint64_t strategyErrors  = 0;
    std::uint64_t budgetExhausted = 0;
    std::uint64_t simTicks        = 0;  ///< total ticks simulated by probes
};

} // namespace pbuf
