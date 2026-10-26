# Distribution model: options and decision

Question: how should other Geode projects depend on this library?

## Facts established from the Geode docs (https://docs.geode-sdk.org/mods/dependencies, /mods/configuring, v5 migration guide)

* A mod can be a **dependency of other mods**. Its `.geode` ships headers (`mod.json` → `api.headers`),
  and Geode CLI ≥ 1.4 adds `build/geode-deps/<id>` to the consumer's include path.
* Since Geode v4.3.0, `GEODE_EVENT_EXPORT` exports functions across mods. Constraints: `inline`, return
  `geode::Result`, defined once with `GEODE_DEFINE_EVENT_EXPORTS`, **not callable from inside the exporting
  mod**, `MY_MOD_ID` set manually.
* The docs say plain C++ libraries should be installed "like any other C++ project" (CPM, submodules…).
* v5: `dependencies` is an object map with `"required"`; C++23 required.
* Index guidelines (https://docs.geode-sdk.org/mods/guidelines): mods must not be incompatible with mods that
  do the same job; mods built on someone else's work need evidence of the original author's approval and
  credit; mostly-AI-generated codebases the author does not understand are rejected.

## Options

| | A. Geode API mod (**chosen**) | B. Source/static lib embedded in each mod | C. Header-only | D. Separate DLL outside Geode |
|---|---|---|---|---|
| Hooks | Once | Every consumer re-hooks | Every consumer re-hooks | Own loader problem |
| Two buffer mods installed | Second `acquire` fails with a clear error | Both hook `handleButton`: double buffering | Same | Same |
| Fix reaches users | Update one mod | Every consumer must rebuild + re-release | Same | Same |
| Consumer size / boilerplate | ~20 lines | Copy hooks, CMake object-library quirks | Same | N/A |
| User must install a 2nd mod | **Yes** (auto-installed only once listed on the index) | No | No | Yes, outside Geode |
| ABI coupling | **Yes** (struct layout, vtables across mods) | None | None | Yes |
| Uses documented Geode mechanisms | Yes | Yes | Yes | No |
| Index "compatible with similar mods" guideline | Satisfied by design | At risk | At risk | n/a |

## Decision: A, with the engine core kept Geode-free

* It is the only option where the problem the project exists to solve (many forks of the same hooks) actually
  goes away, and where the "one buffer at a time" rule can be enforced.
* The Geode-free `src/core` keeps the logic testable and leaves option B open: a consumer that cannot accept a
  dependency can compile `src/core` plus their own backend. That path is **not** supported or tested here.

## Costs you accept with A (be aware)

1. **Binary compatibility between mods.** `Config`, `Preset`, the strategy/observer interfaces and `Stats`
   cross the boundary by value or vtable. A layout change breaks consumers silently, so `Buffer::open`
   enforces an exact major.minor match while 0.x (see `VERSIONING.md`). This only works because Geode requires
   all mods to share a toolchain/ABI.
2. **Event exports are young and constrained** (see above). Not compiled here against the real SDK.
3. **Users need two mods.** Until the API mod is on the Geode index, a consumer's players must install it by hand.
4. **Listing on the index has conditions** (licensing/approval, understanding of the code). See `LICENSING_REVIEW.md`.
