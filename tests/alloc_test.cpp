// Verifies the engine's steady-state hot path performs ZERO heap allocations
// (inputs, ticks, scheduling, firing) for both queue modes, with no observers.
#include "mock.hpp"
#include "Engine.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <new>

static std::atomic<long> g_allocs{0};
static std::atomic<bool> g_count{false};
void* operator new(std::size_t n) { if (g_count) ++g_allocs; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

int main() {
    int bad = 0;
    for (auto q : {pbuf::QueueMode::LegacySingleSlot, pbuf::QueueMode::OrderedSpacing}) {
        pbt::World w; w.gs.inLevel = true; w.gs.p1 = pbuf::Mode::Wave; w.gs.dual = true;
        pbt::MockBackend be(w);
        pbuf::Config c = pbuf::Config::recommended(); c.behavior.queue = q; c.limits = {16, 8}; c.spam.enabled = false;
        pbuf::detail::Engine eng(be, c);
        eng.onLevelEnter();
        w.log.reserve(1 << 20);                       // the MOCK's log is not what we measure
        g_allocs = 0; g_count = true;
        for (int i = 0; i < 200000; ++i) {
            w.probeSeed = static_cast<std::uint64_t>(i) * 2654435761u;
            int ph = i % 100;                         // a press, a release inside the window, then idle
            if (ph == 0) eng.onInput(true, true);
            if (ph == 1) eng.onInput(true, false);     // linked player-2 copy
            if (ph == 4) eng.onInput(false, true);
            eng.onTick();
        }
        g_count = false;
        std::printf("%s: %ld allocations over 200k iterations (buffered=%llu fired=%llu)\n",
                    q == pbuf::QueueMode::LegacySingleSlot ? "legacy " : "ordered", g_allocs.load(),
                    (unsigned long long)eng.stats().buffered, (unsigned long long)eng.stats().fired);
        if (g_allocs != 0) ++bad;
        if (eng.stats().buffered < 1000) { std::printf("  test too weak: buffering barely exercised\n"); ++bad; }
    }
    return bad;
}
