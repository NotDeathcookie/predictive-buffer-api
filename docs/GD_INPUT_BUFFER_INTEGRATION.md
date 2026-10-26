# GD-Input-Buffer integration

What's here now, what it is, and what's still missing before it does anything
in a real game. Written so "the whole engine" request doesn't quietly turn
into "whatever got pasted in" - this file is the honest map.

## What was ported verbatim (used with permission)

`include/pbuf/LegacyCore.hpp` - a straight transliteration of GD-Input-Buffer's
`src/BufferCore.hpp` (namespace renamed, nothing else). This is the part of
that mod its own author kept free of Geode/cocos specifically so it could be
unit-tested on its own - which is exactly what makes it portable here. It
brings in:

- **`searchDelay()`** - a strictly richer version of this repo's existing
  `TrajectorySearchStrategy` search: a dense tick-by-tick scan right after the
  click (narrow safe gaps near the click are common and a coarse stride can
  step over them), a CBF timing guard, an optional safety margin, and "aim"
  (ride further into a safe window instead of firing on the first tick that
  clears it - what an Extreme-style stage wants).
- **`InputQueue`** - game-time scheduling for the held input and everything
  that arrives while it's held, keeping exact spacing (hold lengths, double
  taps).
- **`SpotMemory` / `OutcomeTracker`** - "learn from attempts": remembers
  where your inputs landed on earlier tries and whether you survived, so a
  spot that's proven can override what the live prediction says.
- **`pickOrbTick` / `AimProbe` / `AimPick`** - the orb-aim logic: given a
  probe of "what does a press on tick d do", picks the tick that survives
  longest, preferring the middle of the longest safe run.
- **`Clearance` helpers, `MoverTrack`, `Calibrator`, `PredictionTracker`** -
  hazard-distance measurement, moving-hazard tracking, auto-window
  calibration from your own timing, and live prediction-accuracy stats.

Checked against the original author's own test suite (`tests/legacycore_test.cpp`,
ported from `tests/core_tests.cpp`): 666 checks, all passing.

`include/pbuf/AdvancedStrategy.hpp` is new glue (not derived from either mod)
that exposes `searchDelay()` as a second `IBufferStrategy` -
`"pbuf.trajectory.advanced"` - selectable via `Config::strategyId`, so a
consumer mod opts in without the default `"pbuf.trajectory"` behavior
changing for anyone else. 12 tests, all passing.

## What's new, not ported (because there was nothing separable to port)

GD-Input-Buffer's level-scan and orb-learning features are NOT implemented as
their own reusable functions in the source - they're methods and member
variables on the same single `$modify(PlayLayer)` class as the window UI, the
stats screen and everything else (10.6k lines in `src/main.cpp`). There's no
`BufferCore.hpp`-style file for them to lift out. So instead of pretending to
"port" something that doesn't exist as a separable unit, I wrote new
implementations of the *behavior the changelog documents*:

- **`include/pbuf/LevelScan.hpp`** - a generic branch/prune/select search: up
  to N candidate "versions" of a run, each tried both ways (keep / switch) at
  every decision point, survivors kept spread across a caller-defined metric
  (not just ranked by progress, so the search doesn't collapse into
  near-identical copies), the furthest-with-fewest-inputs branch wins. Tested
  against a synthetic press/release puzzle (`tests/level_scan_test.cpp`,
  11 checks) - this proves the search strategy works, not that any game's
  physics are modeled right, because it has no idea what a spike or a portal
  is.
- **`include/pbuf/OrbLearning.hpp`** - a keyed table (orb type + gamemode +
  size + gravity, however the caller wants to encode that into one integer)
  of observed y-speed and gravity-flip outcomes, with a running average and a
  same-orb-different-context fallback. Tested in isolation
  (`tests/orb_learning_test.cpp`, 16 checks).

## What's still missing - and why it isn't guessed at here

Both new modules are pure algorithms with no idea what Geometry Dash is. To
actually scan a level or learn a real orb, something needs to:

1. **Clone a simulated player's full state** (`IScanBranch::clone()`) - GD-Input-Buffer's
   "hidden copy of your player." This repo's `GeodeBackend` already creates
   one simulated player per probe (`createTrajectoryPlayer` in
   `docs/LICENSING_REVIEW.md`'s provenance table) but not a *forkable* one
   that a branching search can copy many times per decision point.
2. **Detect portals, gamemode switches, and size changes inside a branch's
   `step()`** so the scan can follow them instead of just dying at them.
3. **Detect a real orb use** (vs. the sim's `isActivated`-is-too-early
   problem GD-Input-Buffer's changelog describes) to feed `OrbLearning::record()`
   from actual gameplay, and apply `OrbLearning::lookup()` back into whatever
   predicts a press-on-an-orb's outcome.
4. Only after (1)-(3): wire `runScan()` into a Geode hook (pause-menu button,
   `ScanRunner`-equivalent) with the UI bits (Show scan, Export report, Real
   game/practice fallback) GD-Input-Buffer's window provides.

None of that is written here. I didn't invent Geode/`PlayLayer`/`GameObject`
API calls for it, because this environment has no Geode SDK to compile or
test them against (see `docs/VERIFICATION.md` - the existing `GeodeBackend.*`
was never build-verified either), and guessing at specific member names and
call signatures for code that actually pokes at the game would be far more
likely to silently misbehave than to work. That part needs doing against your
real Geode build, where it can actually be compiled and tried in-game - happy
to pair on it from here once you can report back what compiles and what the
in-game behavior looks like.
