// Predictive Buffer API - optional module: orb-speed learning.
//
// NEW code. Modeled on the behavior GD-Input-Buffer (AlQuerez, used with
// permission - see NOTICE.md) documents in its changelog: its own simulated
// player's orb call did nothing useful (a pink and a green orb both predicted
// a plain flap's y-speed), so it learns what each orb really does from the
// REAL game the first time the player actually uses one, and reuses that from
// then on. This file is an independent implementation of that idea - a small
// keyed table with a running average and a same-orb-different-context
// fallback - not a port of any source file (the original keeps this as
// member variables and methods on its single $modify(PlayLayer) class).
//
// This is purely bookkeeping: it does not know what an orb, a gamemode or a
// gravity flip IS. The caller (a GeodeBackend) builds the key from whatever
// distinguishes "orbs that behave the same way" in the real game (orb type,
// gamemode, player size, gravity direction, ...) and is the one that detects
// "the player just used an orb for real" and calls record() with what
// happened. That detection is NOT included here (see
// docs/GD_INPUT_BUFFER_INTEGRATION.md) - it needs real PlayLayer/GameObject
// access to write and verify.
//
// Geode-free, header-only.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <unordered_map>

namespace pbuf {

/// What a given orb (for some key) does to the player: the y-speed right
/// after using it, and whether it flips gravity.
struct OrbOutcome {
    double vy     = 0.0;
    bool   flips  = false;
};

class OrbLearning {
public:
    /// Record one real observation for `key`. Repeated observations for the
    /// same key are averaged (a key is only ever "learned" with any real
    /// accuracy once you've hit it a handful of times); the flip flag is
    /// majority-vote, matching a thing that is physically consistent per key.
    void record(std::uint32_t key, double vy, bool flipped) {
        Fact& f = table_[key];
        f.vySum += vy;
        f.n += 1;
        if (flipped) f.flips += 1;
    }

    /// Learned outcome for `key`, or nullopt if it has never been observed.
    std::optional<OrbOutcome> lookup(std::uint32_t key) const {
        auto it = table_.find(key);
        if (it == table_.end() || it->second.n == 0) return std::nullopt;
        Fact const& f = it->second;
        return OrbOutcome{f.vySum / f.n, f.flips * 2 > f.n};
    }

    /// Like lookup(), but if `key` has no observations yet, falls back to
    /// `fallbackKey` (e.g. the same orb type at a different size/gravity,
    /// which usually behaves closely enough to be a far better guess than
    /// the un-learned default).
    std::optional<OrbOutcome> lookupWithFallback(std::uint32_t key, std::uint32_t fallbackKey) const {
        if (auto r = lookup(key)) return r;
        return lookup(fallbackKey);
    }

    int observationCount(std::uint32_t key) const {
        auto it = table_.find(key);
        return it == table_.end() ? 0 : it->second.n;
    }

    bool empty() const { return table_.empty(); }
    std::size_t size() const { return table_.size(); }
    void clear() { table_.clear(); }

    /// One line per learned key: "key vySum n flips". Unknown/corrupt lines
    /// are skipped on load rather than failing the whole file, same as
    /// GD-Input-Buffer's own save-file convention for learned data.
    std::string serialize() const {
        std::string out = "PBUFORBS 1\n";
        char buf[96];
        for (auto const& [key, f] : table_) {
            std::snprintf(buf, sizeof(buf), "%u %.6f %d %d\n", key, f.vySum, f.n, f.flips);
            out += buf;
        }
        return out;
    }

    void deserialize(std::string const& text) {
        table_.clear();
        std::size_t pos = 0;
        bool header = true;
        while (pos < text.size()) {
            std::size_t end = text.find('\n', pos);
            if (end == std::string::npos) end = text.size();
            std::string const line = text.substr(pos, end - pos);
            pos = end + 1;
            if (header) {
                header = false;
                if (line.rfind("PBUFORBS", 0) == 0) continue;
            }
            char const* c = line.c_str();
            char* q = nullptr;
            unsigned long const key = std::strtoul(c, &q, 10); if (q == c) continue; c = q;
            double const vySum = std::strtod(c, &q); if (q == c) continue; c = q;
            long const n = std::strtol(c, &q, 10); if (q == c) continue; c = q;
            long const flips = std::strtol(c, &q, 10); if (q == c) continue;
            if (n <= 0 || flips < 0 || flips > n) continue;
            Fact f; f.vySum = vySum; f.n = static_cast<int>(n); f.flips = static_cast<int>(flips);
            table_[static_cast<std::uint32_t>(key)] = f;
        }
    }

private:
    struct Fact { double vySum = 0.0; int n = 0; int flips = 0; };
    std::unordered_map<std::uint32_t, Fact> table_;
};

} // namespace pbuf
