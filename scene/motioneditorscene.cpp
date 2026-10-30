#include "motioneditorscene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <Windows.h>
#include "../Application.h"
#include "../gameobject/capsulehumanoid.h"
#include "../system/BoxDrawer.h"
#include "../system/CapsuleDrawer.h"
#include "../system/DebugUI.h"
#include "../system/SceneClassFactory.h"

REGISTER_CLASS(MotionEditorScene)

namespace {
constexpr float PanelWidth = 460;
const char* PresetLabels[] = {"歩く", "ジャンプ", "剣・横振り", "剣・縦振り"};
const char* PresetFiles[] = {"walk", "jump", "horizontal_slash", "vertical_slash"};
const char* JointLabels[] = {
    "腰（全身の回転）", "胴体", "首", "頭",
    "左上腕", "左ひじ", "左手首", "右上腕", "右ひじ", "右手首",
    "左もも", "左ひざ", "左足首", "右もも", "右ひざ", "右足首"
};
static_assert(std::size(JointLabels) == Motion::JointCount);

std::filesystem::path FindProjectRoot() {
    std::array<wchar_t, 32768> executable{};
    GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    const std::filesystem::path executableDirectory = std::filesystem::path(executable.data()).parent_path();
    for (auto candidate : {std::filesystem::current_path(), executableDirectory}) {
        for (int level = 0; level < 8; ++level) {
            std::error_code error;
            if (std::filesystem::exists(candidate / "shader/common.hlsl", error)) return candidate;
            const auto parent = candidate.parent_path();
            if (parent == candidate || parent.empty()) break;
            candidate = parent;
        }
    }
    return std::filesystem::current_path();
}
}

void MotionEditorScene::init() {
    m_projectRoot = FindProjectRoot();
    std::filesystem::current_path(m_projectRoot);
    SetWindowTextW(Application::GetWindow(), L"GM31 No.7 - Keyframe Motion Editor (Video 1)");
    CapsuleDrawerInit();
    BoxDrawerInit();
    ResetDocument(Motion::NewClip(), false);
    std::snprintf(m_path.data(), m_path.size(), "assets/motion/keyframes/new_motion.gmotion");
    m_status = "プリセット作成、または新規からモーションを作成してください。";
    DebugUI::RedistDebugFunction([this]() { DrawEditor(); });
}

void MotionEditorScene::dispose() {}

void MotionEditorScene::update(uint64_t delta) {
    if (m_playing) {
        const float seconds = std::min(static_cast<float>(delta) / 1000000.0f, 0.25f);
        m_playing = Motion::AdvancePlayback(m_clip, seconds * m_speed, m_time);
        m_pose = Motion::Sample(m_clip, m_time);
        m_selectedKey = -1;
    }
}

void MotionEditorScene::draw(uint64_t /*delta*/) {
    const float width = static_cast<float>(Application::GetWidth());
    const float height = static_cast<float>(Application::GetHeight());
    const float previewWidth = std::max(width - PanelWidth, 100.0f);
    D3D11_VIEWPORT viewport{};
    viewport.Width = previewWidth;
    viewport.Height = height;
    viewport.MaxDepth = 1;
    Renderer::GetDeviceContext()->RSSetViewports(1, &viewport);

    const float yaw = m_cameraYaw * PI / 180;
    const float pitch = m_cameraPitch * PI / 180;
    const Vector3 target(0, m_cameraTargetY, 0);
    const Vector3 eye = target + Vector3(
        m_cameraDistance * std::cos(pitch) * std::sin(yaw),
        m_cameraDistance * std::sin(pitch),
        m_cameraDistance * std::cos(pitch) * std::cos(yaw));
    Matrix4x4 view = DirectX::XMMatrixLookAtLH(eye, target, Vector3(0, 1, 0));
    Matrix4x4 projection = DirectX::XMMatrixPerspectiveFovLH(PI / 4, previewWidth / height, 1, 2000);
    Renderer::SetViewMatrix(&view);
    Renderer::SetProjectionMatrix(&projection);
    Renderer::SetDepthEnable(true);
    Renderer::SetBlendState(BS_NONE);

    BoxDrawerDraw(Matrix4x4::CreateScale(180, 0.5f, 180)
        * Matrix4x4::CreateTranslation(0, -1, 0), Color(0.16f, 0.20f, 0.26f, 1));
    for (int i = -9; i <= 9; ++i) {
        const float coordinate = static_cast<float>(i * 10);
        const Color color = i == 0 ? Color(0.55f, 0.60f, 0.67f, 1) : Color(0.26f, 0.31f, 0.38f, 1);
        BoxDrawerDraw(Matrix4x4::CreateScale(0.25f, 0.05f, 180)
            * Matrix4x4::CreateTranslation(coordinate, -0.7f, 0), color);
        BoxDrawerDraw(Matrix4x4::CreateScale(180, 0.05f, 0.25f)
            * Matrix4x4::CreateTranslation(0, -0.7f, coordinate), color);
    }
    CapsuleHumanoid::Draw(m_pose, m_playing ? -1 : m_joint, m_showSkeleton, m_showSword);
    Renderer::SetBlendState(BS_ALPHABLEND);
    viewport.Width = width;
    Renderer::GetDeviceContext()->RSSetViewports(1, &viewport);
}

