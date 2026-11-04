#pragma once

#include <cstdint>
#include "gameobject.h"
#include "../system/IScene.h"
#include "../utility/charactermotion.h"

class wall;

class player : public gameobject {
public:
    // The humanoid stands on local Y=0.
    static constexpr float MODEL_HEIGHT = 83.0f;
    static constexpr float COLLISION_RADIUS = 20.0f;
    static constexpr int MAX_WALL_RESOLVE = 6;
    static constexpr float WALL_EPSILON = 0.0001f;

    explicit player(SceneBase* scene) : gameobject(scene) {}

    void update(uint64_t delta) override;
    void draw(uint64_t delta) override;
    void init() override;
    void dispose() override;
    bool overlapsWall(const wall& object) const;
    bool isHitWall() const { return m_ishitwall; }
    float getSpeed() const { return m_speed; }
    const Motion::CharacterMotion& animation() const { return m_motion; }
    const std::string& motionStatus() const { return m_motionStatus; }
    bool motionsLoaded() const { return m_motionsLoaded; }

private:
    void reloadMotions();
    Motion::CharacterMotion m_motion;
    std::string m_motionStatus;
    Vector3 m_velocity{};
    float m_targetYaw = PI;
    float m_speed = 0;
    bool m_motionsLoaded = false;
    bool m_ishitwall = false;
};
