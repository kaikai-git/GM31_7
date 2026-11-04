#pragma once

#include "../system/commontypes.h"
#include "../utility/keyframemotion.h"

// Shared skeletal renderer; movement and physics are controlled by the caller.
class CapsuleHumanoid {
public:
    static void Draw(const Motion::Pose& pose, int selectedJoint, bool showSkeleton, bool showSword,
        const Matrix4x4& modelWorld = Matrix4x4::Identity);
};
