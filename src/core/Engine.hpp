// INTERNAL. The game-agnostic buffering engine.
//
// Derived from `TrajectoryBufferSim::interceptInput()` / `onPlayerUpdate()` /
// `m_SimFireButton()` of Predictive Buffer v4.6, restructured so that every rule
// is read from Config and every game interaction goes through IBackend.
// Zero Geode dependency: compiled and unit-tested on a plain C++ toolchain.
#pragma once

#include "Backend.hpp"
#include "Scheduler.hpp"

#include <pbuf/Config.hpp>
#include <pbuf/Observer.hpp>
#include <pbuf/Strategy.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace pbuf::detail {

class Engine final : private IFireSink {
public:
    explicit Engine(IBackend& backend, Config cfg = Config::recommended());
    ~Engine() override;
    Engine(Engine const&) = delete;
    Engine& operator=(Engine const&) = delete;

    // ---- configuration (game thread) --------------------------------------
    Config const& config() const { return cfg_; }

    /// Validates, resolves the strategy and applies. Pending buffered inputs are
    /// delivered immediately (v4.6 silently dropped them when cycling presets).
    /// Returns "" on success, otherwise a clear error and NOTHING is changed.
    std::string setConfig(Config const& cfg);

    /// Register (or replace, by id) a strategy. Returns "" on success.
    std::string registerStrategy(std::shared_ptr<IBufferStrategy> s);
    /// Remove a strategy. If it is the active one the built-in is activated (config updated).
    void unregisterStrategy(std::string_view id);
    bool hasStrategy(std::string_view id) const;
    std::string strategyIds() const;

    void addObserver(std::shared_ptr<IObserver> o);
    void clearObservers() { observers_.clear(); }

    Stats const& stats() const { return stats_; }

    // ---- lifecycle ---------------------------------------------------------
    void onLevelEnter();
    void onLevelExit();
    /// The REAL player died (never call for simulated players).
    void onPlayerDied();

    // ---- game events -------------------------------------------------------
    /// Decide what to do with a button event from the game. Returns true if the
    /// game should process it now, false if it was swallowed (held back/queued).
    bool onInput(bool push, bool isPlayer1);

    /// Call once at the start of every player-1 physics update (never for probes).
    void onTick();

    /// True while the engine is delivering a held-back input; the game hook must
    /// let such calls through untouched.
    bool isFiring() const { return firing_; }

    bool pending() const { return sched_->inFlight(); }

private:
    void fire(bool push) override;  // IFireSink
    bool pass(bool push, PassReason why);
    void releasePending();
    void notifyConfig();

    IBackend& backend_;
    Config cfg_;
    std::unique_ptr<IScheduler> sched_;
    std::shared_ptr<IBufferStrategy> strategy_;
    std::unordered_map<std::string, std::shared_ptr<IBufferStrategy>> strategies_;
    std::vector<std::shared_ptr<IObserver>> observers_;
    Stats stats_;

    bool inLevel_ = false;
    bool firing_ = false;
    bool physical_ = false;      // what the player's finger is doing
    bool gameHolding_ = false;   // what the game was last told
    Tick tick_ = 0;
    Tick ticksSinceLast_ = 999;
    int  streak_ = 0;
};

} // namespace pbuf::detail
