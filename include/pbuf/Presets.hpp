// Predictive Buffer API - presets.
//
// A preset is a named (enabled, delay limits) pair. Applying one changes only
// those fields of a Config, so presets compose with any other settings.
//
// Provenance: the five "classic" values are the F6 stages of Predictive Buffer
// v4.6 (2/2, 4/4, 8/8, 16/8, OFF). The "extended" values come from the binary
// patcher `patch_predictive_buffer.py` shipped in the same archive; see
// docs/ORIGINAL_ANALYSIS.md for what could and could not be verified about it.
#pragma once

#include "Config.hpp"

#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace pbuf {

struct Preset {
    std::string id;     ///< stable machine id, e.g. "classic-3"
    std::string label;  ///< human text, e.g. "8 ticks all / 8 ticks wave"
    bool        enabled = true;
    DelayLimits limits;

    void applyTo(Config& c) const {
        c.enabled = enabled;
        if (enabled) c.limits = limits;
    }
};

namespace presets {

inline Preset make(std::string id, Tick all, Tick wave) {
    Preset p;
    p.id = std::move(id);
    char buf[96];
    // 4 ticks = 1 frame @ 60 fps at the stock 240 TPS.
    std::snprintf(buf, sizeof buf, "%d ticks all / %d ticks wave (%.2f frames @60fps)", all, wave, all / 4.0);
    p.label = buf;
    p.limits = {all, wave};
    return p;
}

inline Preset off() {
    Preset p;
    p.id = "off";
    p.label = "OFF (100% raw input)";
    p.enabled = false;
    p.limits = {0, 0};
    return p;
}

/// Predictive Buffer v4.6 F6 cycle, in its original order.
inline std::vector<Preset> classic() {
    return {make("classic-1", 2, 2), make("classic-2", 4, 4), make("classic-3", 8, 8),
            make("classic-4", 16, 8), off()};
}

/// Classic plus the stages added by the binary patcher (24/32/48/54/60 ticks).
inline std::vector<Preset> extended() {
    return {make("classic-1", 2, 2),   make("classic-2", 4, 4),   make("classic-3", 8, 8),
            make("classic-4", 16, 8),  make("extended-1", 24, 24), make("extended-2", 32, 32),
            make("extended-3", 48, 48), make("extended-4", 54, 54), make("extended-5", 60, 60),
            off()};
}

} // namespace presets

/// Ordered list of presets with a cursor, i.e. the F6 cycle as a reusable object.
/// Cycling moves to the NEXT preset first (v4.6 started on stage 1 and its first
/// F6 press went to stage 2).
class PresetBook {
public:
    PresetBook() : list_(presets::classic()) {}
    explicit PresetBook(std::vector<Preset> list) : list_(std::move(list)) {}

    std::vector<Preset> const& all() const { return list_; }
    bool empty() const { return list_.empty(); }

    Preset const* current() const { return list_.empty() ? nullptr : &list_[index_ % list_.size()]; }

    Preset const* next() {
        if (list_.empty()) return nullptr;
        index_ = (index_ + 1) % list_.size();
        return &list_[index_];
    }

    Preset const* select(std::string_view id) {
        for (std::size_t i = 0; i < list_.size(); ++i)
            if (list_[i].id == id) { index_ = i; return &list_[i]; }
        return nullptr;
    }

    /// Adds or replaces (by id). Does not move the cursor.
    void add(Preset p) {
        for (auto& e : list_) if (e.id == p.id) { e = std::move(p); return; }
        list_.push_back(std::move(p));
    }

    std::string ids() const {
        std::string s;
        for (auto const& p : list_) { if (!s.empty()) s += ", "; s += p.id; }
        return s;
    }

private:
    std::vector<Preset> list_;
    std::size_t index_ = 0;
};

} // namespace pbuf
