// Predictive Buffer API - the cross-mod interface.
//
// STATUS: written to the pattern documented at
//   https://docs.geode-sdk.org/mods/dependencies#event-macro  (Geode >= 4.3.0)
// It has NOT been compiled against the Geode SDK in the environment that
// produced it. See docs/VERIFICATION.md.
//
// How it works: the "Predictive Buffer API" mod owns the game hooks and the
// engine. Your mod calls the functions below; each call is routed to that mod by
// Geode's event system and returns a geode::Result, so a missing/outdated API mod
// is an ordinary error value, not a crash.
//
// Rules inherited from GEODE_EVENT_EXPORT (see the Geode docs):
//   * functions are declared `inline` here and defined once in src/geode/ApiExports.cpp
//   * they cannot be called from inside the API mod itself
//   * all calls must happen on the game's main thread
#pragma once

#include <Geode/loader/Dispatch.hpp>

#include "Config.hpp"
#include "Observer.hpp"
#include "Presets.hpp"
#include "Strategy.hpp"
#include "Version.hpp"

#include <memory>
#include <string>
#include <vector>

// Must be set manually (Geode docs): macros like GEODE_MOD_ID would resolve to the
// CONSUMER's id. tools/set_mod_id.py rewrites this line together with mod.json.
#undef MY_MOD_ID
#define MY_MOD_ID "ssilent.predictive-buffer-api"

namespace pbuf::api {

/// Version of the loaded API mod (see Version.hpp). Err if the API mod is not loaded.
inline geode::Result<int> apiVersion() GEODE_EVENT_EXPORT(&apiVersion, ());

/// Become the (single) owner of the buffer. Only one mod can own it at a time:
/// two buffers fighting over the same inputs is never what a player wants.
/// `owner` must be your own mod id. Err if another loaded mod owns it.
inline geode::Result<> acquire(std::string owner, Config cfg) GEODE_EVENT_EXPORT(&acquire, (owner, cfg));

/// Give the buffer up: disables it and removes your observers and strategies.
inline geode::Result<> release(std::string owner) GEODE_EVENT_EXPORT(&release, (owner));

/// Replace the whole configuration. Atomic: on Err nothing changed.
inline geode::Result<> setConfig(std::string owner, Config cfg) GEODE_EVENT_EXPORT(&setConfig, (owner, cfg));
inline geode::Result<Config> getConfig(std::string owner) GEODE_EVENT_EXPORT(&getConfig, (owner));

/// Replace your preset list (default: presets::classic()).
inline geode::Result<> setPresets(std::string owner, std::vector<Preset> list) GEODE_EVENT_EXPORT(&setPresets, (owner, list));
/// Apply a preset by id / advance to the next one. Return the applied preset's label.
inline geode::Result<std::string> applyPreset(std::string owner, std::string id) GEODE_EVENT_EXPORT(&applyPreset, (owner, id));
inline geode::Result<std::string> cyclePreset(std::string owner) GEODE_EVENT_EXPORT(&cyclePreset, (owner));

/// Register a custom strategy; select it with Config::strategyId == strategy->id().
/// The object must stay valid while registered (the API mod shares ownership).
inline geode::Result<> registerStrategy(std::string owner, std::shared_ptr<IBufferStrategy> s) GEODE_EVENT_EXPORT(&registerStrategy, (owner, s));

inline geode::Result<> addObserver(std::string owner, std::shared_ptr<IObserver> o) GEODE_EVENT_EXPORT(&addObserver, (owner, o));
inline geode::Result<> clearObservers(std::string owner) GEODE_EVENT_EXPORT(&clearObservers, (owner));

inline geode::Result<Stats> getStats(std::string owner) GEODE_EVENT_EXPORT(&getStats, (owner));

} // namespace pbuf::api

namespace pbuf {

/// Convenience handle for consumer mods: remembers your mod id, checks API
/// compatibility on open(), releases on destruction.
///
///   static pbuf::Buffer buffer("my.mod-id");
///   $on_mod(Loaded) {
///       if (auto r = buffer.open(pbuf::Config::recommended()); r.isErr())
///           geode::log::error("buffer: {}", r.unwrapErr());
///   }
class Buffer {
public:
    explicit Buffer(std::string ownerModId) : owner_(std::move(ownerModId)) {}
    ~Buffer() { if (open_) (void)api::release(owner_); }
    Buffer(Buffer const&) = delete;
    Buffer& operator=(Buffer const&) = delete;

    /// Checks that the API mod is present and binary-compatible (see docs/VERSIONING.md), then acquires.
    geode::Result<> open(Config cfg = Config::recommended()) {
        auto v = api::apiVersion();
        if (v.isErr())
            return geode::Err("Predictive Buffer API is not loaded (is it installed and enabled?): {}", v.unwrapErr());
        int const have = v.unwrap();
        // Config and the interfaces cross the mod boundary by value / vtable, so their layout must match.
        // 0.x: the MINOR number is the compatibility number (exact match). >= 1.0: same major, and the
        // installed API must be at least what this mod was built against. See docs/VERSIONING.md.
        bool const compatible = apiMajor(have) == PBUF_API_VERSION_MAJOR &&
            (PBUF_API_VERSION_MAJOR == 0 ? apiMinor(have) == PBUF_API_VERSION_MINOR : have >= kApiVersion);
        if (!compatible)
            return geode::Err("Predictive Buffer API version mismatch: this mod was built for {}.{} and the installed API is {}.{}",
                              PBUF_API_VERSION_MAJOR, PBUF_API_VERSION_MINOR, apiMajor(have), apiMinor(have));
        auto r = api::acquire(owner_, std::move(cfg));
        if (r.isOk()) open_ = true;
        return r;
    }
    bool isOpen() const { return open_; }
    std::string const& owner() const { return owner_; }

    geode::Result<> setConfig(Config c) { return api::setConfig(owner_, std::move(c)); }
    geode::Result<Config> config() const { return api::getConfig(owner_); }

    /// Read-modify-write: buffer.modify([](Config& c) { c.limits.normal = 6; });
    template <class F>
    geode::Result<> modify(F&& f) {
        auto c = config();
        if (c.isErr()) return geode::Err(c.unwrapErr());
        Config cfg = c.unwrap();
        f(cfg);
        return setConfig(std::move(cfg));
    }

    geode::Result<> setPresets(std::vector<Preset> list) { return api::setPresets(owner_, std::move(list)); }
    geode::Result<std::string> applyPreset(std::string id) { return api::applyPreset(owner_, std::move(id)); }
    geode::Result<std::string> cyclePreset() { return api::cyclePreset(owner_); }
    geode::Result<> registerStrategy(std::shared_ptr<IBufferStrategy> s) { return api::registerStrategy(owner_, std::move(s)); }
    geode::Result<> addObserver(std::shared_ptr<IObserver> o) { return api::addObserver(owner_, std::move(o)); }
    geode::Result<Stats> stats() const { return api::getStats(owner_); }

private:
    std::string owner_;
    bool open_ = false;
};

} // namespace pbuf
