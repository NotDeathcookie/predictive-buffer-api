# API reference (v0.1)

All public headers are in `include/pbuf/`. Everything except `Api.hpp` is Geode-free.
Units are **ticks** (stock 240 TPS: 4 ticks = 1 frame @ 60 fps).

## `pbuf::Buffer` (Api.hpp) — what most mods use

```cpp
pbuf::Buffer buffer(GEODE_MOD_ID);              // your mod's own id
geode::Result<> open(Config = Config::recommended());   // checks API presence + version, acquires ownership
geode::Result<> setConfig(Config);   geode::Result<Config> config() const;
template<class F> geode::Result<> modify(F&&);           // read-modify-write: [](Config& c){ ... }
geode::Result<> setPresets(std::vector<Preset>);
geode::Result<std::string> applyPreset(std::string id);  // returns the preset label
geode::Result<std::string> cyclePreset();                // next preset, wraps around
geode::Result<> registerStrategy(std::shared_ptr<IBufferStrategy>);
geode::Result<> addObserver(std::shared_ptr<IObserver>);
geode::Result<Stats> stats() const;
// destructor releases ownership
```
Every call returns a `Result`; errors are sentences (e.g. `unknown preset 'x' (available: classic-1, …)`,
`the buffer is already owned by 'other.mod'`, `invalid config: limits.normal must be in [0, 480], got -1`).
A failed `setConfig` changes nothing. The free functions in `pbuf::api` (`acquire`, `release`, …) take an
`owner` string and do the same thing without the handle.

## `Config` (Config.hpp)

| Field | Default (`recommended`) | Meaning |
|---|---|---|
| `enabled` | true | master switch |
| `limits.normal / .wave` | 2 / 2 | max ticks an input may be held back |
| `horizons.normal/.wave/.miniWave` | 160 / 65 / 50 | lookahead per prediction |
| `tuning.*` | v4.6 constants | heuristics of the default strategy |
| `spam.{enabled,intervalTicks,streakRequired,appliesTo}` | true, 8, 3, {Wave} | rapid-input bypass |
| `rules.bypass` | {Ship} | modes that are never touched |
| `rules.releaseMatters` | {Wave, Robot} | modes where releases are bufferable |
| `rules.continuous` | {Wave, Ufo, Swing} | modes without the "landed ⇒ safe" shortcut |
| `rules.waveLike` | {Wave} | modes using wave limits/horizons/heuristics |
| `rules.bypassPlatformer` | **true** (classic: false) | |
| `behavior.queue` | **OrderedSpacing** (classic: LegacySingleSlot) | |
| `behavior.failsafe` | **ReleaseOnly** (classic: MatchPhysical) | |
| `behavior.dualProbe` | **LinkedOnly** (classic: AlwaysMirror) | |
| `behavior.bufferReleases` | true | false = only presses are ever delayed |
| `budget.maxSimTicksPerInput` | 0 (unlimited) | CPU guard per input |
| `strategyId` | `"pbuf.trajectory"` | which strategy decides |

`Config::classic()` reproduces v4.6 (including its defects). `Config::validate()` returns a list of problems.

## Presets (Presets.hpp)

`Preset{id, label, enabled, limits}`; `presets::classic()` (2/2, 4/4, 8/8, 16/8, OFF),
`presets::extended()` (adds 24/32/48/54/60), `presets::make(id, all, wave)`, `presets::off()`.
`PresetBook` is the cycling logic (usable client-side). Applying a preset changes only `enabled` and `limits`.

## Writing a strategy (Strategy.hpp)

```cpp
class MyStrategy final : public pbuf::IBufferStrategy {
  std::string_view id() const noexcept override { return "my.mod.name"; }   // not "pbuf.*" (reserved)
  pbuf::Decision decide(pbuf::InputContext const& ctx, pbuf::IProbe& probe) override {
      // probe.survival(delay, push, ctx.horizon): ticks survived if the input is applied `delay` ticks from now
      // return {delay in [0, ctx.maxDelay]}; anything else is rejected and the input passes through
  }
};
```
Rules: fast, non-throwing, don't keep `probe`. Honor `ctx.maxSimTicks` if you loop (`probe.simulatedTicks()`).
Compose the default with `pbuf::TrajectorySearchStrategy` (header-only). Select via `Config::strategyId`.

## Observers (Observer.hpp)

`IObserver::{onBuffered, onPassThrough(push, PassReason), onFired, onConfigChanged}`. Optional; with none
registered the engine formats and prints nothing. Called synchronously on the game thread.

## Ownership

One mod owns the buffer at a time. `acquire` fails if another *loaded* mod owns it. If the owner is no longer
loaded the next `acquire` takes over and its observers/strategies/presets are discarded. `release` (or `~Buffer`)
disables the engine.

## Not part of the API

`src/core/*` and `src/geode/*` (Engine, schedulers, backend) may change in any release.
