// INTERNAL. The seam between the game-agnostic Engine and a game backend.
// Implemented by src/geode/GeodeBackend.* for the real game and by a mock in tests/.
// Not part of the public API; may change in any release.
#pragma once

#include <pbuf/Strategy.hpp>
#include <pbuf/Types.hpp>

namespace pbuf::detail {

/// A snapshot of just the game facts the engine needs. Cheap to fill.
struct GameState {
    bool inLevel       = false;  ///< a PlayLayer exists (editor does not count)
    bool dual          = false;  ///< dual mode active and player 2 exists
    bool twoPlayerMode = false;  ///< level uses two-player control
    bool platformer    = false;
    bool p1Dead        = false;
    Mode p1            = Mode::Other;
    Mode p2            = Mode::Other;   ///< valid only when `dual`
    bool p1Mini        = false;
    bool p2Mini        = false;

    /// One-player dual: the game feeds both icons from one button.
    bool linked() const { return dual && !twoPlayerMode; }
};

class IBackend {
public:
    virtual ~IBackend() = default;

    /// Fill `out`. Return false if the game is not in a state where buffering applies.
    virtual bool query(GameState& out) = 0;

    /// Prepare a probe for one input. The returned reference stays valid until endProbe().
    virtual IProbe& beginProbe(InputContext const& ctx, GameState const& state) = 0;
    virtual void endProbe() = 0;

    /// Deliver a (possibly delayed) input to the game: player 1, plus player 2 when linked.
    /// The backend must not call back into Engine::onInput for this delivery
    /// (the Engine also guards against re-entry).
    virtual void fire(bool push) = 0;
};

} // namespace pbuf::detail
