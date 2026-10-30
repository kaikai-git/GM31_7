#include "keyframemotion.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <locale>
#include <sstream>
#include <utility>

namespace Motion {
namespace {
constexpr const char* JointNames[] = {
    "hips", "spine", "neck", "head",
    "left_upper_arm", "left_forearm", "left_hand",
    "right_upper_arm", "right_forearm", "right_hand",
    "left_thigh", "left_shin", "left_foot",
    "right_thigh", "right_shin", "right_foot"
};
static_assert(std::size(JointNames) == JointCount);

bool ValidVector(const Float3& value, float limit) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z)
        && std::abs(value.x) <= limit && std::abs(value.y) <= limit && std::abs(value.z) <= limit;
}
float Lerp(float a, float b, float rate) { return a + (b - a) * rate; }
float LerpAngle(float a, float b, float rate) {
    return a + std::remainder(b - a, 360.0f) * rate;
}
bool Expect(std::istream& stream, const char* expected) {
    std::string token;
    return (stream >> token) && token == expected;
}
void WriteVector(std::ostream& stream, const Float3& value) {
    stream << value.x << ' ' << value.y << ' ' << value.z << '\n';
}
bool ReadVector(std::istream& stream, Float3& value) {
    return static_cast<bool>(stream >> value.x >> value.y >> value.z);
}
}

const char* JointName(std::size_t index) {
    return index < JointCount ? JointNames[index] : "unknown";
}

Clip NewClip() {
    Clip clip;
    clip.keys = {{0, {}}, {1, {}}};
    return clip;
}

Clip MakePreset(Preset preset) {
    Clip clip;
    auto key = [&clip](float time, const Pose& pose) { clip.keys.push_back({time, pose}); };
    Pose neutral;
    if (preset == Preset::Walk) {
        clip.name = "Walk";
        clip.duration = 1.2f;
        clip.loop = true;
        for (int i = 0; i <= 4; ++i) {
            Pose pose;
            const float wave = (i == 1) ? 1.0f : ((i == 3) ? -1.0f : 0.0f);
            pose.rootPosition.y = (i % 2 == 0) ? 0.0f : 1.0f;
            pose.rotation(Joint::LeftThigh).x = wave * 28;
            pose.rotation(Joint::RightThigh).x = -wave * 28;
            pose.rotation(Joint::LeftShin).x = -std::max(wave, 0.0f) * 35;
            pose.rotation(Joint::RightShin).x = -std::max(-wave, 0.0f) * 35;
            pose.rotation(Joint::LeftUpperArm).x = -wave * 32;
            pose.rotation(Joint::RightUpperArm).x = wave * 32;
            pose.rotation(Joint::LeftForearm).x = -12;
            pose.rotation(Joint::RightForearm).x = -12;
            key(clip.duration * i / 4.0f, pose);
        }
    } else if (preset == Preset::Jump) {
        clip.name = "Jump";
        clip.duration = 1.4f;
        key(0, neutral);
        Pose crouch;
        crouch.rootPosition.y = -7;
        crouch.rotation(Joint::LeftThigh).x = 38;
        crouch.rotation(Joint::RightThigh).x = 38;
        crouch.rotation(Joint::LeftShin).x = -70;
        crouch.rotation(Joint::RightShin).x = -70;
        crouch.rotation(Joint::Spine).x = 12;
        key(0.25f, crouch);
        Pose airborne;
        airborne.rootPosition.y = 34;
        airborne.rotation(Joint::LeftUpperArm).x = 140;
        airborne.rotation(Joint::RightUpperArm).x = 140;
        airborne.rotation(Joint::LeftThigh).x = 15;
        airborne.rotation(Joint::RightThigh).x = 15;
        airborne.rotation(Joint::LeftShin).x = -30;
        airborne.rotation(Joint::RightShin).x = -30;
        key(0.65f, airborne);
        key(1.1f, crouch);
        key(clip.duration, neutral);
    } else if (preset == Preset::HorizontalSlash) {
        clip.name = "HorizontalSlash";
        clip.duration = 1.0f;
        key(0, neutral);
        Pose ready;
        ready.rotation(Joint::Hips).y = -35;
        ready.rotation(Joint::Spine).y = -25;
        ready.rotation(Joint::RightUpperArm) = {70, -75, -25};
        ready.rotation(Joint::RightForearm).x = -35;
        key(0.3f, ready);
        Pose strike = ready;
        strike.rotation(Joint::Hips).y = 35;
        strike.rotation(Joint::Spine).y = 40;
        strike.rotation(Joint::RightUpperArm) = {70, 65, 15};
        strike.rotation(Joint::RightForearm).x = -10;
        key(0.55f, strike);
        key(0.72f, strike);
        key(clip.duration, neutral);
    } else {
        clip.name = "VerticalSlash";
        clip.duration = 1.2f;
        key(0, neutral);
        Pose raised;
        raised.rotation(Joint::RightUpperArm) = {165, 0, -15};
        raised.rotation(Joint::RightForearm).x = -45;
        raised.rotation(Joint::RightHand).x = 50;
        raised.rotation(Joint::Spine).x = -8;
        key(0.4f, raised);
        Pose strike;
        strike.rotation(Joint::RightUpperArm).x = 65;
        strike.rotation(Joint::RightForearm).x = -10;
        strike.rotation(Joint::RightHand).x = -70;
        strike.rotation(Joint::Spine).x = 20;
        key(0.65f, strike);
        key(0.85f, strike);
        key(clip.duration, neutral);
    }
    return clip;
}

