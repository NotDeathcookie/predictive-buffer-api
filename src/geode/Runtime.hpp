// INTERNAL - needs the Geode SDK. NOT compiled where it was written.
//
// The process-wide singleton of the API mod: one backend, one engine, at most one
// owning consumer mod. Everything here runs on the game's main thread.
#pragma once

#include "GeodeBackend.hpp"
#include "Engine.hpp"

#include <pbuf/Presets.hpp>

#include <map>
#include <string>

namespace pbuf::geodeimpl {

class Runtime {
public:
    static Runtime& get() { static Runtime r; return r; }

    detail::Engine& engine() { return engine_; }
    GeodeBackend& backend() { return backend_; }

    // ---- called from hooks ----
    void onPlayLayerInit();
    void onPlayLayerQuit();
    void onEditorInit();
    void onRealPlayerDied();
    void onPlayerUpdate(geode::prelude::PlayerObject* player);

    // ---- called from ApiExports.cpp (every function validates `owner`) ----
    geode::Result<> acquire(std::string const& owner, Config cfg);
    geode::Result<> release(std::string const& owner);
    geode::Result<> setConfig(std::string const& owner, Config cfg);
    geode::Result<Config> getConfig(std::string const& owner);
    geode::Result<> setPresets(std::string const& owner, std::vector<Preset> list);
    geode::Result<std::string> applyPreset(std::string const& owner, std::string const& id);
    geode::Result<std::string> cyclePreset(std::string const& owner);
    geode::Result<> registerStrategy(std::string const& owner, std::shared_ptr<IBufferStrategy> s);
    geode::Result<> addObserver(std::string const& owner, std::shared_ptr<IObserver> o);
    geode::Result<> clearObservers(std::string const& owner);
    geode::Result<Stats> getStats(std::string const& owner);

private:
    Runtime();
    geode::Result<> requireOwner(std::string const& owner) const;
    geode::Result<> applyConfig(Config const& cfg);
    void resetOwnedState();

    GeodeBackend backend_;
    detail::Engine engine_;
    std::string owner_;
    PresetBook presets_;
    std::map<std::string, std::string> strategyOwner_;  // strategy id -> owner mod id
};

} // namespace pbuf::geodeimpl
