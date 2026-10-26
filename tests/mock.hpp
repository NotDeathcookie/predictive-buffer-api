// Test support: a fake game and a deterministic fake predictor.
#pragma once

#include "Backend.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace pbt {

using namespace pbuf;
using pbuf::detail::GameState;

struct Ev {
    enum Kind { Fire, Tick, Input, Pass } kind;
    int a = 0;   // Fire: push; Input: push<<1|p1 ; Tick: tick number
    int b = 0;   // Input: returned value
    bool operator==(Ev const&) const = default;
};

inline std::string show(Ev const& e) {
    static const char* n[] = {"Fire", "Tick", "Input", "Pass"};
    return std::string(n[e.kind]) + "(" + std::to_string(e.a) + "," + std::to_string(e.b) + ")";
}

inline std::uint64_t mix(std::uint64_t x) {
    x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL; x ^= x >> 33;
    return x;
}

/// Deterministic stand-in for the trajectory simulation. Shaped so every branch of
/// the search (frame-0 safe, full clear, partial gain, no help) is reachable.
inline Tick mockSurvival(std::uint64_t seed, Tick delay, bool push, Tick horizon) {
    std::uint64_t h0 = mix(seed * 31 + (push ? 7 : 3));
    std::uint64_t h  = mix(h0 + static_cast<std::uint64_t>(delay) * 0x9e3779b97f4a7c15ULL);
    Tick base = static_cast<Tick>(h0 % static_cast<std::uint64_t>(horizon));
    if (delay == 0) return (h0 % 100) < 35 ? horizon : base;
    std::uint64_t r = h % 100;
    if (r < 30) return horizon;
    if (r < 70) { Tick v = base + static_cast<Tick>((h >> 8) % 40); return v > horizon ? horizon : v; }
    Tick v = base + static_cast<Tick>((h >> 8) % 5);
    return v > horizon ? horizon : v;
}

struct World {
    GameState gs;
    std::uint64_t probeSeed = 1;
    std::vector<Ev> log;
};

struct MockProbe final : IProbe {
    World* w = nullptr;
    std::int64_t sim = 0;
    Tick survival(Tick delay, bool push, Tick horizon) override {
        sim += delay + horizon;   // fake cost model: proportional to work
        return mockSurvival(w->probeSeed, delay, push, horizon);
    }
    std::int64_t simulatedTicks() const override { return sim; }
};

struct MockBackend final : pbuf::detail::IBackend {
    World& w;
    MockProbe probe;
    int probes = 0;
    explicit MockBackend(World& world) : w(world) { probe.w = &world; }
    bool query(GameState& out) override { out = w.gs; return w.gs.inLevel; }
    IProbe& beginProbe(InputContext const&, GameState const&) override { probe.sim = 0; ++probes; return probe; }
    void endProbe() override {}
    void fire(bool push) override { w.log.push_back({Ev::Fire, push ? 1 : 0, 0}); }
};

} // namespace pbt
