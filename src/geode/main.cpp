// INTERNAL - needs the Geode SDK. NOT compiled where it was written.
#include <Geode/Geode.hpp>

#include <pbuf/Version.hpp>

using namespace geode::prelude;

$on_mod(Loaded) {
    // Deliberately quiet: the original allocated a console window and printed a banner on every launch.
    log::info("Predictive Buffer API {}.{}.{} loaded (inactive until a mod acquires the buffer)",
              PBUF_API_VERSION_MAJOR, PBUF_API_VERSION_MINOR, PBUF_API_VERSION_PATCH);
}
