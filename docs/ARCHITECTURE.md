# Architecture

## Layers

```
 consumer mod(s)                      include/pbuf/ (public, bundled into the .geode)
 ──────────────                       ────────────────────────────────────────────────
 pbuf::Buffer ──► api::* (event exports, Api.hpp) ─ ─ ─ ─ ─ ─ ┐  Geode routes the call
                                                               ▼  between mods
 API mod "Predictive Buffer API"      src/geode/   (needs Geode SDK)
 ──────────────────────────────       Runtime  (owner, presets, strategy registry)
                                      Hooks    (handleButton, update, destroyPlayer, 13 suppressors)
                                      GeodeBackend (sim players, probe, fire)
                                             │ IBackend / IProbe
                                      src/core/    (NO Geode dependency; unit-tested)
                                      Engine  ── IScheduler (Legacy | Ordered)
                                           └──── IBufferStrategy (default: TrajectorySearch)
```

The requirement "separate input interception, trajectory simulation, buffering logic, configuration and
Geometry Dash integration" maps to:

| Concern | Where |
|---|---|
| Input interception | `Hooks.cpp` (`handleButton` → `Engine::onInput`) |
| Trajectory simulation | `GeodeBackend::survival` behind `IProbe` |
| Buffering logic | `Engine` + `IScheduler` + `IBufferStrategy` |
| Configuration | `Config.hpp`, `Presets.hpp` (plain data) |
| GD integration | `src/geode/*` only |

**The seam that makes this testable:** the Engine never touches the game. It asks `IBackend::query()` for a
small `GameState` snapshot, asks `IBackend::beginProbe()` for a predictor, and calls `IBackend::fire()`.
Tests substitute a mock; production substitutes `GeodeBackend`.

## Life of one click

1. GD calls `GJBaseGameLayer::handleButton(down, 1, isP1)`. The hook (outermost priority) checks two bools;
   if a mod owns the buffer and we are not simulating, it calls `Engine::onInput`.
2. `onInput` records the physical state, applies bypass rules (disabled, dead, platformer, ship), queues the
   input if one is already held back, applies the spam guard and release rules, then builds an `InputContext`.
3. The strategy calls `probe.survival(delay, push, horizon)`. The probe syncs two hidden players to the
   real ones, applies the input after `delay` ticks, and steps `update()` + `checkCollisions()`.
   While `g_probing` is set, the hooks suppress every side effect.
4. `Decision::delay == 0` → the click is returned to the game untouched. `> 0` → the scheduler holds it and
   `onInput` returns `false` (the hook swallows the original call).
5. Every `PlayerObject::update` of player 1 calls `Engine::onTick`; the scheduler fires due inputs through
   `IBackend::fire`, which calls `handleButton` again. `Engine::isFiring()` lets that re-entry through.

## Design decisions

* **One engine, one owner.** Hooks exist once, in the API mod. Consumers cannot double-buffer, and a fix to
  the simulation reaches every consumer. `acquire()` fails with a clear error if another loaded mod owns it.
* **Strategies see a probe, not the game.** A strategy cannot corrupt game state, can be tested with a fake
  probe, and can compose the default (see `examples/custom-buffer`).
* **Behaviors that differ from v4.6 are switches**, and `Config::classic()` flips them all back. The
  differential test proves `classic()` ≡ the original decision logic.
* **No allocation in the hot path** (checked by `tests/alloc_test.cpp`): fixed ring buffer, no `std::function`,
  observers cost nothing when absent, the collision filter reuses one vector.
* **Cheap when idle.** With no owner, each hook costs two bool loads. Simulated players are not even created.
* **RAII for every flag** that suppresses game behavior (`ProbingFlag`, `Engine::fire` guard), so an early
  return can't leave the game stuck in "simulating" mode.

## Threading and lifetime

Everything runs on the game's main thread. Observers and strategies are held by `shared_ptr` by the API mod
and removed on `release()` or when the owning mod is no longer loaded. Geode keeps a disabled mod's code
mapped, but call `release()` (the `Buffer` destructor does) so the engine stops calling into it.

## Known limits (by design, inherited)

See `ORIGINAL_ANALYSIS.md` #4: prediction ignores orbs, pads, portals and triggers. The extension points
(`IBufferStrategy`, and `IProbe`/`IBackend` for backend work) exist so the community can improve that
without forking hooks.
