#pragma once

#include <cstdint>
#include "gameobject.h"
#include "../system/IScene.h"

class player : public gameobject {
public:
    inline static float VALUE_MOVE_MODEL = 2.0f;
    inline static float VALUE_ROTATE_MODEL = PI * 0.002f;
    inline static float RATE_ROTATE_MODEL = 0.4f;
    inline static float RATE_MOVE_MODEL = 0.2f;

    // The humanoid stands on local Y=0.
    static constexpr float MODEL_HEIGHT = 83.0f;
    static constexpr float COLLISION_RADIUS = 20.0f;
    static constexpr int MAX_WALL_RESOLVE = 4;
    static constexpr float WALL_EPSILON = 0.0001f;

    explicit player(SceneBase* scene) : gameobject(scene) {}

    void update(uint64_t delta) override;
    void draw(uint64_t delta) override;
    void init() override;
    void dispose() override;

private:
    float m_walkPhase = 0.0f;
    float m_walkAmount = 0.0f;

    Vector3 m_move{};
    Vector3 m_destrot{};
    Vector3 m_crosspoint{};
    Vector3 m_penetration{};
    bool m_ishitwall = false;
};
