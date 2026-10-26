// INTERNAL - needs the Geode SDK. NOT compiled where it was written. See GeodeBackend.hpp.
#include "GeodeBackend.hpp"

using namespace geode::prelude;

namespace pbuf::geodeimpl {

namespace {

Mode modeOf(PlayerObject* p) {
    if (p->m_isShip) return Mode::Ship;
    if (p->m_isDart) return Mode::Wave;
    if (p->m_isBird) return Mode::Ufo;
    if (p->m_isSwing) return Mode::Swing;
    if (p->m_isRobot) return Mode::Robot;
    return Mode::Other;   // cube, ball, spider: never distinguished by the original
}

// The ONE place that decides "is this a platformer level".
// UNVERIFIED: `GJGameLevel::isPlatformer()` was not confirmed against the 2.2081
// bindings (the original project never used it). If it does not compile, either fix
// this function or configure with -DPBUF_NO_PLATFORMER_DETECT=ON (then
// ModeRules::bypassPlatformer has no effect).
bool isPlatformerLevel(PlayLayer* pl) {
#ifdef PBUF_NO_PLATFORMER_DETECT
    (void)pl;
    return false;
#else
    return pl->m_level && pl->m_level->isPlatformer();
#endif
}

// Keeps g_probing balanced on every exit path.
struct ProbingFlag {
    ProbingFlag() { g_probing = true; }
    ~ProbingFlag() { g_probing = false; }
};

} // namespace

// ---------------------------------------------------------------------------
// IBackend
// ---------------------------------------------------------------------------
bool GeodeBackend::query(detail::GameState& out) {
    PlayLayer* pl = PlayLayer::get();
    if (!pl) return false;

    out.inLevel = true;
    PlayerObject* p1 = pl->m_player1;
    out.p1Dead = !p1 || p1->m_isDead;
    if (p1) { out.p1 = modeOf(p1); out.p1Mini = p1->m_vehicleSize < 1.0f; }

    PlayerObject* p2 = pl->m_player2;
    out.dual = pl->m_gameState.m_isDualMode && p2 != nullptr;
    if (out.dual) { out.p2 = modeOf(p2); out.p2Mini = p2->m_vehicleSize < 1.0f; }

    out.twoPlayerMode = pl->m_level && pl->m_level->m_twoPlayerMode;
    out.platformer = isPlatformerLevel(pl);
    return true;
}

IProbe& GeodeBackend::beginProbe(InputContext const&, detail::GameState const& state) {
    createSimPlayers();   // covers an owner that acquired the buffer after the level started
    m_state = state;
    m_simTicks = 0;
    return *this;
}

void GeodeBackend::endProbe() {}

void GeodeBackend::fire(bool push) {
    // Same as v4.6 m_SimFireButton: player 1, and player 2 as well in one-player dual mode.
    PlayLayer* pl = PlayLayer::get();
    if (!pl) return;
    pl->handleButton(push, 1, true);
    if (pl->m_gameState.m_isDualMode && (!pl->m_level || !pl->m_level->m_twoPlayerMode)) {
        pl->handleButton(push, 1, false);
    }
}

// ---------------------------------------------------------------------------
// lifecycle
// ---------------------------------------------------------------------------
void GeodeBackend::configure(Config const& cfg) {
    m_continuous = cfg.rules.continuous;
    m_dualProbe = cfg.behavior.dualProbe;
}

PlayerObject* GeodeBackend::createSimPlayer() {
    PlayLayer* pl = PlayLayer::get();
    if (!pl) return nullptr;
    PlayerObject* player = PlayerObject::create(1, 1, pl, pl, true);
    if (!player) return nullptr;
    player->retain();
    player->setPosition({0, 105});
    player->setVisible(false);
    pl->m_objectLayer->addChild(player);
    return player;
}

void GeodeBackend::createSimPlayers() {
    if (!g_active || m_sim1 || !PlayLayer::get()) return;
    m_sim1 = createSimPlayer();
    m_sim2 = createSimPlayer();
}

void GeodeBackend::shutdown() {
    // v4.6 only release()d, so a second init() without quit() leaked both players.
    for (PlayerObject** slot : {&m_sim1, &m_sim2}) {
        if (PlayerObject* p = *slot) {
            // If the parent layer is already gone cocos2d has nulled the parent pointer.
            if (p->getParent()) p->removeFromParent();
            p->release();
            *slot = nullptr;
        }
    }
    m_simDead = false;
    g_probing = false;
}

void GeodeBackend::setFrameDelta(float dt) {
    PlayLayer* pl = PlayLayer::get();
    if (pl && pl->m_gameState.m_timeWarp > 0.f) m_frameDt = dt / pl->m_gameState.m_timeWarp;
    else m_frameDt = dt;
}

// ---------------------------------------------------------------------------
// simulation helpers (DERIVED from v4.6)
// ---------------------------------------------------------------------------
void GeodeBackend::resetCollisionLog(PlayerObject* p) {
    p->m_collisionLogTop->removeAllObjects();
    p->m_collisionLogBottom->removeAllObjects();
    p->m_collisionLogLeft->removeAllObjects();
    p->m_collisionLogRight->removeAllObjects();
}

// Matches Show Trajectory's attribute + hitbox synchronisation (as in v4.6).
void GeodeBackend::syncSimPlayer(PlayerObject* sim, PlayerObject* real) {
    sim->setVisible(false);
    sim->copyAttributes(real);
    sim->setScale(real->getScale());
    sim->setContentSize(real->getContentSize());
    sim->m_width = real->m_width;
    sim->m_height = real->m_height;
    sim->m_gravityMod = real->m_gravityMod;
    sim->m_isOnGround = real->m_isOnGround;
    sim->m_isUpsideDown = real->m_isUpsideDown;
    sim->m_vehicleSize = real->m_vehicleSize;
    sim->m_playerSpeed = real->m_playerSpeed;
}

Tick GeodeBackend::survival(Tick delayFrames, bool actionPush, Tick maxTicks) {
    PlayLayer* pl = PlayLayer::get();
    PlayerObject* real1 = pl ? pl->m_player1 : nullptr;
    PlayerObject* real2 = (pl && m_state.dual) ? pl->m_player2 : nullptr;
    if (!pl || !m_sim1 || !real1) return maxTicks;

    ProbingFlag probing;
    m_simDead = false;

    syncSimPlayer(m_sim1, real1);
    bool const two = m_state.dual && m_sim2 && real2;
    if (two) syncSimPlayer(m_sim2, real2);

    // v4.6 pressed P2 as well whenever dual; in two-player mode that is wrong because P2 has its own button.
    bool const pressP2 = two && (m_dualProbe == DualProbe::AlwaysMirror || m_state.linked());

    auto setButton = [&](bool push) {
        if (push) { m_sim1->pushButton(PlayerButton::Jump); if (pressP2) m_sim2->pushButton(PlayerButton::Jump); }
        else      { m_sim1->releaseButton(PlayerButton::Jump); if (pressP2) m_sim2->releaseButton(PlayerButton::Jump); }
    };

    // Before the delay elapses, hold the button in the state opposite to the pending action.
    if (delayFrames > 0) setButton(!actionPush);

    float const dt = validDt();
    Tick survived = 0;
    bool const continuous = m_continuous.has(m_state.p1) || (m_state.dual && m_continuous.has(m_state.p2));
    int airborneTicks = 0;
    int safeLandedTicks = 0;

    for (Tick i = 0; i < delayFrames + maxTicks; i++) {
        if (i == delayFrames) setButton(actionPush);
        ++m_simTicks;

        m_sim1->update(dt);
        m_sim1->updateRotation(dt);
        if (two) { m_sim2->update(dt); m_sim2->updateRotation(dt); }
        if (m_simDead) break;

        resetCollisionLog(m_sim1);
        pl->checkCollisions(m_sim1, dt, false);
        if (m_simDead) break;

        if (two) {
            resetCollisionLog(m_sim2);
            pl->checkCollisions(m_sim2, dt, false);
            if (m_simDead) break;
        }

        if (i >= delayFrames) {
            survived++;
            if (!continuous) {
                bool const grounded = m_sim1->m_isOnGround && (!two || m_sim2->m_isOnGround);
                if (!grounded) {
                    airborneTicks++;
                    safeLandedTicks = 0;
                } else if (airborneTicks >= kSettleAirborneTicks) {
                    safeLandedTicks++;
                    if (safeLandedTicks >= kSettleLandedTicks) { survived = maxTicks; break; }
                }
            }
        }
    }
    return survived;
}

} // namespace pbuf::geodeimpl
