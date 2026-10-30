#include "../utility/keyframemotion.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace {
int checks = 0;
void Check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
bool Equal(const Motion::Float3& a, const Motion::Float3& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}
bool Equal(const Motion::Pose& a, const Motion::Pose& b) {
    if (!Equal(a.rootPosition, b.rootPosition)) return false;
    for (std::size_t i = 0; i < Motion::JointCount; ++i) {
        if (!Equal(a.rotationDegrees[i], b.rotationDegrees[i])) return false;
    }
    return true;
}
bool Equal(const Motion::Clip& a, const Motion::Clip& b) {
    if (a.name != b.name || a.duration != b.duration || a.loop != b.loop || a.keys.size() != b.keys.size()) return false;
    for (std::size_t i = 0; i < a.keys.size(); ++i) {
        if (a.keys[i].time != b.keys[i].time || !Equal(a.keys[i].pose, b.keys[i].pose)) return false;
    }
    return true;
}
std::string ReadText(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
void WriteText(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary);
    output << text;
}
}

int main(int argc, char** argv) {
    try {
        if (argc == 3 && std::string(argv[1]) == "--write-presets") {
            const char* files[] = {"walk", "jump", "horizontal_slash", "vertical_slash"};
            std::string error;
            for (int i = 0; i < 4; ++i) {
                Check(Motion::Save(Motion::MakePreset(static_cast<Motion::Preset>(i)),
                    std::filesystem::path(argv[2]) / (std::string(files[i]) + ".gmotion"), error), error.c_str());
            }
            std::cout << "Wrote 4 serialized motion presets.\n";
            return 0;
        }
        Check(argc == 2, "Pass a test artifact directory.");
        const std::filesystem::path directory = argv[1];
        std::filesystem::create_directories(directory);
        std::string error;
        for (int preset = 0; preset < 4; ++preset) {
            const auto clip = Motion::MakePreset(static_cast<Motion::Preset>(preset));
            Check(Motion::Validate(clip, error), "Invalid preset.");
            const auto path = directory / ("preset-" + std::to_string(preset) + ".gmotion");
            Check(Motion::Save(clip, path, error), "Could not save preset.");
            Motion::Clip restored;
            Check(Motion::Load(path, restored, error), "Could not load preset.");
            Check(Equal(clip, restored), "Preset data changed during serialization.");
            for (int frame = 0; frame <= 120; ++frame) {
                const float time = clip.duration * frame / 120.0f;
                Check(Equal(Motion::Sample(clip, time), Motion::Sample(restored, time)), "Restored playback differs.");
                const auto pose = Motion::Sample(restored, time);
                Check(std::isfinite(pose.rootPosition.y), "Nonfinite preview position.");
            }
        }

        auto clip = Motion::NewClip();
        clip.name = "Quoted \"name\" and \\ backslash";
        clip.keys[1].pose.rootPosition = {1.2345678f, 25.125f, -0.000001f};
        for (std::size_t i = 0; i < Motion::JointCount; ++i) {
            const float index = static_cast<float>(i);
            clip.keys[1].pose.rotationDegrees[i] = {index * 3.1415927f, -index * 1.234567f, index / 7.0f};
        }
        const auto valid = directory / "precise.gmotion";
        Check(Motion::Save(clip, valid, error), "Precise data save failed.");
        Motion::Clip restored;
        Check(Motion::Load(valid, restored, error) && Equal(clip, restored), "Precision or quoted name was lost.");
        Check(Equal(Motion::Sample(clip, -1), clip.keys.front().pose), "Negative sample must clamp.");
        Check(Equal(Motion::Sample(clip, 100), clip.keys.back().pose), "End sample must clamp.");

        auto interpolate = Motion::NewClip();
        interpolate.keys[0].pose.rotation(Motion::Joint::Head).y = 170;
        interpolate.keys[1].pose.rotation(Motion::Joint::Head).y = -170;
        interpolate.keys[1].pose.rootPosition.y = 20;
        const auto halfway = Motion::Sample(interpolate, 0.5f);
        Check(std::abs(halfway.rotation(Motion::Joint::Head).y - 180) < 0.001f, "Angle interpolation did not take shortest path.");
        Check(halfway.rootPosition.y == 10, "Position interpolation is incorrect.");
        float time = 0.9f;
        Check(!Motion::AdvancePlayback(interpolate, 1, time) && time == 1, "Nonloop playback must stop at end.");
        interpolate.loop = true;
        time = 0.5f;
        Check(Motion::AdvancePlayback(interpolate, 5.75f, time) && std::abs(time - 0.25f) < 0.001f, "Loop wrap is incorrect.");
        Check(!Motion::AdvancePlayback(interpolate, std::numeric_limits<float>::infinity(), time), "Invalid delta accepted.");

        const auto broken = directory / "broken.gmotion";
        const auto before = restored;
        const auto basicFile = directory / "basic.gmotion";
        Check(Motion::Save(Motion::NewClip(), basicFile, error), "Basic save failed.");
        const std::string basic = ReadText(basicFile);
        auto reject = [&](std::string data) {
            WriteText(broken, data);
            Check(!Motion::Load(broken, restored, error), "Malformed file was accepted.");
            Check(!error.empty() && Equal(restored, before), "Failed load damaged current motion.");
        };
        reject("");
        reject(basic.substr(0, basic.size() / 2));
        reject(basic + "unexpected\n");
        auto change = [&](const std::string& oldText, const std::string& newText) {
            std::string result = basic;
            const auto position = result.find(oldText);
            Check(position != std::string::npos, "Test token not found.");
            result.replace(position, oldText.size(), newText);
            return result;
        };
        reject(change("MOTION 1", "MOTION 999"));
        reject(change("keys 2", "keys 999999999"));
        reject(change("key 1\n", "key 0\n"));
        reject(change("right_hand", "unrecognized_hand"));
        reject(change("joints 16", "joints 15"));
        Check(!Motion::Load(directory / "missing.gmotion", restored, error) && Equal(restored, before), "Missing file modified destination.");

        const std::string originalFile = ReadText(valid);
        auto invalid = clip;
        invalid.keys[0].pose.rootPosition.x = std::numeric_limits<float>::quiet_NaN();
        Check(!Motion::Save(invalid, valid, error), "NaN was saved.");
        Check(ReadText(valid) == originalFile, "Invalid save overwrote existing data.");
        invalid = clip;
        invalid.keys[1].time = 0;
        Check(!Motion::Validate(invalid, error), "Duplicate key times accepted.");
        invalid = clip;
        invalid.duration = -1;
        Check(!Motion::Validate(invalid, error), "Negative duration accepted.");
        Check(!Motion::Save(clip, directory, error), "Saving onto a directory must fail.");
        std::cout << "PASS: " << checks << " checks (serialization, playback, and invalid input).\n";
    } catch (const std::exception& exception) {
        std::cerr << "FAIL: " << exception.what() << '\n';
        return 1;
    }
}
