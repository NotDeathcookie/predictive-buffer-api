# License and attribution review

*I am not a lawyer; this is a factual review of the supplied files and a list of decisions that are yours
(or a lawyer's) to make. Nothing here grants or removes any right.*

## 1. What the archive says about ownership and licensing

| Project | Developer field | License file | License/copyright text in source |
|---|---|---|---|
| Predictive Buffer (`ai.predictivebuffer`, v4.6 source / v4.7 binary) | `lilWC` | **None** | **None** |
| Patch script + "extended" `.geode` | none stated (same id/developer as above inside the bundle) | None | None |
| GD-Input-Buffer (`alquerez.inputbuffer`) | `AlQuerez` | **None** (only an OFL for a bundled *font*, which covers the font only) | None |

Findings:

1. **No license was found for either mod.** Absent a license, the default legal position in most jurisdictions is
   that the author keeps all rights and others have **no permission** to copy, modify or redistribute. Having
   the files, or the files being public, does not change that. So I cannot tell you that you may fork,
   relicense, or publish a derivative of Predictive Buffer.
2. **Authorship of Predictive Buffer is not established by the archive** beyond the `developer: "lilWC"`
   field. The id prefix `ai.` and the shipped `about.md` ("crazy how there is a non Ai version of this mod
   people are using to beat top demons") suggest the mod is AI-assisted and that a non-AI mod with a similar
   idea exists. I do not know who wrote what, or whether the idea/code descends from that other mod.
3. **I do not know who you are relative to these projects.** If you are lilWC, you can license your own code
   however you like (and see §4). If not, you need lilWC's permission for anything derived from their code.
4. **GD-Input-Buffer is a separate author's work.** I did not copy from it (see ORIGINAL_ANALYSIS.md §6).
   Its code must not be merged in without its author's permission.
5. **Copyright in AI-generated code is unsettled** and varies by jurisdiction. That applies to the original
   *and* to the new code in this repository, which was written by an AI assistant (Claude) at your request.

## 2. Provenance map of this repository

**DERIVED from Predictive Buffer v4.6** (structure re-expressed, logic/heuristics/constants follow the
original; permission from the original author is needed to distribute these):

| File | Derived from |
|---|---|
| `include/pbuf/TrajectorySearch.hpp` | steps 4–5 of `interceptInput` |
| `src/core/Engine.cpp`, `Engine.hpp` | `interceptInput`, `onPlayerUpdate`, `m_SimFireButton` |
| `src/core/Scheduler.cpp` (`LegacyScheduler` only) | the `m_waitTicks` state machine |
| `src/geode/GeodeBackend.*` | `createTrajectoryPlayer`, `syncSimPlayer`, `getSurvivalTicks`, `setFrameDelta` |
| `src/geode/Hooks.cpp` | the `$modify` classes and the set of suppressed functions |
| `tests/reference_v46.hpp` | near-verbatim transliteration (test only) |
| default values in `Config.hpp`, `presets::classic()/extended()` | constants (spam 8/3, horizons 160/65/50, gains 2/6/10/12, stage values) |

**NEW** (no code from the original; designed around its observable behavior):
`Types.hpp`, `Strategy.hpp`, `Observer.hpp`, `Presets.hpp` (structure), `Version.hpp`, `Api.hpp`,
`OrderedScheduler`, `Runtime.*`, `ApiExports.cpp`, `main.cpp`, `tests/*` except the reference, `tools/*`,
`examples/*`, `tests/geode_stub/*`, and all docs.

"New" does not make this independent of the original: the project exists to expose the original's algorithm.

## 3. What I recommend

1. **Ask lilWC** (or whoever holds the rights) for explicit written permission, ideally a license grant
   (e.g. MIT) covering the derived files. Keep it in the repo. Offer credit and a link in `about.md`.
2. **Until then**, treat the derived files as *not redistributable*. Two honest options:
   * keep this repository private / unpublished, or
   * replace the derived files with an independent implementation written without reference to the original code
     (the public API, config, tests and docs can stay; the engine/backend/hooks must be rewritten).
3. **License your own new files deliberately.** `LICENSE` in this repo is an MIT *template* with a blank
   holder and an explicit scope clause (new files only). Fill it in or choose differently. Do not apply it to the
   derived files until (1) is settled.
4. If you also want to reuse ideas from GD-Input-Buffer, contact its author.

## 6. Update: GD-Input-Buffer code now included, by permission

Earlier revisions of this review (§1-§3 above) found no license for GD-Input-Buffer and concluded its code
"must not be merged in without its author's permission." The repository maintainer has since told me that
permission - specifically from AlQuerez, who they say is a personal friend - was given directly. I have no way
to independently verify a conversation that happened outside this chat, so I'm recording what I was told rather
than treating it as confirmed:

* **What changed:** `include/pbuf/LegacyCore.hpp` is now a transliteration of GD-Input-Buffer's
  `src/BufferCore.hpp` (namespace renamed to `pbuf::legacycore`, otherwise verbatim), carried over together with
  its own test suite (`tests/legacycore_test.cpp`, ported from `tests/core_tests.cpp`) so the port's correctness
  is checked against the original author's own test cases, not just mine. `AdvancedStrategy.hpp` is new glue code
  that wires this richer search into the existing `IBufferStrategy` interface. `LevelScan.hpp` and
  `OrbLearning.hpp` are new implementations of the *behavior* GD-Input-Buffer's changelog describes (see the
  banner comment in each file) rather than ports of its source, since the real implementations of those two
  features are not separable from GD-Input-Buffer's single `$modify(PlayLayer)` class the way `BufferCore.hpp`
  is.
* **What I'd still recommend before this gets published anywhere (not just kept for personal use):** get the
  permission into something more durable than a chat message - even a one-line "yes, go ahead - AlQuerez" posted
  somewhere with a timestamp (a Discord message screenshot, a comment on the fork, an email) works. If this is
  ever submitted to the Geode mod index, their guidelines (§4 below) explicitly ask for evidence the original
  creator is fine with a mod building on their work, not just a claim of it.
* **What this does NOT change:** the lilWC-derived material (§1-§3) is unaffected. No permission has been
  claimed for that, so `TrajectorySearch.hpp`, `Engine.cpp`/`.hpp`, `Scheduler.cpp` (the `LegacySingleSlot`
  branch), `GeodeBackend.*`, `Hooks.cpp`, `tests/reference_v46.hpp`, and the `classic()`/`lilWC`-derived defaults
  in `Config.hpp`/`Presets.hpp` remain not-redistributable until that's resolved, same as before.

## 4. Geode index conditions that touch this (from https://docs.geode-sdk.org/mods/guidelines)

* **Mods based on someone else's work** need evidence the original creator is fine with it, and credit.
  A mod that is "a modified copy of another mod" without that is grounds for rejection.
* **AI-generated code:** a codebase that is entirely or mostly AI-generated, where the submitter does not
  understand it, is rejected; using AI to boost productivity is acceptable. Both the original (`ai.` id) and
  this repository's new code are AI-written. If you plan to submit, **you must read, understand and be able to
  maintain every file here**, and expect reviewers to ask.
* Compatibility: a mod that conflicts with mods doing the same thing can be rejected. The single-owner design
  exists partly for this.

## 5. Gameplay-integrity note (not legal)

An input buffer changes which inputs the game sees. The original's `about.md` shows it is used on top-level
demons. Check the rules of any leaderboard/platform you care about (and consider Geode's `cheat` tag for
consumer mods); the API deliberately provides no way to hide that it is active, and `IObserver` makes
decisions inspectable.
