// INTERNAL - needs the Geode SDK. NOT compiled where it was written.
//
// DERIVED from the `$modify` classes of Predictive Buffer v4.6. Signatures and the
// set of hooked functions are the original's. Differences:
//   * state lives in Runtime instead of a file-scope `s_sim`
//   * every hook first tests two plain bools, so with no owning mod the cost is a
//     couple of loads and a branch
//   * `PlayerObject::update` no longer polls the keyboard (F6 was Windows-only
//     GetAsyncKeyState); hotkeys are the consumer's job (see examples/)
//   * no AllocConsole() at load
//
// Priority: the original passes -1000000 to setHookPriority. Per Geode's hook
// priority tutorial, the call chain starts at the LOWEST priority, so this makes
// the suppression hooks the OUTERMOST layer: other mods' hooks on the same
// functions never see calls made on simulated players.
// UNVERIFIED: that `setHookPriority` (as opposed to setHookPriorityPre/Post, which
// the current tutorial shows) is still accepted by Geode 5.11.2. The original used
// it; if it fails to compile, replace it in PBUF_OUTERMOST below.
#include <Geode/Geode.hpp>
#include <Geode/modify/EffectGameObject.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/GameObject.hpp>
#include <Geode/modify/HardStreak.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>

#include "Runtime.hpp"

using namespace geode::prelude;
using pbuf::geodeimpl::g_active;
using pbuf::geodeimpl::g_probing;
using pbuf::geodeimpl::Runtime;

namespace {
constexpr int kOutermost = -1000000;   // value used by the original
}
#define PBUF_OUTERMOST(self, fn) (void)(self).setHookPriority(fn, kOutermost)

class $modify(PbufPlayLayer, PlayLayer) {
    static void onModify(auto& self) {
        PBUF_OUTERMOST(self, "PlayLayer::destroyPlayer");
        PBUF_OUTERMOST(self, "PlayLayer::playEndAnimationToPos");
        PBUF_OUTERMOST(self, "PlayLayer::flipGravity");
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        Runtime::get().onPlayLayerInit();
        return true;
    }

    void destroyPlayer(PlayerObject* player, GameObject* gameObject) {
        auto& rt = Runtime::get();
        if (g_probing && rt.backend().isSimPlayer(player)) {   // a simulated player "died": just record it
            rt.backend().markSimulatedDeath();
            return;
        }
        if (g_active) rt.onRealPlayerDied();
        PlayLayer::destroyPlayer(player, gameObject);
    }

    void playEndAnimationToPos(cocos2d::CCPoint p0) {
        if (g_probing) return;
        PlayLayer::playEndAnimationToPos(p0);
    }

    void flipGravity(PlayerObject* p0, bool p1, bool p2) {
        if (g_probing) return;
        PlayLayer::flipGravity(p0, p1, p2);
    }

    void onQuit() {
        PlayLayer::onQuit();
        Runtime::get().onPlayLayerQuit();
    }
};

class $modify(PbufEditorLayer, LevelEditorLayer) {
    bool init(GJGameLevel* level, bool unk) {
        bool res = LevelEditorLayer::init(level, unk);
        Runtime::get().onEditorInit();
        return res;
    }
};

class $modify(PbufHardStreak, HardStreak) {
    static void onModify(auto& self) { PBUF_OUTERMOST(self, "HardStreak::addPoint"); }
    void addPoint(cocos2d::CCPoint p0) {
        if (g_probing) return;
        HardStreak::addPoint(p0);
    }
};

class $modify(PbufEffectGameObject, EffectGameObject) {
    static void onModify(auto& self) { PBUF_OUTERMOST(self, "EffectGameObject::triggerObject"); }
    void triggerObject(GJBaseGameLayer* p0, int p1, gd::vector<int> const* p2) {
        if (g_probing) return;
        EffectGameObject::triggerObject(p0, p1, p2);
    }
};

class $modify(PbufGameObject, GameObject) {
    static void onModify(auto& self) { PBUF_OUTERMOST(self, "GameObject::playShineEffect"); }
    void playShineEffect() {
        if (g_probing) return;
        GameObject::playShineEffect();
    }
};

class $modify(PbufPlayerObject, PlayerObject) {
    static void onModify(auto& self) {
        PBUF_OUTERMOST(self, "PlayerObject::playSpiderDashEffect");
        PBUF_OUTERMOST(self, "PlayerObject::incrementJumps");
        PBUF_OUTERMOST(self, "PlayerObject::ringJump");
    }

    void playSpiderDashEffect(cocos2d::CCPoint from, cocos2d::CCPoint to) {
        if (g_probing) return;
        PlayerObject::playSpiderDashEffect(from, to);
    }

    void incrementJumps() {
        if (g_probing) return;
        PlayerObject::incrementJumps();
    }

    void update(float dt) {
        bool const live = g_active && !g_probing;
        if (live) Runtime::get().onPlayerUpdate(this);
        PlayerObject::update(dt);
        if (live) Runtime::get().backend().setFrameDelta(dt);
    }

    void ringJump(RingObject* p0, bool p1) {
        if (g_probing) return;
        PlayerObject::ringJump(p0, p1);
    }
};

class $modify(PbufBaseGameLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        PBUF_OUTERMOST(self, "GJBaseGameLayer::canBeActivatedByPlayer");
        PBUF_OUTERMOST(self, "GJBaseGameLayer::handleButton");
        PBUF_OUTERMOST(self, "GJBaseGameLayer::collisionCheckObjects");
        PBUF_OUTERMOST(self, "GJBaseGameLayer::playerTouchedRing");
    }

    bool canBeActivatedByPlayer(PlayerObject* p0, EffectGameObject* p1) {
        if (g_probing) return false;
        return GJBaseGameLayer::canBeActivatedByPlayer(p0, p1);
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        // Engine::onInput ignores the calls the engine makes itself (it is "firing").
        if (g_active && !g_probing && button == 1 && PlayLayer::get()) {
            if (!Runtime::get().engine().onInput(down, isPlayer1)) return;   // swallowed: delivered later
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void collisionCheckObjects(PlayerObject* player, gd::vector<GameObject*>* vec, int objectsCount, float dt) {
        if (g_probing) {
            // Only static geometry matters for a trajectory; pads, orbs, portals, triggers are skipped
            // (this is also a known accuracy limit of v4.6: see docs/ORIGINAL_ANALYSIS.md).
            // The scratch vector is reused so no allocation happens per simulated tick.
            auto& keep = Runtime::get().backend().scratch();
            keep.clear();
            for (int i = 0; i < objectsCount; i++) {
                GameObject* obj = vec->at(i);
                if (!obj) continue;
                auto t = obj->m_objectType;
                if (t == GameObjectType::Solid || t == GameObjectType::Hazard ||
                    t == GameObjectType::AnimatedHazard || t == GameObjectType::CollisionObject ||
                    t == GameObjectType::Slope) {
                    keep.push_back(obj);
                }
            }
            GJBaseGameLayer::collisionCheckObjects(player, &keep, static_cast<int>(keep.size()), dt);
            return;
        }
        GJBaseGameLayer::collisionCheckObjects(player, vec, objectsCount, dt);
    }

    void playerTouchedRing(PlayerObject* player, RingObject* ring) {
        if (g_probing) return;
        GJBaseGameLayer::playerTouchedRing(player, ring);
    }
};
