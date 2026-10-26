// INTERNAL - needs the Geode SDK. NOT compiled where it was written.
#include "Runtime.hpp"

#include <pbuf/TrajectorySearch.hpp>

using namespace geode::prelude;

namespace pbuf::geodeimpl {

namespace {
Config inertConfig() {
    Config c = Config::recommended();
    c.enabled = false;   // with no owner the engine must do nothing
    return c;
}
} // namespace

Runtime::Runtime() : engine_(backend_, inertConfig()) {}

// ---------------------------------------------------------------------------
// hook entry points
// ---------------------------------------------------------------------------
void Runtime::onPlayLayerInit() {
    backend_.shutdown();          // never leak the previous level's simulated players
    engine_.onLevelEnter();       // runs even when inactive so an owner can acquire mid-level
    backend_.createSimPlayers();  // no-op unless g_active
}

void Runtime::onPlayLayerQuit() {
    engine_.onLevelExit();
    backend_.shutdown();
}

void Runtime::onEditorInit() {
    engine_.onLevelExit();
    backend_.shutdown();
}

void Runtime::onRealPlayerDied() { engine_.onPlayerDied(); }

void Runtime::onPlayerUpdate(PlayerObject* player) {
    PlayLayer* pl = PlayLayer::get();
    if (!pl || player != pl->m_player1) return;
    engine_.onTick();
}

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------
Result<> Runtime::requireOwner(std::string const& owner) const {
    if (owner_.empty())
        return Err("the buffer has no owner; call acquire() first (caller: '{}')", owner);
    if (owner != owner_)
        return Err("'{}' does not own the buffer (owned by '{}')", owner, owner_);
    return Ok();
}

Result<> Runtime::applyConfig(Config const& cfg) {
    std::string err = engine_.setConfig(cfg);
    if (!err.empty()) return Err(err);
    backend_.configure(engine_.config());
    return Ok();
}

void Runtime::resetOwnedState() {
    engine_.clearObservers();
    for (auto const& [id, _] : strategyOwner_) engine_.unregisterStrategy(id);
    strategyOwner_.clear();
    presets_ = PresetBook();
}

Result<> Runtime::acquire(std::string const& owner, Config cfg) {
    // UNVERIFIED against Geode 5.11.2: Loader::getLoadedMod / Mod::isLoaded
    // (isLoaded is the v5 name for the old isEnabled, per the v5 migration guide).
    Mod* mod = Loader::get()->getLoadedMod(owner);
    if (!mod) return Err("'{}' is not a loaded mod id; pass your own mod's id", owner);

    if (!owner_.empty() && owner_ != owner) {
        Mod* current = Loader::get()->getLoadedMod(owner_);
        if (current && current->isLoaded())
            return Err("the buffer is already owned by '{}'; only one mod can own it at a time", owner_);
        resetOwnedState();   // previous owner is gone: take over cleanly
    }

    if (auto r = applyConfig(cfg); r.isErr()) return r;
    owner_ = owner;
    g_active = true;
    backend_.createSimPlayers();
    log::info("buffer acquired by '{}'", owner);
    return Ok();
}

Result<> Runtime::release(std::string const& owner) {
    if (auto r = requireOwner(owner); r.isErr()) return r;
    resetOwnedState();
    Config c = engine_.config();
    c.enabled = false;
    (void)applyConfig(c);
    owner_.clear();
    g_active = false;
    backend_.shutdown();
    log::info("buffer released by '{}'", owner);
    return Ok();
}

Result<> Runtime::setConfig(std::string const& owner, Config cfg) {
    if (auto r = requireOwner(owner); r.isErr()) return r;
    return applyConfig(cfg);
}

Result<Config> Runtime::getConfig(std::string const& owner) {
    if (auto r = requireOwner(owner); r.isErr()) return Err(r.unwrapErr());
    return Ok(engine_.config());
}

Result<> Runtime::setPresets(std::string const& owner, std::vector<Preset> list) {
    if (auto r = requireOwner(owner); r.isErr()) return r;
    if (list.empty()) return Err("preset list is empty");
    presets_ = PresetBook(std::move(list));
    return Ok();
}

Result<std::string> Runtime::applyPreset(std::string const& owner, std::string const& id) {
    if (auto r = requireOwner(owner); r.isErr()) return Err(r.unwrapErr());
    Preset const* p = presets_.select(id);
    if (!p) return Err("unknown preset '{}' (available: {})", id, presets_.ids());
    Config c = engine_.config();
    p->applyTo(c);
    if (auto r = applyConfig(c); r.isErr()) return Err(r.unwrapErr());
    return Ok(p->label);
}

Result<std::string> Runtime::cyclePreset(std::string const& owner) {
    if (auto r = requireOwner(owner); r.isErr()) return Err(r.unwrapErr());
    Preset const* p = presets_.next();
    if (!p) return Err("preset list is empty");
    Config c = engine_.config();
    p->applyTo(c);
    if (auto r = applyConfig(c); r.isErr()) return Err(r.unwrapErr());
    return Ok(p->label);
}

Result<> Runtime::registerStrategy(std::string const& owner, std::shared_ptr<IBufferStrategy> s) {
    if (auto r = requireOwner(owner); r.isErr()) return r;
    if (!s) return Err("strategy is null");
    std::string id(s->id());
    if (id.rfind("pbuf.", 0) == 0) return Err("strategy ids starting with 'pbuf.' are reserved (got '{}')", id);
    auto it = strategyOwner_.find(id);
    if (it != strategyOwner_.end() && it->second != owner)
        return Err("strategy id '{}' is already registered by '{}'", id, it->second);
    std::string err = engine_.registerStrategy(std::move(s));
    if (!err.empty()) return Err(err);
    strategyOwner_[id] = owner;
    return Ok();
}

Result<> Runtime::addObserver(std::string const& owner, std::shared_ptr<IObserver> o) {
    if (auto r = requireOwner(owner); r.isErr()) return r;
    if (!o) return Err("observer is null");
    engine_.addObserver(std::move(o));
    return Ok();
}

Result<> Runtime::clearObservers(std::string const& owner) {
    if (auto r = requireOwner(owner); r.isErr()) return r;
    engine_.clearObservers();
    return Ok();
}

Result<Stats> Runtime::getStats(std::string const& owner) {
    if (auto r = requireOwner(owner); r.isErr()) return Err(r.unwrapErr());
    return Ok(engine_.stats());
}

} // namespace pbuf::geodeimpl
