// Differential test: Engine under Config::classic() must behave identically to a
// transliteration of Predictive Buffer v4.6's decision logic (reference_v46.hpp).
//
// What this proves: the restructuring into Engine/Scheduler/Strategy/Config did not
// change decisions or the sequence of injected inputs, for any interleaving of
// inputs, ticks, deaths, gamemode changes and preset changes the generator produces.
// What it does NOT prove: that the reference matches the real game, or that the
// real simulation (getSurvivalTicks) behaves like the mock. See docs/VERIFICATION.md.
#include "reference_v46.hpp"

#include "Engine.hpp"

#include <cstdio>
#include <random>

using namespace pbt;

namespace {

struct Preset3 { bool en; int all; int wave; };
const Preset3 kPresets[] = {{true, 2, 2}, {true, 4, 4}, {true, 8, 8}, {true, 16, 8}, {false, 0, 0}, {true, 24, 24}, {true, 60, 60}};

Config cfgFor(Preset3 p) {
    Config c = Config::classic();
    c.enabled = p.en;
    c.limits = {p.all, p.wave};
    return c;
}

Mode randMode(std::mt19937_64& r) { return static_cast<Mode>(r() % static_cast<unsigned>(Mode::Count_)); }

} // namespace

int main() {
    long long totalSteps = 0, failures = 0;
    std::uint64_t cBuffered = 0, cAbsorbed = 0, cSpam = 0, cFired = 0, cPassed = 0;

    for (std::uint64_t seed = 1; seed <= 400 && failures == 0; ++seed) {
        std::mt19937_64 rng(seed);
        World wRef, wEng;
        RefV46 ref(wRef);
        MockBackend backend(wEng);
        pbuf::detail::Engine eng(backend, cfgFor(kPresets[0]));

        auto setState = [&](GameState g) { wRef.gs = g; wEng.gs = g; };
        auto enterLevel = [&] {
            GameState g; g.inLevel = true;
            setState(g);
            ref.init(); eng.onLevelEnter();
        };
        enterLevel();

        std::size_t compared = 0;
        for (int step = 0; step < 4000; ++step) {
            ++totalSteps;
            unsigned r = static_cast<unsigned>(rng() % 100);
            std::uint64_t pseed = rng();
            wRef.probeSeed = wEng.probeSeed = pseed;

            if (r < 45) {                                   // physics tick
                ref.onPlayerUpdate(); eng.onTick();
            } else if (r < 80) {                            // player 1 input (press/release)
                bool push = (rng() & 1) != 0;
                bool a = ref.interceptInput(push, true);
                bool b = eng.onInput(push, true);
                if (a != b) { std::printf("seed %llu step %d: P1 input(%d) ref=%d eng=%d\n", (unsigned long long)seed, step, push, a, b); ++failures; }
            } else if (r < 86) {                            // player 2 input
                bool push = (rng() & 1) != 0;
                bool a = ref.interceptInput(push, false);
                bool b = eng.onInput(push, false);
                if (a != b) { std::printf("seed %llu step %d: P2 input ref=%d eng=%d\n", (unsigned long long)seed, step, a, b); ++failures; }
            } else if (r < 92) {                            // gamemode / dual / flags change
                GameState g = wRef.gs;
                g.p1 = randMode(rng); g.p2 = randMode(rng);
                g.dual = (rng() % 100) < 35;
                g.twoPlayerMode = (rng() % 100) < 30;
                g.p1Mini = rng() & 1; g.p2Mini = rng() & 1;
                g.platformer = (rng() % 100) < 25;          // classic must IGNORE this
                g.p1Dead = (rng() % 100) < 6;
                setState(g);
            } else if (r < 95) {                            // real death
                ref.realPlayerDied(); eng.onPlayerDied();
                GameState g = wRef.gs; g.p1Dead = false; setState(g);
            } else if (r < 98) {                            // preset change (only when idle, see note)
                if (ref.m_waitTicks == -1 && !eng.pending()) {
                    Preset3 p = kPresets[rng() % (sizeof kPresets / sizeof *kPresets)];
                    ref.applyPreset(p.en, p.all, p.wave);
                    std::string err = eng.setConfig(cfgFor(p));
                    if (!err.empty()) { std::printf("setConfig failed: %s\n", err.c_str()); ++failures; }
                }
            } else {                                        // respawn/new level
                if ((rng() % 4) == 0) enterLevel();
            }

            // Compare injected inputs produced so far.
            if (wRef.log.size() != wEng.log.size()) {
                std::printf("seed %llu step %d: fire count ref=%zu eng=%zu\n", (unsigned long long)seed, step, wRef.log.size(), wEng.log.size());
                ++failures;
            }
            for (; compared < wRef.log.size() && compared < wEng.log.size(); ++compared) {
                if (!(wRef.log[compared] == wEng.log[compared])) {
                    std::printf("seed %llu step %d: fire #%zu differs ref=%s eng=%s\n", (unsigned long long)seed, step, compared,
                                show(wRef.log[compared]).c_str(), show(wEng.log[compared]).c_str());
                    ++failures;
                }
            }
            if (failures) break;
        }
        auto const& s = eng.stats();
        cBuffered += s.buffered; cAbsorbed += s.absorbed; cSpam += s.spamBypassed; cFired += s.fired; cPassed += s.passed;
    }

    std::printf("differential: %lld steps over 400 seeds, %lld divergences\n", totalSteps, failures);
    std::printf("coverage: buffered=%llu absorbed(queued opposite)=%llu spam-bypass=%llu injected=%llu passthrough=%llu\n",
                (unsigned long long)cBuffered, (unsigned long long)cAbsorbed, (unsigned long long)cSpam,
                (unsigned long long)cFired, (unsigned long long)cPassed);
    if (cBuffered == 0 || cAbsorbed == 0 || cSpam == 0) { std::printf("FAIL: generator did not exercise key branches\n"); return 2; }
    return failures ? 1 : 0;
}
