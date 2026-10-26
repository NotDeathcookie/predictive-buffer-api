// Type-checks the consumer-facing API (Buffer, api::*) as a downstream mod would use it.
#include <pbuf/pbuf.hpp>
struct Obs final : pbuf::IObserver { void onBuffered(pbuf::BufferedEvent const&) override {} };
int main() {
    static pbuf::Buffer buffer("some.mod");
    if (auto r = buffer.open(pbuf::Config::recommended()); r.isErr()) return 0;   // stub: API absent -> Err
    (void)buffer.modify([](pbuf::Config& c) { c.limits.normal = 6; });
    (void)buffer.cyclePreset(); (void)buffer.applyPreset("classic-2"); (void)buffer.addObserver(std::make_shared<Obs>());
    (void)buffer.setPresets(pbuf::presets::extended()); (void)buffer.stats();
}
