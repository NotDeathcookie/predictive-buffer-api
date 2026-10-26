# What was verified, and what was not

Environment that produced this repo: Linux, g++ 13.3, **no CMake, no Geode SDK, no Geometry Dash, no network
from the build sandbox**. Geode documentation was read through a separate web tool.

## Verified by execution (`tools/build_and_test.sh`)

| Claim | How |
|---|---|
| Public headers are self-contained and warning-free (`-Wall -Wextra -Wpedantic -Werror`, C++20 and C++23) | compiled each alone |
| Config validation, presets & F6 cycle order, conversions | 124 unit checks |
| `Config::classic()` makes the Engine behave identically to the v4.6 decision logic | **differential test**: 1.6M random steps (inputs, ticks, deaths, mode/dual/platformer changes, preset changes) vs `tests/reference_v46.hpp`, 0 divergences; generator confirmed to hit buffering, queued-opposite, spam-bypass and failsafe paths |
| That test can actually fail | 5 hand-made mutants (spam threshold, queued opposite, wave heuristic switch, failsafe condition, delay off-by-one) were each caught |
| Each deliberate change from v4.6 behaves as documented | unit tests (press-release-press, ordering/spacing, queue overflow, P2 swallow rule, platformer bypass, ship bypass in both modes, failsafe policies, flush-on-config-change) |
| No heap allocation in the steady-state hot path (both queue modes) | `tests/alloc_test.cpp` counts `operator new` over 200k iterations |
| No memory errors / UB in the core | the unit and differential tests also run clean under ASan + UBSan |
| The Geode-layer code is internally consistent (no typos, signature drift, wrong member names versus what the original used) | `tools/syntax_check_stub.sh`: `-fsyntax-only` against a **fake** Geode (`tests/geode_stub`). **This is not a build against the real SDK.** |

Caveat on the differential test: `tests/reference_v46.hpp` is *my transliteration* of the original logic, and
the predictor is a deterministic mock. It proves the refactor preserved decisions; it does not prove the
reference matches the game or that real simulations behave like the mock.

## NOT verified (read before shipping)

1. **Nothing was compiled against the real Geode SDK.** Expect to fix compile errors on first build.
   Specific risks, in my estimated order of likelihood:
   * `GJGameLevel::isPlatformer()` (platformer detection; not in the original). Isolated in one function; build with
     `-DPBUF_NO_PLATFORMER_DETECT=ON` to remove it.
   * `setHookPriority` vs the `setHookPriorityPre/Post` names shown in the current hook-priority tutorial
     (the original used `setHookPriority`). One macro: `PBUF_OUTERMOST` in `Hooks.cpp`.
   * `listenForKeybindSettingPresses` callback: the docs list `(Keybind const&, bool, bool, double)` but do not
     name the bools; I assumed (down, repeat). Return value `ListenerResult::Propagate` is an assumption.
   * `GEODE_EVENT_EXPORT` with by-value `std::string`/struct/`shared_ptr` arguments and `Result<struct>` returns
     (the docs' example uses ints).
   * `Loader::getLoadedMod`, `Mod::isLoaded` (v5 rename), `gd::vector::clear()`.
2. **Gameplay behavior is untested.** Nothing here was run in Geometry Dash. The simulation code is a port of
   v4.6's and inherits its unverified assumptions (`copyAttributes` as a full clone, `dt / timeWarp`,
   the 15/10-tick "landed" heuristic).
3. **The new behaviors are unplayed.** `OrderedSpacing`, platformer bypass, `LinkedOnly`, `ReleaseOnly` are logically
   tested, but whether they *feel* better than v4.6 (or break some level) needs playtesting.
4. **v4.7 differences** ("Zero-Lag + Up Arrow Fix") are unknown (source not supplied).
5. **The extended build's horizon change** is unknown and not ported (only its delay values are).
6. **Cross-mod behavior**: the API-mod dependency flow (auto-install, load order, ownership takeover when a
   consumer is disabled at runtime) was not exercised.
7. **Performance on real levels.** The allocation claim is for the engine; the per-input simulation cost is
   v4.6's (bounded by `Budget` if you set one).
8. **Platforms**: only Windows is declared; others untested.

## Suggested first-build checklist
1. `geode build` the API mod; fix any compile errors listed above.
2. Build `examples/custom-buffer`; confirm the dependency headers resolve.
3. In game: with `Config::classic()` the feel should match Predictive Buffer v4.6 at the same stage.
4. Then try `recommended()`; play a wave section with fast taps and a respawn-while-holding.
5. Enable `log-decisions` and compare "held back N ticks" lines against expectations.
