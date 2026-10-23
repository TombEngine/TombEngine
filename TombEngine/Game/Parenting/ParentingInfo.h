#pragma once

#include "Math/Objects/Pose.h"
#include "Types.h"

struct ItemInfo;

// Describes a one-way parenting relationship in which an item follows a parent item.
struct ParentingInfo
{
    int  ParentObject = NO_VALUE; // Parent item index. NO_VALUE means no parent.
    Pose PrevPose     = Pose::Zero;

    void Attach(ItemInfo& parent);
    void Detach();
    void Update(ItemInfo& item);

    bool IsAttached() const;
};
