#pragma once
#include "Result.hpp"
#include <cstdio>
#include <format>
#include <string>
#include <vector>
namespace gd { template <class T> using vector = std::vector<T>; }
namespace cocos2d {
struct CCPoint { float x = 0, y = 0; };
struct CCSize {};
struct CCArray { void removeAllObjects() {} };
struct CCNode {
    virtual ~CCNode() = default;
    void setPosition(CCPoint) {} void setVisible(bool) {} void setScale(float) {} float getScale() { return 1; }
    CCSize getContentSize() { return {}; } void setContentSize(CCSize) {}
    CCNode* getParent() { return nullptr; } void removeFromParent() {} void addChild(CCNode*) {}
    void retain() {} void release() {}
};
}
enum class PlayerButton { Jump };
enum class GameObjectType { Solid, Hazard, AnimatedHazard, CollisionObject, Slope };
struct GameObject : cocos2d::CCNode { GameObjectType m_objectType{}; };
struct PlayLayer;
struct PlayerObject : cocos2d::CCNode {
    bool m_isShip{}, m_isDart{}, m_isBird{}, m_isSwing{}, m_isRobot{}, m_isDead{}, m_isOnGround{}, m_isUpsideDown{};
    float m_vehicleSize{1}, m_width{}, m_height{}, m_gravityMod{}, m_playerSpeed{};
    cocos2d::CCArray *m_collisionLogTop{}, *m_collisionLogBottom{}, *m_collisionLogLeft{}, *m_collisionLogRight{};
    static PlayerObject* create(int, int, PlayLayer*, PlayLayer*, bool) { return new PlayerObject; }
    void copyAttributes(PlayerObject*) {} void pushButton(PlayerButton) {} void releaseButton(PlayerButton) {}
    void update(float) {} void updateRotation(float) {}
};
struct GJGameLevel { bool m_twoPlayerMode{}; bool isPlatformer() { return false; } };
struct GameStateStub { bool m_isDualMode{}; float m_timeWarp{1}; };
struct PlayLayer : cocos2d::CCNode {
    static PlayLayer* get() { return nullptr; }
    PlayerObject *m_player1{}, *m_player2{}; GJGameLevel* m_level{}; GameStateStub m_gameState; cocos2d::CCNode* m_objectLayer{};
    void handleButton(bool, int, bool) {} void checkCollisions(PlayerObject*, float, bool) {}
};
namespace geode {
struct Mod { bool isLoaded() const { return true; } };
struct Loader { static Loader* get() { static Loader l; return &l; } Mod* getLoadedMod(std::string const&) { static Mod m; return &m; } };
namespace log { template <class... A> void info(std::format_string<A...> f, A&&... a) { std::puts(std::format(f, std::forward<A>(a)...).c_str()); } }
namespace prelude { using namespace geode; using namespace cocos2d; using ::PlayLayer; using ::PlayerObject; using ::GameObject; using ::GJGameLevel; using ::PlayerButton; using ::GameObjectType; }
}
