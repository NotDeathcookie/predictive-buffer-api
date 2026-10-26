// INTERNAL - needs the Geode SDK. NOT compiled where it was written.
//
// Defines the functions declared in include/pbuf/Api.hpp. Per the Geode docs,
// GEODE_DEFINE_EVENT_EXPORTS MUST be defined before including the header, and this
// must happen in exactly one source file. Exported functions cannot be called from
// inside this mod, so they are thin wrappers over Runtime.
#define GEODE_DEFINE_EVENT_EXPORTS
#include <pbuf/Api.hpp>

#include "Runtime.hpp"

using namespace geode::prelude;
using pbuf::geodeimpl::Runtime;

namespace pbuf::api {

Result<int> apiVersion() { return Ok(pbuf::kApiVersion); }

Result<> acquire(std::string owner, Config cfg) { return Runtime::get().acquire(owner, std::move(cfg)); }
Result<> release(std::string owner) { return Runtime::get().release(owner); }
Result<> setConfig(std::string owner, Config cfg) { return Runtime::get().setConfig(owner, std::move(cfg)); }
Result<Config> getConfig(std::string owner) { return Runtime::get().getConfig(owner); }
Result<> setPresets(std::string owner, std::vector<Preset> list) { return Runtime::get().setPresets(owner, std::move(list)); }
Result<std::string> applyPreset(std::string owner, std::string id) { return Runtime::get().applyPreset(owner, id); }
Result<std::string> cyclePreset(std::string owner) { return Runtime::get().cyclePreset(owner); }
Result<> registerStrategy(std::string owner, std::shared_ptr<IBufferStrategy> s) { return Runtime::get().registerStrategy(owner, std::move(s)); }
Result<> addObserver(std::string owner, std::shared_ptr<IObserver> o) { return Runtime::get().addObserver(owner, std::move(o)); }
Result<> clearObservers(std::string owner) { return Runtime::get().clearObservers(owner); }
Result<Stats> getStats(std::string owner) { return Runtime::get().getStats(owner); }

} // namespace pbuf::api
