#pragma once

#include <array>
#include <filesystem>
#include <string>
#include "../system/IScene.h"
#include "../utility/keyframemotion.h"

class MotionEditorScene : public IScene {
public:
    void init() override;
    void update(uint64_t delta) override;
    void draw(uint64_t delta) override;
    void dispose() override;

private:
    enum class ReplaceAction { None, New, Preset, Load, Sample };
    void DrawEditor();
    void DrawTimeline();
    void DrawReplaceConfirmation();
    void RequestReplace(ReplaceAction action);
    void ReplaceDocument(ReplaceAction action);
    void ResetDocument(Motion::Clip clip, bool dirty);
    void SelectTime(float time);
    void SelectKey(int index);
    bool StorePose();
    bool SaveFile();
    bool LoadFile();
    std::filesystem::path FilePath() const;

    Motion::Clip m_clip;
    Motion::Pose m_pose;
    std::filesystem::path m_projectRoot;
    std::array<char, 128> m_name{};
    std::array<char, 512> m_path{};
    std::string m_status;
    bool m_statusSuccess = true;
    bool m_dirty = false;
    bool m_poseDirty = false;
    bool m_playing = false;
    bool m_showSkeleton = false;
    bool m_showSword = true;
    float m_time = 0;
    float m_speed = 1;
    float m_durationEdit = 1;
    float m_cameraYaw = -135;
    float m_cameraPitch = 15;
    float m_cameraDistance = 210;
    float m_cameraTargetY = 48;
    int m_joint = 7;
    int m_selectedKey = 0;
    int m_preset = 0;
    ReplaceAction m_pending = ReplaceAction::None;
};
