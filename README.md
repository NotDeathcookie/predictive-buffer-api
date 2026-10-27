# Predictive Buffer API

A reusable foundation for Geometry Dash **input-buffer mods** on the Geode framework: the hooks, the
trajectory-prediction engine and the buffering state machine live in **one shared API mod**; your mod supplies
configuration, presets, hotkeys and (optionally) its own buffering strategy.

> **Status: v0.1, pre-release.** The engine core is tested (see below). The Geode integration layer and the
> example mod were written against the Geode docs but **have not been compiled against the real SDK or run in
> the game**. Read `docs/VERIFICATION.md` first. **Licensing is unresolved** for the parts derived from
> Predictive Buffer (`docs/LICENSING_REVIEW.md`, `NOTICE.md`); do not publish before settling it.

## Why

Every input-buffer variant today is a fork of the same ~650-line `main.cpp`: the same 15 hooks, the same
simulation, edited constants. With this repo a variant is a small mod:

```cpp
#include <ssilent.predictive-buffer-api/include/pbuf/pbuf.hpp>
static pbuf::Buffer g_buffer(GEODE_MOD_ID);
$on_mod(Loaded) {
    auto cfg = pbuf::Config::recommended();
    cfg.limits = {6, 4};                       // ticks (4 ticks = 1 frame @ 60 fps)
    if (auto r = g_buffer.open(cfg); r.isErr()) geode::log::error("{}", r.unwrapErr());
}
```

## What you get

* **`Config`**: delay limits, horizons, spam guard, per-gamemode rules, behavior switches, CPU budget. All data.
* **Presets** (`classic`: 2/2, 4/4, 8/8, 16/8, OFF; `extended`: up to 60 ticks) + a cycling `PresetBook`.
* **Strategies**: replace the delay-selection algorithm by implementing `IBufferStrategy` (compose the default if you like).
* **Observers** instead of console printing. **Clear `Result` errors** instead of crashes.
* **One owner at a time**, so two buffer mods can't fight over the same clicks.
* **`Config::classic()`** reproduces Predictive Buffer v4.6; `recommended()` adds documented fixes (`docs/ORIGINAL_ANALYSIS.md`).
* **`pbuf.trajectory.advanced`** (`AdvancedStrategy.hpp`): an opt-in strategy with a dense near-click scan, a
  safety margin and "aim" (ride into the safe window instead of firing the instant it opens) - from
  GD-Input-Buffer by AlQuerez, used with permission. See `docs/GD_INPUT_BUFFER_INTEGRATION.md`.
* **`LevelScan.hpp` / `OrbLearning.hpp`**: new, standalone-tested modules for a level-pathfinding scan and an
  orb-speed learning table. Pure algorithms only - wiring either into the real game still needs backend work;
  see `docs/GD_INPUT_BUFFER_INTEGRATION.md` for exactly what's left.

## Layout

```
include/pbuf/        public API (Api.hpp needs Geode; everything else is plain C++)
src/core/            engine, schedulers (no Geode dependency)       <- tested
src/geode/           hooks, simulation backend, API exports         <- needs the Geode SDK
tests/               unit, differential-vs-original, allocation tests; fake-Geode type-check stubs
examples/custom-buffer/   a separate consumer mod (custom preset, strategy, observer, settings, hotkey)
docs/                analysis of the original, architecture, distribution, API, integration, versioning,
                     verification, licensing review
tools/               build_and_test.sh, syntax_check_stub.sh, set_mod_id.py
```

## Try it

```
tools/build_and_test.sh          # needs only g++/clang++ (C++20). No Geode, no game.
tools/syntax_check_stub.sh       # type-checks the Geode layer against a FAKE Geode (not a real build)
python3 tools/set_mod_id.py you  # then: GEODE_SDK=... geode build   (see docs/INTEGRATION.md)
```

## Documentation

| | |
|---|---|
| `docs/ORIGINAL_ANALYSIS.md` | what the supplied archive contains, how the original works, 15 findings |
| `docs/ARCHITECTURE.md` | layers, the engine/backend seam, life of a click |
| `docs/DISTRIBUTION.md` | options considered and why a Geode API mod was chosen (and its costs) |
| `docs/API.md`, `docs/INTEGRATION.md` | reference and step-by-step |
| `docs/VERSIONING.md` | semver + the binary-compatibility rule |
| `docs/VERIFICATION.md` | exactly what was tested and what was not |
| `docs/LICENSING_REVIEW.md` | ownership, derived-file map, Geode index conditions |
| `docs/GD_INPUT_BUFFER_INTEGRATION.md` | what's ported from GD-Input-Buffer, what's new, what's still unwired |
