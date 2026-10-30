#include "player.h"
#include "field.h"
#include "wall.h"
#include "capsulehumanoid.h"
#include <algorithm>
#include <cmath>
#include "../Application.h"
#include "../system/Inputmanager.h"
#include "../system/collision.h"
#include "../system/imgui/imgui.h"

namespace {
using namespace GM31::GE::Collision;

BoundingBoxOBB WallBounds(const wall& object) {
    BoundingBoxOBB box;
    SRT transform = object.getSRT();
    box.lengthx = object.getwidth();
    box.lengthy = object.getheight();
    box.lengthz = object.getthick();
    box.worldcenter = transform.pos;
    transform.scale = Vector3(1, 1, 1);
    const Matrix4x4 matrix = transform.GetMatrix();
    box.axisX = matrix.Right();
    box.axisY = matrix.Up();
    box.axisZ = matrix.Forward();
    return box;
}

bool HitsWallAt(const Vector3& feet, const wall& object) {
    // For walls rotated about Y, this point minimizes the capsule/box distance.
    BoundingSphere sphere;
    sphere.radius = player::COLLISION_RADIUS;
    sphere.center = feet;
    sphere.center.y = std::clamp(object.getSRT().pos.y,
        feet.y + sphere.radius, feet.y + player::MODEL_HEIGHT - sphere.radius);
    return CollisionSphereOBB(sphere, WallBounds(object));
}
}

void player::init() {
    m_srt = {};
    m_srt.rot.y = PI;
    m_targetYaw = PI;
    m_velocity = {};
    m_motion.Reset(0);
    reloadMotions();
}

void player::reloadMotions() {
    std::string error;
    m_motionsLoaded = m_motion.LoadClips("assets/motion/keyframes", error);
    m_motionStatus = m_motionsLoaded ? "Loaded 4 .gmotion files" : "Using previous/default clips: " + error;
}

bool player::overlapsWall(const wall& object) const {
    return HitsWallAt(m_srt.pos, object);
}

void player::update(uint64_t delta) {
    const float seconds = std::min(static_cast<float>(delta) / 1000000.0f, 0.05f);
    if (seconds <= 0) return;
    field* terrain = m_ownerscene->GetGameObject<field>();
    Camera* camera = m_ownerscene->getcamera();
    if (!terrain || !camera) return;
    auto& input = CInputManager::GetInstance();
    const bool hasFocus = GetForegroundWindow() == Application::GetWindow();
    const bool captured = ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureKeyboard;
    const bool acceptInput = hasFocus && !captured;

    const Matrix4x4 view = camera->GetViewMatrix();
    Vector3 forward(view._13, 0, view._33);
    Vector3 right(view._11, 0, view._31);
    if (forward.LengthSquared() < 0.0001f) forward = Vector3(0, 0, 1);
    if (right.LengthSquared() < 0.0001f) right = Vector3(1, 0, 0);
    forward.Normalize();
    right.Normalize();
    Vector3 direction{};
    Motion::CharacterCommands commands;
    if (acceptInput) {
        if (input.IsKeyPressed(DIK_W)) direction += forward;
        if (input.IsKeyPressed(DIK_S)) direction -= forward;
        if (input.IsKeyPressed(DIK_D)) direction += right;
        if (input.IsKeyPressed(DIK_A)) direction -= right;
        commands.jump = input.IsKeyTriggered(DIK_SPACE);
        commands.horizontalSlash = input.IsKeyTriggered(DIK_J);
        commands.verticalSlash = input.IsKeyTriggered(DIK_K);
        if (input.IsKeyTriggered(DIK_R)) {
            if (m_motion.CanReload()) reloadMotions();
            else m_motionStatus = "Finish the current jump/attack before reloading.";
        }
    }
    if (direction.LengthSquared() > 0.0001f) {
        direction.Normalize();
        m_targetYaw = std::atan2(-direction.x, -direction.z);
    }
    const float blend = 1 - std::exp(-12 * seconds);
    m_velocity = Vector3::Lerp(m_velocity, direction * Motion::CharacterMotion::WalkSpeed, blend);
    if (m_velocity.LengthSquared() < 0.01f) m_velocity = {};
    m_srt.rot.y += std::remainder(m_targetYaw - m_srt.rot.y, 2 * PI) * blend;
    m_srt.rot.y = std::remainder(m_srt.rot.y, 2 * PI);

    const Vector3 previous = m_srt.pos;
    Vector3 next = previous + m_velocity * seconds;
    const float limitX = std::max(terrain->GetSizeX() * 0.5f - COLLISION_RADIUS, 0.0f);
    const float limitZ = std::max(terrain->GetSizeZ() * 0.5f - COLLISION_RADIUS, 0.0f);
    next.x = std::clamp(next.x, -limitX, limitX);
    next.z = std::clamp(next.z, -limitZ, limitZ);
    const auto walls = m_ownerscene->GetGameObjects<wall>();
    m_ishitwall = false;
    bool unresolved = false;
    for (int iteration = 0; iteration < MAX_WALL_RESOLVE; ++iteration) {
        unresolved = false;
        for (const wall* object : walls) {
            const auto plane = object->getEquation().GetPlaneInfo().plane;
            const float clearance = COLLISION_RADIUS + object->getthick() * 0.5f;
            if (std::fabs(GM31::GE::Collision::calcPlaneDistance(plane, next)) >= clearance) continue;
            if (!HitsWallAt(next, *object)) continue;
            const Vector3 correction = GM31::GE::Collision::calcPenetrationVector(plane, previous, next, clearance);
            if (correction.LengthSquared() <= WALL_EPSILON) continue;
            next += correction;
            m_ishitwall = unresolved = true;
        }
        if (!unresolved) break;
    }
    if (unresolved || std::fabs(next.x) > limitX || std::fabs(next.z) > limitZ) {
        next.x = previous.x;
        next.z = previous.z;
    }
    const Vector3 traveled = next - previous;
    m_speed = std::sqrt(traveled.x * traveled.x + traveled.z * traveled.z) / seconds;
    m_velocity = Vector3(traveled.x / seconds, 0, traveled.z / seconds);
    m_srt.pos.x = next.x;
    m_srt.pos.z = next.z;
    m_motion.Update(seconds, terrain->GetHeight2(m_srt.pos), m_speed, commands);
    m_srt.pos.y = m_motion.Height();
}

void player::draw(uint64_t /*delta*/) {
    CapsuleHumanoid::Draw(m_motion.GetPose(), -1, false, true, m_srt.GetMatrix());
}

void player::dispose() {}
