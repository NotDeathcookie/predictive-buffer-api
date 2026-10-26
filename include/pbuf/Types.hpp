// Predictive Buffer API - vocabulary types.
//
// This header has NO Geode dependency, so it can be used from strategies,
// unit tests and tools without pulling in the game SDK.
//
// Provenance: new API infrastructure. See NOTICE.md.
#pragma once

#include <cstdint>
#include <initializer_list>

namespace pbuf {

/// One physics step of the game. At the stock 240 TPS, 1 tick = 1/240 s and
/// one 60 fps frame = 4 ticks. Everything in this library is measured in ticks;
/// use ticksFromFrames60() / ticksFromMillis() to convert.
using Tick = std::int32_t;

/// Gamemodes as far as buffering rules are concerned. Ball, spider and cube are
/// deliberately folded into `Other`: the original implementation never
/// distinguished them, and the backend is only required to detect these five.
enum class Mode : std::uint8_t { Other, Ship, Ufo, Wave, Robot, Swing, Count_ };

/// Tiny constexpr bit set over Mode (allocation-free, trivially copyable).
class ModeSet {
public:
    constexpr ModeSet() = default;
    constexpr ModeSet(std::initializer_list<Mode> modes) {
        for (Mode m : modes) bits_ = static_cast<std::uint8_t>(bits_ | bit(m));
    }
    constexpr bool has(Mode m) const { return (bits_ & bit(m)) != 0; }
    constexpr ModeSet& add(Mode m) { bits_ = static_cast<std::uint8_t>(bits_ | bit(m)); return *this; }
    constexpr ModeSet& remove(Mode m) { bits_ = static_cast<std::uint8_t>(bits_ & ~bit(m)); return *this; }
    constexpr bool empty() const { return bits_ == 0; }
    friend constexpr bool operator==(ModeSet a, ModeSet b) { return a.bits_ == b.bits_; }

private:
    static constexpr std::uint8_t bit(Mode m) { return static_cast<std::uint8_t>(1u << static_cast<unsigned>(m)); }
    std::uint8_t bits_ = 0;
};

/// How inputs that arrive while another input is already being held back are
/// handled.
enum class QueueMode : std::uint8_t {
    /// Bit-faithful to Predictive Buffer v4.6: one buffered action plus ONE
    /// "queued opposite" slot that always fires 1 tick after the buffered action.
    /// Known defect: press->release->press inside one window loses the release.
    LegacySingleSlot,
    /// Every input that arrives while one is held back is delayed by the SAME
    /// amount, in order. Relative spacing between inputs is preserved exactly.
    OrderedSpacing,
};

/// What the engine does on idle ticks to keep the game's button state aligned
/// with the player's physical button.
enum class FailsafePolicy : std::uint8_t {
    None,           ///< never inject anything
    ReleaseOnly,    ///< only release a hold the player no longer has (prevents stuck holds)
    MatchPhysical,  ///< v4.6 behavior: also PRESS if the player holds but the game does not
};

/// How the second player is treated while predicting a first-player input.
enum class DualProbe : std::uint8_t {
    LinkedOnly,   ///< P2 receives the simulated input only when the game links P1/P2 (1-player dual)
    AlwaysMirror, ///< v4.6 behavior: P2 receives it even in two-player mode
};

/// Why an input was not buffered. Passed to observers.
enum class PassReason : std::uint8_t {
    Disabled,       ///< engine disabled / preset OFF
    PlayerDead,     ///< player 1 is dead
    ModeBypass,     ///< gamemode rule says "never touch" (ship by default)
    PlatformerBypass,
    SpamGuard,      ///< rapid-input streak guard
    ReleaseIgnored, ///< release in a mode where release does not change physics
    NoWindow,       ///< delay limit or horizon is zero
    NoDelayHelps,   ///< strategy found no delay that improves survival (includes "already safe")
    QueueOverflow,  ///< ordered queue was full; pending inputs were flushed
    StrategyError,  ///< strategy returned an invalid delay; input passed through
};

/// Conversions (round to nearest tick).
constexpr Tick ticksFromFrames60(double frames, int ticksPerSecond = 240) {
    double t = frames * ticksPerSecond / 60.0;
    return static_cast<Tick>(t >= 0 ? t + 0.5 : t - 0.5);
}
constexpr Tick ticksFromMillis(double ms, int ticksPerSecond = 240) {
    double t = ms * ticksPerSecond / 1000.0;
    return static_cast<Tick>(t >= 0 ? t + 0.5 : t - 0.5);
}

} // namespace pbuf
