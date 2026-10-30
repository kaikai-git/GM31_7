#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

// File data and interpolation are independent of the renderer and game input.
namespace Motion {
enum class Joint : std::size_t {
    Hips, Spine, Neck, Head,
    LeftUpperArm, LeftForearm, LeftHand,
    RightUpperArm, RightForearm, RightHand,
    LeftThigh, LeftShin, LeftFoot,
    RightThigh, RightShin, RightFoot,
    Count
};
constexpr std::size_t JointCount = static_cast<std::size_t>(Joint::Count);

struct Float3 {
    float x = 0;
    float y = 0;
    float z = 0;
};
struct Pose {
    Float3 rootPosition;
    std::array<Float3, JointCount> rotationDegrees{};
    Float3& rotation(Joint joint) { return rotationDegrees[static_cast<std::size_t>(joint)]; }
    const Float3& rotation(Joint joint) const { return rotationDegrees[static_cast<std::size_t>(joint)]; }
};
struct Keyframe {
    float time = 0;
    Pose pose;
};
struct Clip {
    std::string name = "Untitled";
    float duration = 1;
    bool loop = false;
    std::vector<Keyframe> keys;
};
enum class Preset { Walk, Jump, HorizontalSlash, VerticalSlash };

const char* JointName(std::size_t index);
Clip NewClip();
Clip MakePreset(Preset preset);
bool Validate(const Clip& clip, std::string& error);
Pose Sample(const Clip& clip, float time);
// Returns false when a non-looping clip reaches its last frame.
bool AdvancePlayback(const Clip& clip, float deltaSeconds, float& time);
bool Save(const Clip& clip, const std::filesystem::path& path, std::string& error);
// A failed load leaves destination unchanged.
bool Load(const std::filesystem::path& path, Clip& destination, std::string& error);
}
