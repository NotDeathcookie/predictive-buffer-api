# Analysis of the original Predictive Buffer

Everything here was derived by reading the supplied archive. Where something could
not be verified, it says so.

## 1. What the archive actually contains

| Item | What it is |
|---|---|
| `Predictive-Buffer-main.zip` | The **source**: `src/main.cpp` (665 lines), `CMakeLists.txt`, `mod.json` (`id: ai.predictivebuffer`, `developer: lilWC`, `version: v1.0.0`, Geode `5.11.2`, GD `2.2081`, Windows only), a one-line `README.md`. Source banner: **v4.6**. |
| `PredictiveBuffer.geode` | A **compiled** build. Its log string says **v4.7 "Zero-Lag + Up Arrow Fix"**. |
| `predictive buffer files/PredictiveBuffer_extended.geode` | A second compiled build, the same size as the first. |
| `predictive buffer files/patch_predictive_buffer.py` | A script that **patches the compiled DLL's bytes** to enlarge the delay stages. |
| `files.zip`, `predictive buffer files.zip` | Copies/subsets of the above. |
| `GD-Input-Buffer-5265b3c…zip` | A **different project** by a different author (see §6). |

Two consequences matter for everything else:

* **The source is older than the binary.** v4.6 source vs v4.7 binary. Whatever "Zero-Lag" and
  "Up Arrow Fix" changed is **not in the supplied source**, and I did not reverse-engineer it. This
  library is based on v4.6. If v4.7 changed decision logic, the "classic" preset reproduces v4.6, not v4.7.
* **The "extended" build is not a source build.** 631 bytes differ from the standard DLL; the patcher
  edits hard-coded instruction addresses. It cannot be maintained, ported, or reviewed as source. The
  *values* it introduces (24/32/48/54/60 ticks) are kept as `presets::extended()`. The patch also
  changes some other quantity (it adds `limit - 16` to a register when the limit exceeds 16, which looks
  like a horizon adjustment); **I did not port that and could not verify what it does.**

## 2. Architecture of v4.6

One file, one global singleton `TrajectoryBufferSim s_sim`, plus ~15 `$modify` hooks.

```
 player clicks ─► GJBaseGameLayer::handleButton (hook) ─► s_sim.interceptInput()
                                                             │ decide delay by simulation
                                                             ▼
                 two hidden PlayerObjects ("sim players") ◄── getSurvivalTicks(delay, push, horizon)
                      copy real player → apply input after `delay` → step update()+checkCollisions()
                                                             │
 PlayerObject::update (hook) ─► s_sim.onPlayerUpdate() ──────┘ counts down, then m_SimFireButton()
                                                               = pl->handleButton(...) re-entering the hook
```

* **Prediction.** For a press, simulate up to a horizon (160 ticks; wave 65; mini wave 50) with the
  press at delay 0. If the player survives the horizon, pass the click through. Otherwise try delays
  1..max and take the first that clears the horizon, or one that beats the best by > 12 ticks. Wave uses
  a stricter "gain must exceed 2·delay + 6/10" rule because delaying a wave turn gains ticks for free.
* **Suppressing side effects.** While simulating, the hooks listed at that priority no-op or filter (ring/pad/trigger activation, gravity
  flip, jump counter, shine/dash effects, end animation) at hook priority −1,000,000. Per the Geode hook
  priority docs the chain starts at the *lowest* priority, so this is the **outermost** layer: other mods
  never see simulated calls. That is a good design and is preserved.
* **Collision.** The collision hook filters to Solid / Hazard / AnimatedHazard / CollisionObject / Slope.
  A simulated death is detected by hooking `destroyPlayer` for the sim player.
* **State machine.** `m_waitTicks`: −1 idle, ≥0 countdown, −2 "queued opposite action due". Plus
  `m_bufferedPush`, `m_queuedOpposite`, `m_physicalHolding`, `m_gameHolding`.
* **Mode rules.** Ship: never buffered. Release is buffered only for wave/robot. "Continuous" modes
  (wave, UFO, swing) never use the "landed, therefore safe" early exit.
* **Dual.** One-player dual: P2's copy of the click is swallowed while P1's is held back, and both are
  fired together later.
* **Spam guard.** 3 inputs in a row each < 8 ticks apart bypass the buffer for wave.
* **Presets.** F6 cycles 2/2 → 4/4 → 8/8 → 16/8 → OFF (all-modes / wave ticks), via `GetAsyncKeyState`.

## 3. Dependencies

Geode 5.11.2, GD 2.2081, Windows only (`<Windows.h>`, `AllocConsole`, `GetAsyncKeyState`). No third-party
libraries. `CMakeLists.txt` sets **C++20**, but the Geode v5 migration guide states v5 requires **C++23**
(this repo uses 23).

## 4. Findings (verified by reading unless marked)

Numbered because other docs and tests refer to them.

1. **Source/binary mismatch** (v4.6 vs v4.7), above.
2. **Binary-patched extended build**, above.
3. **Platform and I/O.** `AllocConsole()`/`freopen_s` on *every launch* with no opt-out; `printf` on the
   gameplay thread for each decision; F6 via polling `GetAsyncKeyState(...) & 1`, which is Windows-only
   and its "pressed since last call" bit can be consumed by any other caller. None of it is configurable.
