// TEST-ONLY REFERENCE. A line-by-line transliteration of the decision logic of
// `TrajectoryBufferSim` from Predictive Buffer v4.6 (src/main.cpp in the supplied
// archive; developer field "lilWC"; no license found in the archive).
//
// Only the game access is replaced:
//   PlayLayer / PlayerObject queries -> pbt::World::gs
//   getSurvivalTicks()               -> pbt::mockSurvival()
//   m_SimFireButton() side effect    -> a logged Fire event
// F6 polling and printf() are omitted. Everything else keeps the original names
// and control flow so a reviewer can diff it against the original by eye.
// This file is not shipped in any build target other than the tests.
#pragma once

#include "mock.hpp"

namespace pbt {

constexpr int SPAM_INTERVAL_TICKS = 8;
constexpr int SPAM_STREAK_REQUIRED = 3;

class RefV46 {
public:
    World& w;
    bool m_enabled = true;
    int  m_maxTicksNormal = 2;
    int  m_maxTicksWave = 2;

    bool m_physicalHolding = false;
    bool m_gameHolding = false;
    int  m_waitTicks = -1;
    bool m_bufferedPush = true;
    bool m_queuedOpposite = false;
    int  m_ticksSinceLastInput = 999;
    int  m_rapidInputStreak = 0;

    explicit RefV46(World& world) : w(world) {}

    void init() {
        m_waitTicks = -1; m_queuedOpposite = false; m_physicalHolding = false; m_gameHolding = false;
        m_ticksSinceLastInput = 999; m_rapidInputStreak = 0;
    }

    // cyclePreset(): clears the wait state
    void applyPreset(bool enabled, int normal, int wave) {
        m_waitTicks = -1; m_queuedOpposite = false;
        m_enabled = enabled; m_maxTicksNormal = normal; m_maxTicksWave = wave;
    }

    // checkSimulationDeath(): real-player branch
    void realPlayerDied() {
        m_waitTicks = -1; m_queuedOpposite = false; m_gameHolding = false; m_rapidInputStreak = 0;
    }

    void m_SimFireButton(bool push) {
        m_gameHolding = push;
        w.log.push_back({Ev::Fire, push ? 1 : 0, 0});
    }

    int getSurvivalTicks(int delayFrames, bool actionPush, int maxTicks) {
        return mockSurvival(w.probeSeed, delayFrames, actionPush, maxTicks);
    }

    bool interceptInput(bool push, bool isPlayer1) {
        GameState const& g = w.gs;
        if (!g.inLevel) return true;                                   // if (!pl) return true;

        bool isDual = g.dual;
        bool isTwoPlayerControl = g.twoPlayerMode;

        if (!isPlayer1) {
            if (isDual && !isTwoPlayerControl && (m_waitTicks >= 0 || m_waitTicks == -2)) return false;
            return true;
        }

        m_physicalHolding = push;

        if (!m_enabled) { m_gameHolding = push; return true; }

        bool p1Dead = g.p1Dead;
        if (p1Dead) {
            m_waitTicks = -1; m_queuedOpposite = false; m_gameHolding = push; return true;
        }

        if (g.p1 == Mode::Ship || (isDual && g.p2 == Mode::Ship)) {
            m_waitTicks = -1; m_queuedOpposite = false; m_gameHolding = push; return true;
        }

        if (m_waitTicks >= 0) {
            if (push != m_bufferedPush) m_queuedOpposite = true; else m_queuedOpposite = false;
            m_ticksSinceLastInput = 0;
            return false;
        }

        if (m_waitTicks == -2) {
            m_waitTicks = -1;
            m_queuedOpposite = false;
            if (m_gameHolding != !m_bufferedPush) m_SimFireButton(!m_bufferedPush);
        }

        bool isWave = g.p1 == Mode::Wave || (isDual && g.p2 == Mode::Wave);
        bool isMiniWave = (g.p1 == Mode::Wave && g.p1Mini) || (isDual && g.p2 == Mode::Wave && g.p2Mini);

        if (m_ticksSinceLastInput < SPAM_INTERVAL_TICKS) m_rapidInputStreak++; else m_rapidInputStreak = 0;
        m_ticksSinceLastInput = 0;

        if (isWave && m_rapidInputStreak >= SPAM_STREAK_REQUIRED) { m_gameHolding = push; return true; }

        bool releaseAffectsPhysics = g.p1 == Mode::Wave || g.p1 == Mode::Robot ||
                                     (isDual && (g.p2 == Mode::Wave || g.p2 == Mode::Robot));
        if (!push && !releaseAffectsPhysics) { m_gameHolding = false; return true; }

        m_queuedOpposite = false;

        int targetHorizon = isMiniWave ? 50 : (isWave ? 65 : 160);
        int maxDelayFrames = isWave ? m_maxTicksWave : m_maxTicksNormal;

        int ticksFrame0 = getSurvivalTicks(0, push, targetHorizon);
        if (ticksFrame0 >= targetHorizon) { m_gameHolding = push; return true; }

        int bestDelay = 0;
        int bestTicks = ticksFrame0;

        for (int delay = 1; delay <= maxDelayFrames; delay++) {
            int ticks = getSurvivalTicks(delay, push, targetHorizon);

            if (isWave) {
                int minClearedGain = (delay * 2) + 10;
                if (ticks >= targetHorizon && (ticks - ticksFrame0) >= (delay * 2) + 6) {
                    bestDelay = delay; bestTicks = ticks; break;
                }
                if (ticks >= ticksFrame0 + minClearedGain && ticks > bestTicks + 2) {
                    bestDelay = delay; bestTicks = ticks;
                    if (ticks >= targetHorizon) break;
                }
            } else {
                if (ticks >= targetHorizon) { bestDelay = delay; bestTicks = ticks; break; }
                if (ticks > bestTicks + 12) { bestDelay = delay; bestTicks = ticks; }
            }
        }

        if (bestDelay > 0) {
            m_bufferedPush = push;
            m_waitTicks = bestDelay;
            return false;
        }

        m_gameHolding = push;
        return true;
    }

    // onPlayerUpdate() minus the F6 poll
    void onPlayerUpdate() {
        GameState const& g = w.gs;
        if (!g.inLevel) return;

        if (!m_enabled) return;

        if (m_ticksSinceLastInput < 999) m_ticksSinceLastInput++;

        if (g.p1 == Mode::Ship || (g.dual && g.p2 == Mode::Ship)) {
            m_waitTicks = -1;
            m_queuedOpposite = false;
            if (m_gameHolding != m_physicalHolding) m_SimFireButton(m_physicalHolding);
            return;
        }

        if (m_waitTicks == 0) {
            m_SimFireButton(m_bufferedPush);
            if (m_queuedOpposite) { m_waitTicks = -2; m_queuedOpposite = false; }
            else { m_waitTicks = -1; }
        } else if (m_waitTicks > 0) {
            m_waitTicks--;
        } else if (m_waitTicks == -2) {
            m_SimFireButton(!m_bufferedPush);
            m_queuedOpposite = false;
            m_waitTicks = -1;
        } else if (m_waitTicks == -1) {
            if (m_gameHolding != m_physicalHolding) m_SimFireButton(m_physicalHolding);
        }
    }
};

} // namespace pbt
