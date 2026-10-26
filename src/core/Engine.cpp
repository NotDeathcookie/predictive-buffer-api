// INTERNAL. See Engine.hpp.
//
// DERIVED from Predictive Buffer v4.6 (`interceptInput`, `onPlayerUpdate`,
// `m_SimFireButton`). The order of the checks below deliberately follows the
// original; deviations are marked "CHANGE:" and documented in
// docs/ORIGINAL_ANALYSIS.md.
#include "Engine.hpp"

#include <pbuf/TrajectorySearch.hpp>

namespace pbuf::detail {

namespace {

bool anyActive(GameState const& s, ModeSet set) {
    return set.has(s.p1) || (s.dual && set.has(s.p2));
}

// RAII so a probe is always ended, even if a strategy misbehaves.
struct ProbeScope {
    ProbeScope(IBackend& b, InputContext const& c, GameState const& s) : b_(b), probe_(b.beginProbe(c, s)) {}
    ~ProbeScope() { b_.endProbe(); }
    ProbeScope(ProbeScope const&) = delete;
    ProbeScope& operator=(ProbeScope const&) = delete;
    IProbe& probe() { return probe_; }
private:
    IBackend& b_;
    IProbe& probe_;
};

} // namespace

Engine::Engine(IBackend& backend, Config cfg) : backend_(backend), cfg_(std::move(cfg)) {
    auto builtin = std::make_shared<TrajectorySearchStrategy>();
    strategies_.emplace(TrajectorySearchStrategy::kId, builtin);
    auto it = strategies_.find(cfg_.strategyId);
    if (it == strategies_.end()) { cfg_.strategyId = TrajectorySearchStrategy::kId; it = strategies_.find(cfg_.strategyId); }
    strategy_ = it->second;
    sched_ = makeScheduler(cfg_.behavior.queue);
}

Engine::~Engine() = default;

std::string Engine::setConfig(Config const& c) {
    auto errs = c.validate();
    if (!errs.empty()) {
        std::string msg = "invalid config: ";
        for (std::size_t i = 0; i < errs.size(); ++i) { if (i) msg += "; "; msg += errs[i]; }
        return msg;
    }
    auto it = strategies_.find(c.strategyId);
    if (it == strategies_.end())
        return "unknown strategy '" + c.strategyId + "' (registered: " + strategyIds() + ")";

    // CHANGE: v4.6 dropped a pending buffered input when cycling presets; deliver it instead.
    if (inLevel_ && sched_->inFlight()) sched_->flush(*this); else sched_->clear();

    if (c.behavior.queue != cfg_.behavior.queue) sched_ = makeScheduler(c.behavior.queue);
    cfg_ = c;
    strategy_ = it->second;
    notifyConfig();
    return {};
}

std::string Engine::registerStrategy(std::shared_ptr<IBufferStrategy> s) {
    if (!s) return "strategy is null";
    std::string id(s->id());
    if (id.empty()) return "strategy id is empty";
    bool const replacingActive = (id == cfg_.strategyId);
    strategies_[id] = std::move(s);
    if (replacingActive) strategy_ = strategies_[id];
    return {};
}

void Engine::unregisterStrategy(std::string_view id) {
    std::string key(id);
    if (key == TrajectorySearchStrategy::kId) return;   // built-in is permanent
    strategies_.erase(key);
    if (cfg_.strategyId == key) {
        cfg_.strategyId = TrajectorySearchStrategy::kId;
        strategy_ = strategies_.find(cfg_.strategyId)->second;
        notifyConfig();
    }
}

bool Engine::hasStrategy(std::string_view id) const { return strategies_.find(std::string(id)) != strategies_.end(); }

std::string Engine::strategyIds() const {
    std::string out;
    for (auto const& [k, _] : strategies_) { if (!out.empty()) out += ", "; out += k; }
    return out;
}

void Engine::addObserver(std::shared_ptr<IObserver> o) { if (o) observers_.push_back(std::move(o)); }

void Engine::notifyConfig() { for (auto& o : observers_) o->onConfigChanged(cfg_); }

// ---------------------------------------------------------------------------
// lifecycle
// ---------------------------------------------------------------------------
void Engine::onLevelEnter() {
    sched_->clear();
    physical_ = gameHolding_ = false;
    ticksSinceLast_ = 999;
    streak_ = 0;
    tick_ = 0;
    inLevel_ = true;
}

void Engine::onLevelExit() {
    sched_->clear();
    inLevel_ = false;
}

void Engine::onPlayerDied() {
    // v4.6 checkSimulationDeath(): a real death clears every pending buffer so the respawn is clean.
    sched_->clear();
    gameHolding_ = false;
    streak_ = 0;
}

// ---------------------------------------------------------------------------
// delivering inputs
// ---------------------------------------------------------------------------
void Engine::fire(bool push) {
    struct Guard { bool& f; explicit Guard(bool& x) : f(x) { f = true; } ~Guard() { f = false; } } g(firing_);
    gameHolding_ = push;
    ++stats_.fired;
    backend_.fire(push);
    for (auto& o : observers_) o->onFired(push);
}

// A bypass rule just took over. Legacy: drop (v4.6). Ordered: deliver in order so no input is lost.
void Engine::releasePending() {
    if (cfg_.behavior.queue == QueueMode::OrderedSpacing) sched_->flush(*this);
    else sched_->clear();
}

bool Engine::pass(bool push, PassReason why) {
    ++stats_.passed;
    for (auto& o : observers_) o->onPassThrough(push, why);
    return true;
}

// ---------------------------------------------------------------------------
// interceptInput()
// ---------------------------------------------------------------------------
bool Engine::onInput(bool push, bool isPlayer1) {
    if (firing_) return true;

    GameState gs;
    if (!backend_.query(gs) || !gs.inLevel) return true;

    // Keep player 2 in sync with player 1 in one-player dual mode: while P1's input is
    // being held back, swallow P2's copy of it too.
    if (!isPlayer1) {
        if (gs.linked() && sched_->busy()) return false;
        return true;
    }

    ++stats_.inputs;

    // Always record the player's true physical state first.
    physical_ = push;

    if (!cfg_.enabled) { gameHolding_ = push; return pass(push, PassReason::Disabled); }

    if (gs.p1Dead) {
        sched_->clear();
        gameHolding_ = push;
        return pass(push, PassReason::PlayerDead);
    }

    // CHANGE (new): platformer levels are not predictable (left/right buttons are not modeled).
    if (gs.platformer && cfg_.rules.bypassPlatformer) {
        releasePending();
        gameHolding_ = push;
        return pass(push, PassReason::PlatformerBypass);
    }

    // 1. Modes that must never be touched (ship by default).
    if (anyActive(gs, cfg_.rules.bypass)) {
        releasePending();
        gameHolding_ = push;
        return pass(push, PassReason::ModeBypass);
    }

    // 2. An input is already held back: queue this one behind it.
    if (sched_->inFlight()) {
        ticksSinceLast_ = 0;
        if (sched_->absorb(push, tick_)) { ++stats_.absorbed; return false; }
        sched_->flush(*this);               // queue full: deliver in order, then let this one through
        gameHolding_ = push;
        return pass(push, PassReason::QueueOverflow);
    }

    // Legacy only: a queued opposite action is due next tick; fire it first.
    bool oppositePush = false;
    if (sched_->takeDueOpposite(oppositePush)) {
        if (gameHolding_ != oppositePush) fire(oppositePush);
    }

    bool const isWave     = anyActive(gs, cfg_.rules.waveLike);
    bool const isMiniWave = (cfg_.rules.waveLike.has(gs.p1) && gs.p1Mini) ||
                            (gs.dual && cfg_.rules.waveLike.has(gs.p2) && gs.p2Mini);

    // 3. Streak-based spam guard: only 3+ rapid inputs in a row bypass the buffer.
    if (ticksSinceLast_ < cfg_.spam.intervalTicks) ++streak_; else streak_ = 0;
    ticksSinceLast_ = 0;
    if (cfg_.spam.enabled && anyActive(gs, cfg_.spam.appliesTo) && streak_ >= cfg_.spam.streakRequired) {
        ++stats_.spamBypassed;
        gameHolding_ = push;
        return pass(push, PassReason::SpamGuard);
    }

    // Only some modes change trajectory on RELEASE.
    bool const releaseAffects = anyActive(gs, cfg_.rules.releaseMatters);
    if (!push && (!releaseAffects || !cfg_.behavior.bufferReleases)) {
        gameHolding_ = false;
        return pass(push, PassReason::ReleaseIgnored);
    }

    sched_->dropQueuedOpposite();

    Tick const horizon  = isMiniWave ? cfg_.horizons.miniWave : (isWave ? cfg_.horizons.wave : cfg_.horizons.normal);
    Tick const maxDelay = isWave ? cfg_.limits.wave : cfg_.limits.normal;
    if (maxDelay <= 0 || horizon <= 0) { gameHolding_ = push; return pass(push, PassReason::NoWindow); }

    InputContext ctx;
    ctx.push = push;
    ctx.dual = gs.dual;
    ctx.wave = isWave;
    ctx.miniWave = isMiniWave;
    ctx.maxDelay = maxDelay;
    ctx.horizon = horizon;
    ctx.tuning = cfg_.tuning;
    ctx.maxSimTicks = cfg_.budget.maxSimTicksPerInput;

    // 4+5. Ask the strategy.
    Decision d;
    {
        ProbeScope scope(backend_, ctx, gs);
        d = strategy_->decide(ctx, scope.probe());
        stats_.simTicks += static_cast<std::uint64_t>(scope.probe().simulatedTicks());
    }
    if (d.budgetExhausted) ++stats_.budgetExhausted;

    if (d.delay < 0 || d.delay > maxDelay) {
        ++stats_.strategyErrors;
        gameHolding_ = push;
        return pass(push, PassReason::StrategyError);
    }

    if (d.delay > 0) {
        ++stats_.buffered;
        if (!observers_.empty()) {
            BufferedEvent ev;
            ev.push = push; ev.delay = d.delay; ev.frame0Ticks = d.frame0Ticks;
            ev.bestTicks = d.bestTicks; ev.maxDelay = maxDelay; ev.wave = isWave;
            for (auto& o : observers_) o->onBuffered(ev);
        }
        sched_->schedule(push, d.delay, tick_);
        return false;
    }

    gameHolding_ = push;
    return pass(push, PassReason::NoDelayHelps);
}

// ---------------------------------------------------------------------------
// onPlayerUpdate()
// ---------------------------------------------------------------------------
void Engine::onTick() {
    ++tick_;
    if (!cfg_.enabled) return;
    if (ticksSinceLast_ < 999) ++ticksSinceLast_;

    GameState gs;
    if (!backend_.query(gs) || !gs.inLevel) return;

    // Bypassed modes (ship) and platformer: stop buffering and resynchronise.
    if (anyActive(gs, cfg_.rules.bypass) || (gs.platformer && cfg_.rules.bypassPlatformer)) {
        if (cfg_.behavior.queue == QueueMode::LegacySingleSlot) {
            // v4.6: drop whatever is pending, then match the physical button.
            sched_->clear();
            if (gameHolding_ != physical_) fire(physical_);
        } else {
            // CHANGE: deliver the inputs in order instead of dropping them; the end state equals physical_.
            sched_->flush(*this);
        }
        return;
    }

    bool const idle = sched_->tick(tick_, *this);
    if (!idle) return;

    // FAILSAFE: nothing pending; make sure the game's hold state can never stay stuck.
    switch (cfg_.behavior.failsafe) {
        case FailsafePolicy::None: break;
        case FailsafePolicy::ReleaseOnly:
            if (gameHolding_ && !physical_) fire(false);
            break;
        case FailsafePolicy::MatchPhysical:
            if (gameHolding_ != physical_) fire(physical_);
            break;
    }
}

} // namespace pbuf::detail
