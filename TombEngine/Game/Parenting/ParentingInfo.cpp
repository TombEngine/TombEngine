#include "framework.h"
#include "Game/Parenting/ParentingInfo.h"

#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Specific/level.h"

void ParentingInfo::Attach(ItemInfo& parent)
{
    ParentObject = parent.Index;
    PrevPose = parent.Pose;
}

void ParentingInfo::Detach()
{
    ParentObject = NO_VALUE;
}

void ParentingInfo::Update(ItemInfo& item)
{
    if (ParentObject == NO_VALUE)
        return;

    if (ParentObject < 0 || ParentObject >= (int)g_Level.Items.size())
    {
        Detach();
        return;
    }

    auto& parent = g_Level.Items[ParentObject];

    if (parent.Index == item.Index)
    {
        Detach();
        return;
    }

    if (PrevPose == parent.Pose)
        return;

    auto deltaPos = parent.Pose.Position - PrevPose.Position;
    auto deltaOrient = parent.Pose.Orientation - PrevPose.Orientation;

    // Translate with the parent.
    item.Pose.Position += deltaPos;
    item.Pose.Orientation += deltaOrient;

    // Rotate around the parent.
    auto deltaItemPos = (item.Pose.Position - parent.Pose.Position).ToVector3();
    auto rotMatrix = deltaOrient.ToRotationMatrix();
    item.Pose.Position = Vector3i(parent.Pose.Position.ToVector3() + Vector3::Transform(deltaItemPos, rotMatrix));

    if (item.IsLara())
        UpdateLaraRoom(&item, item.Pose.Position.y);
    else
        UpdateItemRoom(item.Index);

    PrevPose = parent.Pose;
}

bool ParentingInfo::IsAttached() const
{
    return ParentObject != NO_VALUE;
}