bool Validate(const Clip& clip, std::string& error) {
    error.clear();
    if (clip.name.empty() || clip.name.size() > 127 || clip.name.find_first_of("\r\n") != std::string::npos) {
        error = "Motion name must contain 1-127 bytes without line breaks.";
    } else if (!std::isfinite(clip.duration) || clip.duration < 0.1f || clip.duration > 3600) {
        error = "Duration must be between 0.1 and 3600 seconds.";
    } else if (clip.keys.size() < 2 || clip.keys.size() > 600) {
        error = "A motion must contain 2-600 keyframes.";
    } else {
        float previous = -1;
        for (const auto& key : clip.keys) {
            if (!std::isfinite(key.time) || key.time < 0 || key.time > clip.duration || key.time <= previous) {
                error = "Keyframe times must be strictly increasing within the duration.";
                break;
            }
            if (!ValidVector(key.pose.rootPosition, 10000)) {
                error = "Root position contains an invalid value.";
                break;
            }
            for (const auto& rotation : key.pose.rotationDegrees) {
                if (!ValidVector(rotation, 3600)) {
                    error = "Joint rotation contains an invalid value.";
                    break;
                }
            }
            if (!error.empty()) break;
            previous = key.time;
        }
        if (error.empty() && (clip.keys.front().time != 0 || clip.keys.back().time != clip.duration)) {
            error = "The first key must be at 0 and the last key at the motion duration.";
        }
    }
    return error.empty();
}

Pose Sample(const Clip& clip, float time) {
    if (clip.keys.empty()) return {};
    if (!std::isfinite(time) || time <= clip.keys.front().time) return clip.keys.front().pose;
    if (time >= clip.keys.back().time) return clip.keys.back().pose;
    const auto right = std::upper_bound(clip.keys.begin(), clip.keys.end(), time,
        [](float value, const Keyframe& key) { return value < key.time; });
    const auto& a = *(right - 1);
    const auto& b = *right;
    const float rate = (time - a.time) / (b.time - a.time);
    Pose result;
    result.rootPosition = {
        Lerp(a.pose.rootPosition.x, b.pose.rootPosition.x, rate),
        Lerp(a.pose.rootPosition.y, b.pose.rootPosition.y, rate),
        Lerp(a.pose.rootPosition.z, b.pose.rootPosition.z, rate)
    };
    for (std::size_t i = 0; i < JointCount; ++i) {
        result.rotationDegrees[i] = {
            LerpAngle(a.pose.rotationDegrees[i].x, b.pose.rotationDegrees[i].x, rate),
            LerpAngle(a.pose.rotationDegrees[i].y, b.pose.rotationDegrees[i].y, rate),
            LerpAngle(a.pose.rotationDegrees[i].z, b.pose.rotationDegrees[i].z, rate)
        };
    }
    return result;
}

