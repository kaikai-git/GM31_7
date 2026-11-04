#pragma once
#include "keyframemotion.h"

namespace Motion {
enum class Attack { None, Horizontal, Vertical };
struct CharacterCommands {
    bool jump = false;
    bool horizontalSlash = false;
    bool verticalSlash = false;
};

// Key presses are edge-triggered by the caller. Physics is independent of the render rate.
class CharacterMotion {
public:
    static constexpr float WalkSpeed = 90;
    static constexpr float Gravity = 600;
    static constexpr float JumpVelocity = 240;

    CharacterMotion();
    bool LoadClips(const std::filesystem::path& directory, std::string& error);
    void Reset(float groundHeight);
    void Update(float seconds, float groundHeight, float horizontalSpeed, const CharacterCommands& commands);
    const Pose& GetPose() const { return m_pose; }
    float Height() const { return m_height; }
    bool Grounded() const { return m_grounded; }
    bool Jumping() const { return m_jumpActive; }
    Attack CurrentAttack() const { return m_attack; }
    bool CanReload() const { return !m_jumpActive && m_attack == Attack::None; }
    const char* LocomotionName() const;
    const Clip& GetClip(Preset preset) const { return m_clips[static_cast<std::size_t>(preset)]; }

private:
    void BuildPose();
    std::array<Clip, 4> m_clips;
    Pose m_pose;
    float m_height = 0;
    float m_verticalVelocity = 0;
    float m_walkTime = 0;
    float m_walkWeight = 0;
    float m_jumpTime = 0;
    float m_attackTime = 0;
    bool m_grounded = true;
    bool m_jumpActive = false;
    bool m_launched = false;
    Attack m_attack = Attack::None;
};
}
