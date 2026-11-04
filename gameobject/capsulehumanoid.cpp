#include "capsulehumanoid.h"

#include <array>
#include <cmath>
#include "../system/BoxDrawer.h"
#include "../system/CapsuleDrawer.h"
#include "../system/SphereDrawer.h"

namespace {
using Motion::Joint;
struct Bone {
    int parent;
    Vector3 offset;
};
const std::array<Bone, Motion::JointCount> Bones = {{
    {-1, {0, 35, 0}}, {0, {0, 8, 0}}, {1, {0, 19, 0}}, {2, {0, 9, 0}},
    {1, {-13, 12, 0}}, {4, {-3, -12, 0}}, {5, {-1, -11, -1}},
    {1, {13, 12, 0}}, {7, {3, -12, 0}}, {8, {1, -11, -1}},
    {0, {-6, 0, 0}}, {10, {0, -14, 0}}, {11, {0, -14, 0}},
    {0, {6, 0, 0}}, {13, {0, -14, 0}}, {14, {0, -14, 0}}
}};

void Segment(const Vector3& start, const Vector3& end, float radius,
    const Color& color, const Matrix4x4& parent)
{
    Vector3 up = end - start;
    const float length = up.Length();
    if (length < 0.0001f) return;
    up /= length;
    const Vector3 reference = std::fabs(up.z) < 0.99f ? Vector3(0, 0, 1) : Vector3(1, 0, 0);
    Vector3 right = up.Cross(reference);
    right.Normalize();
    const Vector3 forward = right.Cross(up);
    const Vector3 center = (start + end) * 0.5f;
    Matrix4x4 local = Matrix4x4::Identity;
    local._11 = right.x; local._12 = right.y; local._13 = right.z;
    local._21 = up.x; local._22 = up.y; local._23 = up.z;
    local._31 = forward.x; local._32 = forward.y; local._33 = forward.z;
    local._41 = center.x; local._42 = center.y; local._43 = center.z;
    CapsuleDrawerDraw(radius, length, color, local * parent);
}
}

void CapsuleHumanoid::Draw(const Motion::Pose& pose, int selectedJoint, bool showSkeleton, bool showSword,
    const Matrix4x4& modelWorld) {
    std::array<Matrix4x4, Motion::JointCount> world;
    constexpr float ToRadians = PI / 180.0f;
    for (std::size_t i = 0; i < world.size(); ++i) {
        const auto& angle = pose.rotationDegrees[i];
        Vector3 offset = Bones[i].offset;
        if (Bones[i].parent < 0) {
            offset += Vector3(pose.rootPosition.x, pose.rootPosition.y, pose.rootPosition.z);
        }
        world[i] = Matrix4x4::CreateFromYawPitchRoll(angle.y * ToRadians, angle.x * ToRadians, angle.z * ToRadians)
            * Matrix4x4::CreateTranslation(offset);
        if (Bones[i].parent >= 0) world[i] *= world[Bones[i].parent];
        else world[i] *= modelWorld;
    }
    const Color shirt(0.12f, 0.48f, 0.85f, 1);
    const Color trousers(0.13f, 0.19f, 0.30f, 1);
    const Color skin(1.0f, 0.76f, 0.55f, 1);
    const Color shoes(0.08f, 0.09f, 0.12f, 1);
    const auto part = [&](Joint joint, Vector3 start, Vector3 end, float radius, Color color) {
        const auto index = static_cast<std::size_t>(joint);
        if (selectedJoint == static_cast<int>(index)) color = Color(1.0f, 0.65f, 0.12f, 1);
        Segment(start, end, radius, color, world[index]);
    };
    part(Joint::Hips, {-6, 0, 0}, {6, 0, 0}, 7, trousers);
    part(Joint::Spine, {0, 0, 0}, {0, 12, 0}, 10, shirt);
    part(Joint::Neck, {0, 0, 0}, {0, 4, 0}, 3.5f, skin);
    part(Joint::Head, {0, 0, 0}, {0, 4, 0}, 8, skin);
    part(Joint::LeftUpperArm, {0, 0, 0}, Bones[5].offset, 3.5f, shirt);
    part(Joint::LeftForearm, {0, 0, 0}, Bones[6].offset, 3, skin);
    part(Joint::LeftHand, {0, 0, 0}, {0, -3, 0}, 3.5f, skin);
    part(Joint::RightUpperArm, {0, 0, 0}, Bones[8].offset, 3.5f, shirt);
    part(Joint::RightForearm, {0, 0, 0}, Bones[9].offset, 3, skin);
    part(Joint::RightHand, {0, 0, 0}, {0, -3, 0}, 3.5f, skin);
    part(Joint::LeftThigh, {0, 0, 0}, Bones[11].offset, 4.5f, trousers);
    part(Joint::LeftShin, {0, 0, 0}, Bones[12].offset, 4, trousers);
    part(Joint::LeftFoot, {0, -3, 1}, {0, -3, -6}, 4, shoes);
    part(Joint::RightThigh, {0, 0, 0}, Bones[14].offset, 4.5f, trousers);
    part(Joint::RightShin, {0, 0, 0}, Bones[15].offset, 4, trousers);
    part(Joint::RightFoot, {0, -3, 1}, {0, -3, -6}, 4, shoes);

    const auto& head = world[static_cast<std::size_t>(Joint::Head)];
    for (float side : {-1.0f, 1.0f}) {
        SphereDrawerDraw(Matrix4x4::CreateScale(1.2f)
            * Matrix4x4::CreateTranslation(side * 2.8f, 3, -7.4f) * head, Color(0.04f, 0.05f, 0.07f, 1));
    }
    Segment({0, -1, -7}, {0, -1, -9}, 1.5f, skin, head);

    if (showSword) {
        const auto& hand = world[static_cast<std::size_t>(Joint::RightHand)];
        Segment({0, -1, 0}, {0, -1, -6}, 1.8f, Color(0.22f, 0.14f, 0.08f, 1), hand);
        BoxDrawerDraw(Matrix4x4::CreateScale(12, 2, 2)
            * Matrix4x4::CreateTranslation(0, -1, -7) * hand, Color(0.85f, 0.61f, 0.16f, 1));
        BoxDrawerDraw(Matrix4x4::CreateScale(2.5f, 1, 32)
            * Matrix4x4::CreateTranslation(0, -1, -24) * hand, Color(0.82f, 0.90f, 1, 1));
    }
    if (showSkeleton) {
        Renderer::SetDepthEnable(false);
        for (std::size_t i = 0; i < world.size(); ++i) {
            const Color color = selectedJoint == static_cast<int>(i) ? Color(1, 0.3f, 0, 1) : Color(1, 1, 0.3f, 1);
            SphereDrawerDraw(Matrix4x4::CreateScale(1.8f)
                * Matrix4x4::CreateTranslation(world[i].Translation()), color);
            if (Bones[i].parent >= 0) {
                Segment(world[i].Translation(), world[Bones[i].parent].Translation(), 0.4f, color, Matrix4x4::Identity);
            }
        }
        Renderer::SetDepthEnable(true);
    }
}