void MotionEditorScene::ResetDocument(Motion::Clip clip, bool dirty) {
    m_clip = std::move(clip);
    std::snprintf(m_name.data(), m_name.size(), "%s", m_clip.name.c_str());
    m_durationEdit = m_clip.duration;
    m_time = 0;
    m_selectedKey = 0;
    m_pose = m_clip.keys.front().pose;
    m_playing = false;
    m_dirty = dirty;
    m_poseDirty = false;
}

void MotionEditorScene::SelectTime(float time) {
    m_playing = false;
    m_time = std::clamp(time, 0.0f, m_clip.duration);
    m_pose = Motion::Sample(m_clip, m_time);
    m_poseDirty = false;
    m_selectedKey = -1;
    for (std::size_t i = 0; i < m_clip.keys.size(); ++i) {
        if (std::abs(m_clip.keys[i].time - m_time) < 0.0001f) m_selectedKey = static_cast<int>(i);
    }
}

void MotionEditorScene::SelectKey(int index) {
    if (index < 0 || index >= static_cast<int>(m_clip.keys.size())) return;
    SelectTime(m_clip.keys[index].time);
}

bool MotionEditorScene::StorePose() {
    auto at = std::lower_bound(m_clip.keys.begin(), m_clip.keys.end(), m_time,
        [](const Motion::Keyframe& key, float time) { return key.time < time; });
    if (at != m_clip.keys.begin() && std::abs((at - 1)->time - m_time) < 0.0001f) --at;
    if (at != m_clip.keys.end() && std::abs(at->time - m_time) < 0.0001f) {
        at->pose = m_pose;
    } else {
        if (m_clip.keys.size() >= 600) {
            m_status = "キーは最大600個です。";
            m_statusSuccess = false;
            return false;
        }
        at = m_clip.keys.insert(at, {m_time, m_pose});
    }
    m_selectedKey = static_cast<int>(at - m_clip.keys.begin());
    m_time = at->time;
    m_poseDirty = false;
    m_dirty = true;
    return true;
}

std::filesystem::path MotionEditorScene::FilePath() const {
    const std::string text(m_path.data());
    const std::filesystem::path path(std::u8string(text.begin(), text.end()));
    return path.is_absolute() ? path : m_projectRoot / path;
}

bool MotionEditorScene::SaveFile() {
    if (m_poseDirty && !StorePose()) return false;
    std::string error;
    try {
        if (m_path[0] == '\0' || !Motion::Save(m_clip, FilePath(), error)) {
            m_status = "保存失敗: " + (error.empty() ? std::string("保存先を指定してください。") : error);
            m_statusSuccess = false;
            return false;
        }
    } catch (const std::exception& exception) {
        m_status = "保存失敗: " + std::string(exception.what());
        m_statusSuccess = false;
        return false;
    }
    m_dirty = false;
    m_statusSuccess = true;
    m_status = "保存成功: " + std::string(m_path.data()) + " (" + std::to_string(m_clip.keys.size()) + " keys)";
    return true;
}