4. **Interactive objects are invisible to the predictor.** Only solid/hazard/slope geometry is collided
   with; orbs, pads, portals, speed/gravity/size changes and triggers are skipped. A prediction that
   crosses one is a prediction of a different level. Larger delays make this worse (an orb click delayed
   far enough can miss its window). This is the largest accuracy limit and is **not fixed** here; it is
   the main reason `IProbe`/`IBufferStrategy` are replaceable.
5. **Platformer levels are not special-cased.** Left/right buttons are not simulated, so predictions there
   are unreliable. *Changed:* `ModeRules::bypassPlatformer` (default on in `recommended()`, off in `classic()`).
6. **Two-player mode.** The probe presses P2 whenever dual is active, even when P2 has its own button.
   *Changed:* `DualProbe::LinkedOnly` (default); `AlwaysMirror` = v4.6.
7. **Press→release→press inside one window loses the release.** The single `m_queuedOpposite` flag is
   overwritten by the second press, so the game sees one long press instead of two taps; and a queued
   release always fires exactly 1 tick after the press whatever the real spacing was. With the 2-tick
   stages this is rare; with 8/16-tick and the extended stages it is reachable by fast tapping.
   *Changed:* `QueueMode::OrderedSpacing` (default) delays every input of a burst by the same amount in
   order; `LegacySingleSlot` = v4.6. Pinned by `legacy_loses_release_in_press_release_press_burst__ordered_keeps_it`.
8. **The failsafe can inject presses.** Whenever idle, if the game isn't holding but the finger is, it
   presses. After a death while still holding, that is a phantom press at respawn. (Whether vanilla GD
   would also hold across a respawn I could not check offline.) *Changed:* `FailsafePolicy`
   (`ReleaseOnly` default; `MatchPhysical` = v4.6). Pinned by a test.
9. **Cycling presets drops a pending input** (`m_waitTicks = -1`). *Changed:* it is delivered immediately.
10. **Spam-streak undercount.** Inputs absorbed while a buffer is pending reset the gap timer but do not
    count towards the streak. *Preserved* (bit-exact in `classic`; differential-tested); `ordered` keeps the same rule.
11. **Unit assumptions.** "Ticks" and comments like "4 ticks = 1 frame" assume 240 TPS. Presets stay in ticks; use
    `ticksFromFrames60(frames, tps)` if a TPS-changing mod is in play.
12. **Cost.** Per input: up to (max delay + 1) simulations of up to `horizon` ticks, 2 players when dual, and
    the collision hook allocated a fresh vector **per simulated tick**. No budget. *Changed:* the vector is
    reused (`GeodeBackend::scratch`), and `Budget::maxSimTicksPerInput` exists (default unlimited = v4.6).
13. **Lifetime.** `init()` called twice without `quit()` leaks both sim players; they are never removed from
    their parent. *Changed:* `shutdown()` removes and releases, and `PlayLayer::init` always calls it first.
14. **Nothing persists.** Every launch starts at stage 1; there are no Geode settings.
15. **Unverified claims.** README: "Doesn't matter if Click Between Frames is on." Not verified.
    `copyAttributes` as a full state clone, and `dt / m_timeWarp`, are inherited from the original
    (it says it follows the Show Trajectory mod) and not independently checked.

## 5. What became API vs what stayed internal

| Original behavior | Becomes |
|---|---|
| Delay search + heuristics (4–5 of `interceptInput`) | `IBufferStrategy`, default `TrajectorySearchStrategy`, constants in `SearchTuning` |
| Simulation (`getSurvivalTicks`) | `IProbe` (public, so strategies can be tested); implementation internal (`GeodeBackend`) |
| `m_waitTicks` machine | internal `IScheduler` (`Legacy` + `Ordered`), chosen by `Behavior::queue` |
| Dual / P2 swallow rule | engine, driven by `GameState::linked()` |
| Ship bypass, release rules, continuous modes | `ModeRules` (data, not `if`s) |
| Spam guard | `SpamGuard` |
| 160/65/50 horizons, 2/2…16/8 stages | `Horizons`, `DelayLimits`, `presets::*` |
| F6, console, printf | removed from the library; hotkeys are the consumer's, logging is `IObserver` |
| The simulation-suppression hooks | internal to the API mod (one set, shared by all consumers) |

## 6. The second project in the archive: GD-Input-Buffer

`GD-Input-Buffer-5265b3c…` is an independent mod (separate author per its `mod.json`; a 10.6k-line
`main.cpp`, a 1k-line pure `BufferCore.hpp`, ~1k lines of tests, its own changelog). It only *refers* to
Predictive Buffer, as a point of comparison in comments and its changelog. It contains no Predictive
Buffer source that I could identify. **I did not copy code from it**; I read it to understand the problem
space (notably that a Geode-free core is easy to test, which informed this design). Its statements about
Predictive Buffer's behavior were treated as claims to check, not facts. It also has no top-level license
file (only an OFL for a bundled font). See `LICENSING_REVIEW.md`.
