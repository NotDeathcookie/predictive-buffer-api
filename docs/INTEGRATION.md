# Integration guide

Prerequisites: Geode SDK 5.x, Geode CLI ≥ 1.4, a C++23 compiler, GD 2.2081 (Windows; see VERIFICATION.md).
**These steps have not been executed** in the environment that produced this repo (no SDK available).

## 0. One-time: pick your id
```
python3 tools/set_mod_id.py yourname
```
Edit `"developer"` in both `mod.json` files. Ids must agree in `mod.json`, `Api.hpp` (`MY_MOD_ID`) and includes.

## 1. Build and install the API mod
```
export GEODE_SDK=/path/to/geode
cd predictive-buffer-api && geode build        # produces yourname.predictive-buffer-api.geode
```
Install the `.geode` in GD's `geode/mods` folder. It does nothing until a consumer acquires the buffer.
If the build fails at platformer detection, configure with `-DPBUF_NO_PLATFORMER_DETECT=ON` (see VERIFICATION.md).

## 2. Create a consumer mod
`mod.json`:
```json
"dependencies": { "yourname.predictive-buffer-api": ">=v0.1.0" }
```
This is the v5 object form (required dependency). The Geode dependency docs describe the object form with
`"required": false` if you want it optional.

`CMakeLists.txt`: nothing special; `setup_geode_mod()` puts the dependency's headers on the include path.

`main.cpp` — the minimum:
```cpp
#include <Geode/Geode.hpp>
#include <yourname.predictive-buffer-api/include/pbuf/pbuf.hpp>
static pbuf::Buffer g_buffer(GEODE_MOD_ID);
$on_mod(Loaded) {
    if (auto r = g_buffer.open(pbuf::Config::classic()); r.isErr())
        geode::log::error("{}", r.unwrapErr());
}
```
That is a working buffer with v4.6 behavior. For presets, strategies, observers, settings and a hotkey see
`examples/custom-buffer/src/main.cpp`.

## 3. Choosing `classic()` vs `recommended()`
`classic()` = v4.6 bit-for-bit (known defects included). `recommended()` = fixes from `ORIGINAL_ANALYSIS.md`
#5–#9. If you want most fixes but one old behavior, start from `recommended()` and change that one field.

## 4. Testing without the game
Strategies and config logic are plain C++: `tools/build_and_test.sh` shows how (fake probe, mock backend).

## Troubleshooting (messages you may see)
| Message | Cause |
|---|---|
| `Predictive Buffer API is not loaded` | API mod missing/disabled, or its id differs from `MY_MOD_ID` in the header you compiled against |
| `…version mismatch: built for 0.1 and installed is 0.2` | rebuild your mod against the installed API's headers |
| `…already owned by 'x'` | another buffer mod is active; only one can be |
| `invalid config: …` | see `Config::validate()` |
