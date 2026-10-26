#pragma once
#include "../Result.hpp"
#include <tuple>
#ifdef GEODE_DEFINE_EVENT_EXPORTS
#define GEODE_EVENT_EXPORT(fp, args) ;
#else
// Real macro forwards `args` to the API mod; the fake just consumes them.
#define GEODE_EVENT_EXPORT(fp, args) { [[maybe_unused]] auto _fwd = std::make_tuple args; return {}; }
#endif
