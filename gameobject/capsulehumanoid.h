#pragma once

#include "../system/commontypes.h"
#include "../utility/keyframemotion.h"

// An editor preview only: no movement input, physics, or gameplay actions.
class CapsuleHumanoid {
public:
    static void Draw(const Motion::Pose& pose, int selectedJoint, bool showSkeleton, bool showSword);
};
