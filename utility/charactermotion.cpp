#include "charactermotion.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace Motion {
namespace {
Float3 BlendRotation(const Float3& a, const Float3& b, float rate) {
    return {a.x + std::remainder(b.x - a.x, 360.0f) * rate,
        a.y + std::remainder(b.y - a.y, 360.0f) * rate,
        a.z + std::remainder(b.z - a.z, 360.0f) * rate};
}
}

CharacterMotion::CharacterMotion() {
    for (std::size_t i = 0; i < m_clips.size(); ++i) m_clips[i] = MakePreset(static_cast<Preset>(i));
}

bool CharacterMotion::LoadClips(const std::filesystem::path& directory, std::string& error) {
    const char* files[] = {"walk.gmotion", "jump.gmotion", "horizontal_slash.gmotion", "vertical_slash.gmotion"};
    auto loaded = m_clips;
    for (std::size_t i = 0; i < loaded.size(); ++i) {
        if (!Load(directory / files[i], loaded[i], error)) {
            error = std::string(files[i]) + ": " + error;
            return false;
        }
    }
    m_clips = std::move(loaded);
    m_walkTime = std::fmod(m_walkTime, m_clips[0].duration);
    BuildPose();
    return true;
}

void CharacterMotion::Reset(float groundHeight) {
    m_height = groundHeight;
    m_verticalVelocity = m_walkTime = m_walkWeight = m_jumpTime = m_attackTime = 0;
    m_grounded = true;
    m_jumpActive = m_launched = false;
    m_attack = Attack::None;
    m_pose = {};
}

void CharacterMotion::Update(float seconds, float groundHeight, float horizontalSpeed, const CharacterCommands& commands) {
    if (!std::isfinite(seconds) || seconds <= 0 || !std::isfinite(groundHeight) || !std::isfinite(horizontalSpeed)) return;
    seconds = std::min(seconds, 0.1f);
    if (m_grounded) m_height = groundHeight;
    if (commands.jump && m_grounded && !m_jumpActive) {
        m_jumpActive = true;
        m_launched = false;
        m_jumpTime = 0;
    }
    if (m_attack == Attack::None && (commands.horizontalSlash || commands.verticalSlash)) {
        m_attack = commands.verticalSlash ? Attack::Vertical : Attack::Horizontal;
        m_attackTime = 0;
    }
    const float speedRate = std::clamp(horizontalSpeed / WalkSpeed, 0.0f, 1.0f);
    m_walkWeight += (speedRate - m_walkWeight) * (1 - std::exp(-12 * seconds));
    m_walkTime = std::fmod(m_walkTime + seconds * speedRate, m_clips[0].duration);

    float airStep = seconds;
    if (m_jumpActive) {
        const float takeoff = m_clips[1].duration * (0.25f / 1.4f);
        const float previousTime = m_jumpTime;
        m_jumpTime = std::min(m_jumpTime + seconds, m_clips[1].duration);
        if (!m_launched && m_jumpTime >= takeoff) {
            m_launched = true;
            m_grounded = false;
            m_verticalVelocity = JumpVelocity;
            airStep = std::clamp(previousTime + seconds - takeoff, 0.0f, seconds);
        }
    }
    if (!m_grounded) {
        m_height += m_verticalVelocity * airStep - 0.5f * Gravity * airStep * airStep;
        m_verticalVelocity -= Gravity * airStep;
        if (m_height <= groundHeight && m_verticalVelocity <= 0) {
            m_height = groundHeight;
            m_verticalVelocity = 0;
            m_grounded = true;
        }
    }
    if (m_jumpActive && m_jumpTime >= m_clips[1].duration && m_grounded) m_jumpActive = false;
    if (m_attack != Attack::None) {
        m_attackTime += seconds;
        const auto index = m_attack == Attack::Horizontal ? 2u : 3u;
        if (m_attackTime >= m_clips[index].duration) m_attack = Attack::None;
    }
    BuildPose();
}

void CharacterMotion::BuildPose() {
    m_pose = {};
    if (m_jumpActive) {
        m_pose = Sample(m_clips[1], m_jumpTime);
        // Positive jump root motion is represented by the physical body height, once.
        m_pose.rootPosition.y = m_grounded ? std::min(m_pose.rootPosition.y, 0.0f) : 0;
    } else if (m_walkWeight > 0.001f) {
        const Pose walk = Sample(m_clips[0], m_walkTime);
        m_pose.rootPosition = {walk.rootPosition.x * m_walkWeight,
            walk.rootPosition.y * m_walkWeight, walk.rootPosition.z * m_walkWeight};
        for (std::size_t i = 0; i < JointCount; ++i) m_pose.rotationDegrees[i] = BlendRotation({}, walk.rotationDegrees[i], m_walkWeight);
    }
    if (m_attack != Attack::None) {
        const auto index = m_attack == Attack::Horizontal ? 2u : 3u;
        const Pose attack = Sample(m_clips[index], m_attackTime);
        const float weight = std::clamp(std::min(m_attackTime / 0.08f, (m_clips[index].duration - m_attackTime) / 0.12f), 0.0f, 1.0f);
        for (std::size_t i = static_cast<std::size_t>(Joint::Spine); i <= static_cast<std::size_t>(Joint::RightHand); ++i) {
            m_pose.rotationDegrees[i] = BlendRotation(m_pose.rotationDegrees[i], attack.rotationDegrees[i], weight);
        }
        m_pose.rotation(Joint::Hips).y += attack.rotation(Joint::Hips).y * weight;
    }
}

const char* CharacterMotion::LocomotionName() const {
    return m_jumpActive ? "Jump" : (m_walkWeight > 0.05f ? "Walk" : "Idle");
}
}
