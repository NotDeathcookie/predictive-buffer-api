# Versioning and compatibility

**Semantic Versioning of the public API** (`include/pbuf/`). `PBUF_API_VERSION_*` in `Version.hpp`; the API mod's
`mod.json` version is the same number.

## Why binary compatibility is the real issue
`Config`, `Preset`, `Stats`, `InputContext`, `Decision` and the `IBufferStrategy` / `IObserver` / `IProbe`
vtables cross between two separately compiled mods. Adding a field or virtual function changes the layout.
Source compatibility is not enough.

## Rules
* **0.x (now):** the *minor* number is the compatibility number. `Buffer::open()` requires the installed
  API's `major.minor` to equal the one the consumer was built against; patch may differ. Any change to a
  public struct or interface bumps the minor and requires consumers to rebuild.
* **≥ 1.0:** layout/vtable-affecting changes bump MAJOR. Minor releases may only add free functions to
  `api::` and new headers; `open()` accepts same-major, installed ≥ built.
* New `Config` fields are appended with defaults equal to current behavior. `Config::classic()` never changes
  meaning: it is the v4.6 compatibility anchor, guarded by `tests/differential_test.cpp`.
* Strategy ids `pbuf.*` are reserved; the built-in `pbuf.trajectory` keeps its algorithm within a major version.
* Presets: ids `classic-N`, `extended-N`, `off` are stable.

## Platform / game compatibility
Declared in `mod.json`: Geode `5.11.2`, GD `2.2081`, Windows. Nothing in `src/` is Windows-specific
(the original's `Windows.h` use was removed), but other platforms are **untested**; add them to `mod.json`
only after building and playing on them. A GD or Geode update that changes any hooked signature is a patch
release of the API mod; consumers need not rebuild.

## Deprecation
Deprecated API stays for one minor (0.x: one release) with `[[deprecated("use X")]]` before removal.
