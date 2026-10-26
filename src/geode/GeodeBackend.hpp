// INTERNAL - needs the Geode SDK. NOT compiled in the environment that produced
// this repository (no SDK available there). See docs/VERIFICATION.md.
//
// DERIVED from Predictive Buffer v4.6 (`TrajectoryBufferSim`: createTrajectoryPlayer,
// syncSimPlayer, getSurvivalTicks, setFrameDelta, m_SimFireButton). Every use of a
// game member or function below appears in that original source, with the single
// exception marked "UNVERIFIED".
#pragma once

#include <Geode/Geode.hpp>

#include "Backend.hpp"

#include <pbuf/Config.hpp>

namespace pbuf::geodeimpl {

// Plain flags read by the hooks on every call. Game thread only.
inline bool g_active  = false;  // some mod owns the buffer
inline bool g_probing = false;  // a trajectory probe is running: suppress side effects

class GeodeBackend final : public detail::IBackend, private IProbe {
public:
    // ---- detail::IBackend ----
    bool query(detail::GameState& out) override;
    IProbe& beginProbe(InputContext const& ctx, detail::GameState const& state) override;
    void endProbe() override;
    void fire(bool push) override;

    // ---- lifecycle / hook support ----
    void configure(Config const& cfg);        // rules the probe needs (continuous modes, dual policy)
    void createSimPlayers();                  // idempotent; no-op without a PlayLayer
    void shutdown();                          // releases simulated players
    bool isSimPlayer(geode::prelude::PlayerObject* p) const { return p && (p == m_sim1 || p == m_sim2); }
    void markSimulatedDeath() { m_simDead = true; }
    void setFrameDelta(float dt);

    /// Reused by the collisionCheckObjects hook: avoids one heap allocation per simulated tick.
    gd::vector<geode::prelude::GameObject*>& scratch() { return m_scratch; }

private:
    // ---- IProbe ----
    Tick survival(Tick delay, bool push, Tick horizon) override;
    std::int64_t simulatedTicks() const override { return m_simTicks; }

    geode::prelude::PlayerObject* createSimPlayer();
    static void syncSimPlayer(geode::prelude::PlayerObject* sim, geode::prelude::PlayerObject* real);
    static void resetCollisionLog(geode::prelude::PlayerObject* p);
    float validDt() const { return (m_frameDt > 0.001f && m_frameDt < 2.0f) ? m_frameDt : 0.25f; }

    geode::prelude::PlayerObject* m_sim1 = nullptr;
    geode::prelude::PlayerObject* m_sim2 = nullptr;
    bool  m_simDead = false;
    float m_frameDt = 0.25f;            // 240 TPS normalized dt

    // Per-input probe state
    detail::GameState m_state;
    std::int64_t m_simTicks = 0;

    // Rules copied from Config
    ModeSet   m_continuous = {Mode::Wave, Mode::Ufo, Mode::Swing};
    DualProbe m_dualProbe  = DualProbe::LinkedOnly;

    // v4.6 heuristics for "the player has landed and is safe", kept as named constants.
    static constexpr int kSettleAirborneTicks = 15;
    static constexpr int kSettleLandedTicks   = 10;

    gd::vector<geode::prelude::GameObject*> m_scratch;
};

} // namespace pbuf::geodeimpl