bool AdvancePlayback(const Clip& clip, float deltaSeconds, float& time) {
    if (!std::isfinite(deltaSeconds) || !std::isfinite(time) || clip.duration <= 0) return false;
    const double next = static_cast<double>(time) + std::max(deltaSeconds, 0.0f);
    if (clip.loop) {
        time = static_cast<float>(std::fmod(next, static_cast<double>(clip.duration)));
        return true;
    }
    time = static_cast<float>(std::min(next, static_cast<double>(clip.duration)));
    return time < clip.duration;
}

bool Save(const Clip& clip, const std::filesystem::path& path, std::string& error) {
    if (!Validate(clip, error)) return false;
    std::ostringstream data;
    data.imbue(std::locale::classic());
    data << std::setprecision(std::numeric_limits<float>::max_digits10);
    data << "GM31_KEYFRAME_MOTION 1\nname " << std::quoted(clip.name)
        << "\nduration " << clip.duration << "\nloop " << clip.loop
        << "\njoints " << JointCount << "\nkeys " << clip.keys.size() << '\n';
    for (const auto& key : clip.keys) {
        data << "key " << key.time << "\nroot ";
        WriteVector(data, key.pose.rootPosition);
        for (std::size_t i = 0; i < JointCount; ++i) {
            data << JointName(i) << ' ';
            WriteVector(data, key.pose.rotationDegrees[i]);
        }
    }
    data << "end\n";
    std::error_code ec;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) { error = "Cannot create the destination directory: " + ec.message(); return false; }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) { error = "Cannot open the destination file for writing."; return false; }
    output << data.str();
    output.close();
    if (!output) { error = "The motion file could not be completely written."; return false; }
    error.clear();
    return true;
}

bool Load(const std::filesystem::path& path, Clip& destination, std::string& error) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size > 2 * 1024 * 1024) {
        error = "File not found, inaccessible, or larger than 2 MiB.";
        return false;
    }
    std::ifstream input(path, std::ios::binary);
    input.imbue(std::locale::classic());
    Clip clip;
    int version = 0, loop = 0;
    std::size_t joints = 0, keys = 0;
    if (!Expect(input, "GM31_KEYFRAME_MOTION") || !(input >> version) || version != 1
        || !Expect(input, "name") || !(input >> std::quoted(clip.name))
        || !Expect(input, "duration") || !(input >> clip.duration)
        || !Expect(input, "loop") || !(input >> loop) || (loop != 0 && loop != 1)
        || !Expect(input, "joints") || !(input >> joints) || joints != JointCount
        || !Expect(input, "keys") || !(input >> keys) || keys < 2 || keys > 600) {
        error = "Invalid motion header, version, joint count, or keyframe count.";
        return false;
    }
    clip.loop = loop != 0;
    clip.keys.resize(keys);
    for (auto& key : clip.keys) {
        if (!Expect(input, "key") || !(input >> key.time)
            || !Expect(input, "root") || !ReadVector(input, key.pose.rootPosition)) {
            error = "Invalid or truncated keyframe.";
            return false;
        }
        for (std::size_t i = 0; i < JointCount; ++i) {
            if (!Expect(input, JointName(i)) || !ReadVector(input, key.pose.rotationDegrees[i])) {
                error = "Invalid joint order, joint name, or rotation.";
                return false;
            }
        }
    }
    if (!Expect(input, "end") || !(input >> std::ws).eof()) {
        error = "Missing end marker or unexpected trailing data.";
        return false;
    }
    if (!Validate(clip, error)) return false;
    destination = std::move(clip);
    return true;
}
}