bool MotionEditorScene::LoadFile() {
    Motion::Clip loaded;
    std::string error;
    try {
        if (m_path[0] == '\0' || !Motion::Load(FilePath(), loaded, error)) {
            m_status = "読込失敗（現在の編集は保持）: " + (error.empty() ? std::string("ファイルを指定してください。") : error);
            m_statusSuccess = false;
            return false;
        }
    } catch (const std::exception& exception) {
        m_status = "読込失敗（現在の編集は保持）: " + std::string(exception.what());
        m_statusSuccess = false;
        return false;
    }
    ResetDocument(std::move(loaded), false);
    m_statusSuccess = true;
    m_status = "ファイル読込成功: " + std::string(m_path.data()) + " (" + std::to_string(m_clip.keys.size()) + " keys)";
    return true;
}

void MotionEditorScene::RequestReplace(ReplaceAction action) {
    if (m_dirty || m_poseDirty) {
        m_pending = action;
        ImGui::OpenPopup("未保存の編集があります");
    } else {
        ReplaceDocument(action);
    }
}

void MotionEditorScene::ReplaceDocument(ReplaceAction action) {
    if (action == ReplaceAction::New) {
        ResetDocument(Motion::NewClip(), false);
        std::snprintf(m_path.data(), m_path.size(), "assets/motion/keyframes/new_motion.gmotion");
        m_status = "新規モーション。姿勢を編集してキー登録してください。";
        m_statusSuccess = true;
    } else if (action == ReplaceAction::Preset) {
        ResetDocument(Motion::MakePreset(static_cast<Motion::Preset>(m_preset)), true);
        std::snprintf(m_path.data(), m_path.size(), "assets/motion/keyframes/%s.gmotion", PresetFiles[m_preset]);
        m_status = "プリセット作成。関節を編集して保存できます。";
        m_statusSuccess = true;
    } else if (action == ReplaceAction::Sample) {
        std::snprintf(m_path.data(), m_path.size(), "assets/motion/keyframes/%s.gmotion", PresetFiles[m_preset]);
        LoadFile();
    } else if (action == ReplaceAction::Load) {
        LoadFile();
    }
}

