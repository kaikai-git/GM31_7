#include "../utility/charactermotion.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
int checks = 0;
void Check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
void Advance(Motion::CharacterMotion& character, int frames, float step, float ground = 10, float speed = 0) {
    for (int i = 0; i < frames; ++i) character.Update(step, ground, speed, {});
}
float JumpAtRate(int rate) {
    Motion::CharacterMotion character;
    character.Reset(10);
    float peak = 10;
    for (int frame = 0; frame < rate * 2; ++frame) {
        Motion::CharacterCommands input;
        input.jump = frame == 0;
        character.Update(1.0f / rate, 10, 0, input);
        peak = std::max(peak, character.Height());
        Check(character.Height() >= 10, "Character fell below ground.");
        if (character.Jumping()) Check(character.GetPose().rootPosition.y <= 0, "Jump root height was applied twice.");
    }
    Check(character.Grounded() && !character.Jumping(), "Jump did not land and recover.");
    Check(std::abs(character.Height() - 10) < 0.001f, "Landing height is wrong.");
    return peak;
}
}

int main(int argc, char** argv) {
    try {
        Check(argc == 2, "Pass a test output directory.");
        const std::filesystem::path directory = argv[1];
        Motion::CharacterMotion character;
        std::string error;
        Check(character.LoadClips("assets/motion/keyframes", error), error.c_str());
        character.Reset(10);
        Advance(character, 60, 1.0f / 60, 10, Motion::CharacterMotion::WalkSpeed);
        Check(std::string(character.LocomotionName()) == "Walk", "Moving character does not walk.");
        Check(std::abs(character.GetPose().rotation(Motion::Joint::RightThigh).x) > 1, "Loaded walk pose is not playing.");
        Advance(character, 90, 1.0f / 60);
        Check(std::string(character.LocomotionName()) == "Idle", "Stopping does not return to idle.");
        character.Update(0.016f, 25, 0, {});
        Check(character.Grounded() && character.Height() == 25, "Terrain following is incorrect.");

        const float at30 = JumpAtRate(30);
        const float at60 = JumpAtRate(60);
        const float at120 = JumpAtRate(120);
        Check(std::abs(at30 - at120) < 0.2f && std::abs(at60 - at120) < 0.2f, "Jump depends on frame rate.");
        Check(std::abs(at60 - 58) < 0.2f, "Jump apex is not determined by gravity.");

        character.Reset(10);
        character.Update(0.05f, 10, 0, {true, true, false});
        Check(character.Jumping() && character.CurrentAttack() == Motion::Attack::Horizontal, "Jump and attack cannot start together.");
        Advance(character, 8, 0.05f);
        Check(!character.Grounded() && character.CurrentAttack() == Motion::Attack::Horizontal, "Attack cancelled the jump.");
        Check(std::abs(character.GetPose().rotation(Motion::Joint::RightUpperArm).x) > 20, "Slash pose is not applied.");
        Advance(character, 14, 0.05f);
        Check(character.CurrentAttack() == Motion::Attack::None, "Horizontal attack did not finish.");
        Advance(character, 10, 0.05f);
        Check(character.CanReload(), "Character never returned to idle.");
        character.Update(0.05f, 10, 0, {false, false, true});
        Check(character.CurrentAttack() == Motion::Attack::Vertical, "Vertical slash is not selected.");
        Advance(character, 25, 0.05f);
        Check(character.CurrentAttack() == Motion::Attack::None, "Vertical attack loops unexpectedly.");

        Motion::CharacterMotion walking, attacking;
        walking.Reset(10);
        attacking.Reset(10);
        for (int i = 0; i < 15; ++i) {
            walking.Update(0.02f, 10, Motion::CharacterMotion::WalkSpeed, {});
            attacking.Update(0.02f, 10, Motion::CharacterMotion::WalkSpeed, {false, i == 0, false});
        }
        Check(walking.GetPose().rotation(Motion::Joint::LeftThigh).x
            == attacking.GetPose().rotation(Motion::Joint::LeftThigh).x, "Attacking overwrote the walking legs.");
        const float beforeInvalidDelta = character.Height();
        character.Update(std::numeric_limits<float>::infinity(), 10, 0, {});
        Check(character.Height() == beforeInvalidDelta, "Invalid elapsed time modified physics.");

        std::filesystem::create_directories(directory);
        const char* files[] = {"walk.gmotion", "jump.gmotion", "horizontal_slash.gmotion", "vertical_slash.gmotion"};
        for (int i = 0; i < 4; ++i) {
            auto clip = Motion::MakePreset(static_cast<Motion::Preset>(i));
            if (i == 2) for (auto& key : clip.keys) key.pose.rotation(Motion::Joint::Spine).x = 123;
            Check(Motion::Save(clip, directory / files[i], error), "Could not create edited motion fixture.");
        }
        Check(character.LoadClips(directory, error), "Could not load edited motion files.");
        character.Reset(10);
        character.Update(0.1f, 10, 0, {false, true, false});
        character.Update(0.1f, 10, 0, {});
        Check(std::abs(character.GetPose().rotation(Motion::Joint::Spine).x - 123) < 0.001f, "Gameplay did not use the saved motion.");
        { std::ofstream broken(directory / files[3]); broken << "broken"; }
        Check(!character.LoadClips(directory, error), "Broken clip was accepted.");
        Check(character.GetClip(Motion::Preset::HorizontalSlash).keys.front().pose.rotation(Motion::Joint::Spine).x == 123,
            "Failed reload destroyed previously loaded motions.");
        std::cout << "PASS: " << checks << " gameplay motion checks.\n";
    } catch (const std::exception& exception) {
        std::cerr << "FAIL: " << exception.what() << '\n';
        return 1;
    }
}
