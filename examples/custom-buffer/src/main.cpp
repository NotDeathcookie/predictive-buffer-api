// Example consumer of the Predictive Buffer API.
//
// STATUS: not compiled against the real Geode SDK (none was available when this was written).
// Items that rely on documented-but-untested signatures are marked UNVERIFIED.
//
// Compare with the original: ~100 lines here replace a forked 665-line main.cpp, and none of
// the hooks, simulation or input handling is duplicated.
#include <Geode/Geode.hpp>
#include <Geode/loader/SettingV3.hpp>

#include <your-id.predictive-buffer-api/include/pbuf/pbuf.hpp>

using namespace geode::prelude;

namespace {

// --- 1. A custom strategy -------------------------------------------------------------------
// Composition: reuse the original search, then veto delays that don't clear the whole lookahead.
// Strategies see only the probe, never the game, so this is also unit-testable with a fake probe.
class StrictStrategy final : public pbuf::IBufferStrategy {
public:
    std::string_view id() const noexcept override { return "your-id.custom-buffer-example.strict"; }

    pbuf::Decision decide(pbuf::InputContext const& ctx, pbuf::IProbe& probe) override {
        pbuf::Decision d = inner_.decide(ctx, probe);
        if (d.delay > 0 && d.bestTicks < ctx.horizon) d.delay = 0;   // partial improvement: not good enough
        return d;
    }

private:
    pbuf::TrajectorySearchStrategy inner_;
};

// --- 2. An observer (replaces the original's printf + AllocConsole) -------------------------
class LogObserver final : public pbuf::IObserver {
public:
    bool enabled = false;
    void onBuffered(pbuf::BufferedEvent const& e) override {
        if (enabled) log::info("{} held back {} ticks (survival {} -> {}, limit {})",
                               e.push ? "PRESS" : "RELEASE", e.delay, e.frame0Ticks, e.bestTicks, e.maxDelay);
    }
};

pbuf::Buffer g_buffer(GEODE_MOD_ID);   // GEODE_MOD_ID = this mod's id: the buffer's owner name
std::shared_ptr<LogObserver> g_log = std::make_shared<LogObserver>();

constexpr char const* kStrict = "your-id.custom-buffer-example.strict";

void applySettings() {
    auto* mod = Mod::get();
    bool const strict = mod->getSettingValue<bool>("strict");
    g_log->enabled = mod->getSettingValue<bool>("log-decisions");

    // Read-modify-write keeps whatever else is configured; only these fields are ours.
    auto r = g_buffer.modify([&](pbuf::Config& c) {
        c.strategyId = strict ? kStrict : "pbuf.trajectory";
    });
    if (r.isErr()) log::error("could not update buffer config: {}", r.unwrapErr());

    if (!mod->getSettingValue<bool>("enabled")) { (void)g_buffer.applyPreset("off"); return; }
    if (auto p = g_buffer.applyPreset(mod->getSettingValue<std::string>("preset")); p.isErr())
        log::error("{}", p.unwrapErr());
}

} // namespace

$on_mod(Loaded) {
    // --- 3. Open: acquire the buffer (fails with a readable message if the API mod is missing/old/owned) ---
    // Start from the recommended config; use pbuf::Config::classic() to reproduce Predictive Buffer v4.6 exactly.
    if (auto r = g_buffer.open(pbuf::Config::recommended()); r.isErr()) {
        log::error("Predictive Buffer API unavailable: {}", r.unwrapErr());
        return;
    }

    // --- 4. Presets: the original F6 ladder plus one of our own ---
    auto list = pbuf::presets::classic();                            // 2/2, 4/4, 8/8, 16/8, OFF
    list.insert(list.end() - 1, pbuf::presets::make("tournament", 6, 4));  // before OFF
    (void)g_buffer.setPresets(std::move(list));

    (void)g_buffer.registerStrategy(std::make_shared<StrictStrategy>());
    (void)g_buffer.addObserver(g_log);
    applySettings();

    // --- 5. React to settings ---
    // v5 requires the type argument (see the Geode v5 migration guide).
    listenForSettingChanges<bool>("enabled", [](bool) { applySettings(); });
    listenForSettingChanges<std::string>("preset", [](std::string) { applySettings(); });
    listenForSettingChanges<bool>("strict", [](bool) { applySettings(); });
    listenForSettingChanges<bool>("log-decisions", [](bool) { applySettings(); });

    // --- 6. Hotkey. Replaces GetAsyncKeyState(VK_F6), which was Windows-only. ---
    // UNVERIFIED: the callback shape (Keybind const&, bool, bool, double) comes from the class
    // listing of KeybindSettingPressedEventV3; the docs do not name the two bools. I assume
    // (down, repeat). If they are swapped, F6 will fire on key-repeat only; swap the names.
    listenForKeybindSettingPresses("cycle-key",
        [](Keybind const&, bool down, bool repeat, double) {
            if (down && !repeat) {
                auto r = g_buffer.cyclePreset();
                if (r.isOk()) log::info("preset: {}", r.unwrap());
            }
            return ListenerResult::Propagate;
        }, Mod::get());
}