void MotionEditorScene::DrawReplaceConfirmation() {
    if (ImGui::BeginPopupModal("未保存の編集があります", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("編集を破棄して続けますか？必要な場合は先に保存してください。");
        if (ImGui::Button("破棄して続行")) {
            ReplaceDocument(m_pending);
            m_pending = ReplaceAction::None;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("キャンセル")) {
            m_pending = ReplaceAction::None;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void MotionEditorScene::DrawTimeline() {
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    constexpr float Padding = 10;
    ImGui::InvisibleButton("timeline", ImVec2(width, 40));
    if (ImGui::IsItemActive() && ImGui::IsMouseDown(0)) {
        float time = std::clamp((ImGui::GetIO().MousePos.x - start.x - Padding) / (width - 2 * Padding), 0.0f, 1.0f)
            * m_clip.duration;
        for (const auto& key : m_clip.keys) {
            if (std::abs(key.time - time) * (width - 2 * Padding) / m_clip.duration < 8) {
                time = key.time;
                break;
            }
        }
        SelectTime(time);
    }
    auto* list = ImGui::GetWindowDrawList();
    list->AddRectFilled(start, ImVec2(start.x + width, start.y + 40), IM_COL32(24, 29, 39, 255), 4);
    const float y = start.y + 20;
    list->AddLine(ImVec2(start.x + Padding, y), ImVec2(start.x + width - Padding, y), IM_COL32(110, 120, 140, 255));
    for (std::size_t i = 0; i < m_clip.keys.size(); ++i) {
        const float x = start.x + Padding + m_clip.keys[i].time / m_clip.duration * (width - 2 * Padding);
        const ImVec2 diamond[] = {{x, y - 6}, {x + 6, y}, {x, y + 6}, {x - 6, y}};
        list->AddConvexPolyFilled(diamond, 4, m_selectedKey == static_cast<int>(i)
            ? IM_COL32(255, 180, 55, 255) : IM_COL32(90, 175, 235, 255));
    }
    const float x = start.x + Padding + m_time / m_clip.duration * (width - 2 * Padding);
    list->AddLine(ImVec2(x, start.y + 2), ImVec2(x, start.y + 38), IM_COL32(255, 90, 90, 255), 2);
}

void MotionEditorScene::DrawEditor() {
    const float height = static_cast<float>(Application::GetHeight());
    const float previewWidth = static_cast<float>(Application::GetWidth()) - PanelWidth;
    ImGui::SetWindowPos("Debug Information", ImVec2(12, height - 78), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(previewWidth, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(PanelWidth, height), ImGuiCond_Always);
    ImGui::Begin("GM31 No.7  モーション編集 / 動画1", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    ImGui::TextUnformatted("作成 > キー登録 > 保存 > 新規 > 読込 > 再生");
    ImGui::SetNextItemWidth(260);
    if (ImGui::InputText("モーション名", m_name.data(), m_name.size())) {
        m_clip.name = m_name.data();
        m_dirty = true;
    }
    if (ImGui::Button("新規")) RequestReplace(ReplaceAction::New);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(135);
    ImGui::Combo("##preset", &m_preset, PresetLabels, 4);
    ImGui::SameLine();
    if (ImGui::Button("作成")) RequestReplace(ReplaceAction::Preset);
    ImGui::SameLine();
    if (ImGui::Button("サンプル読込")) RequestReplace(ReplaceAction::Sample);

    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##file", m_path.data(), m_path.size());
    if (ImGui::Button("保存", ImVec2(85, 0))) SaveFile();
    ImGui::SameLine();
    if (ImGui::Button("読込", ImVec2(85, 0))) RequestReplace(ReplaceAction::Load);
    ImGui::SameLine();
    ImGui::TextUnformatted(m_dirty || m_poseDirty ? "未保存の変更あり" : "保存済 / 未変更");
    ImGui::PushTextWrapPos(0);
    ImGui::TextColored(m_statusSuccess ? ImVec4(0.5f, 1, 0.65f, 1) : ImVec4(1, 0.45f, 0.4f, 1), "%s", m_status.c_str());
    ImGui::PopTextWrapPos();
    ImGui::Separator();

    if (ImGui::Button(m_playing ? "一時停止" : "再生", ImVec2(85, 0))) {
        if (m_playing) {
            m_playing = false;
        } else if ((!m_poseDirty || StorePose())) {
            if (m_time >= m_clip.duration) m_time = 0;
            m_pose = Motion::Sample(m_clip, m_time);
            m_playing = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("停止 / 先頭", ImVec2(110, 0))) SelectKey(0);
    ImGui::SameLine();
    if (ImGui::Checkbox("ループ", &m_clip.loop)) m_dirty = true;
    ImGui::SameLine();
    ImGui::TextUnformatted(m_playing ? "再生中" : "停止中");
    ImGui::SetNextItemWidth(180);
    ImGui::SliderFloat("再生速度", &m_speed, 0.1f, 3, "%.2fx");
    float time = m_time;
    ImGui::SetNextItemWidth(285);
    if (ImGui::SliderFloat("時刻 (秒)", &time, 0, m_clip.duration, "%.3f")) SelectTime(time);
    DrawTimeline();

    if (ImGui::CollapsingHeader("キーフレーム", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginChild("keyList", ImVec2(0, 68), true);
        for (std::size_t i = 0; i < m_clip.keys.size(); ++i) {
            char label[80];
            std::snprintf(label, sizeof(label), "#%02zu   %.3f s", i, m_clip.keys[i].time);
            if (ImGui::Selectable(label, m_selectedKey == static_cast<int>(i))) SelectKey(static_cast<int>(i));
        }
        ImGui::EndChild();
        ImGui::BeginDisabled(m_playing);
        if (ImGui::Button("現在の姿勢をキー登録")) {
            if (StorePose()) { m_status = "キーフレームを登録しました。保存でファイルに書き出せます。"; m_statusSuccess = true; }
        }
        ImGui::SameLine();
        const bool canDelete = m_selectedKey > 0 && m_selectedKey < static_cast<int>(m_clip.keys.size()) - 1;
        ImGui::BeginDisabled(!canDelete);
        if (ImGui::Button("選択キー削除")) {
            m_clip.keys.erase(m_clip.keys.begin() + m_selectedKey);
            SelectTime(m_time);
            m_dirty = true;
        }
        ImGui::EndDisabled();
        ImGui::EndDisabled();
    }
    if (ImGui::CollapsingHeader("姿勢を編集（角度: 度）", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginDisabled(m_playing);
        ImGui::SetNextItemWidth(245);
        ImGui::Combo("関節", &m_joint, JointLabels, static_cast<int>(Motion::JointCount));
        auto& rotation = m_pose.rotationDegrees[m_joint];
        float angles[] = {rotation.x, rotation.y, rotation.z};
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat3("##angles", angles, -180, 180, "%.1f")) {
            rotation = {angles[0], angles[1], angles[2]};
            m_poseDirty = true;
        }
        float position[] = {m_pose.rootPosition.x, m_pose.rootPosition.y, m_pose.rootPosition.z};
        ImGui::SetNextItemWidth(270);
        if (ImGui::DragFloat3("胴体位置 XYZ", position, 0.2f, -100, 100, "%.1f")) {
            m_pose.rootPosition = {position[0], position[1], position[2]};
            m_poseDirty = true;
        }
        if (ImGui::Button("選択関節をリセット")) { rotation = {}; m_poseDirty = true; }
        ImGui::SameLine();
        if (ImGui::Button("全身をリセット")) { m_pose = {}; m_poseDirty = true; }
        if (m_poseDirty) ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "姿勢をキー登録してください（保存時も自動登録）。");
        ImGui::EndDisabled();
    }
    if (ImGui::CollapsingHeader("長さ・プレビュー設定")) {
        ImGui::BeginDisabled(m_playing);
        ImGui::SetNextItemWidth(150);
        ImGui::InputFloat("全体の秒数", &m_durationEdit, 0.1f, 1, "%.2f");
        if (ImGui::Button("長さを適用（キー時刻も伸縮）")) {
            if (std::isfinite(m_durationEdit) && m_durationEdit >= 0.1f && m_durationEdit <= 60) {
                if (m_poseDirty) StorePose();
                const float factor = m_durationEdit / m_clip.duration;
                for (auto& key : m_clip.keys) key.time *= factor;
                m_clip.duration = m_durationEdit;
                m_clip.keys.back().time = m_clip.duration;
                SelectTime(std::min(m_time * factor, m_clip.duration));
                m_dirty = true;
            } else { m_status = "長さは0.1〜60秒で指定してください。"; m_statusSuccess = false; }
        }
        ImGui::EndDisabled();
        ImGui::Checkbox("骨格を表示", &m_showSkeleton);
        ImGui::SameLine();
        ImGui::Checkbox("剣を表示", &m_showSword);
        ImGui::SliderFloat("カメラ方位", &m_cameraYaw, -180, 180, "%.1f");
        ImGui::SliderFloat("カメラ仰角", &m_cameraPitch, -5, 80, "%.1f");
        ImGui::SliderFloat("カメラ距離", &m_cameraDistance, 100, 400, "%.0f");
        ImGui::SliderFloat("注視点の高さ", &m_cameraTargetY, 0, 110, "%.0f");
    }
    DrawReplaceConfirmation();
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.75f);
    ImGui::Begin("Preview status", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);
    ImGui::Text("%s  |  %.3f / %.3f s  |  %zu keys", m_clip.name.c_str(), m_time, m_clip.duration, m_clip.keys.size());
    ImGui::TextUnformatted("モーション作成・保存・読込・再生の確認用プレビュー");
    ImGui::End();
}
