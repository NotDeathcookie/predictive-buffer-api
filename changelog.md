# v0.1.0
- First pre-release: public API (`Config`, presets, strategies, observers, `Buffer`), engine core, Geode backend and hooks.
- `Config::classic()` reproduces Predictive Buffer v4.6 decisions (differential-tested); `recommended()` adds fixes for
  the collapsed press-release-press burst, phantom presses after respawn, two-player probing, platformer levels, and
  dropped input when changing presets.
