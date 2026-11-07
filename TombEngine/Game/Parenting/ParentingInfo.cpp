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

ItemInfo* ParentingInfo::GetParent() const
{
    if (ParentObject < 0 || ParentObject >= (int)g_Level.Items.size())
        return nullptr;

    auto& parent = g_Level.Items[ParentObject];

    // Reject unused or killed item slots.
    if (parent.ObjectNumber == ID_NO_OBJECT || (parent.Flags & IFLAG_KILLED))
        return nullptr;

    return &parent;
}

void ParentingInfo::Update(ItemInfo& item)
{
    if (ParentObject == NO_VALUE)
        return;

    auto* parent = GetParent();
    if (parent == nullptr || parent->Index == item.Index)
    {
        Detach();
        return;
    }

    if (PrevPose == parent->Pose)
        return;

    // Compute the parent's relative rigid motion between frames in world space.
    auto prevRotMatrix = PrevPose.Orientation.ToRotationMatrix();
    auto newRotMatrix = parent->Pose.Orientation.ToRotationMatrix();
    auto deltaRotMatrix = newRotMatrix * prevRotMatrix.Invert();

    // Propagate the parent's motion onto the child's current pose.
    item.Pose.Orientation = EulerAngles(deltaRotMatrix * item.Pose.Orientation.ToRotationMatrix());

    auto relOffset = (item.Pose.Position - PrevPose.Position).ToVector3();
    item.Pose.Position = parent->Pose.Position + Vector3i(Vector3::Transform(relOffset, deltaRotMatrix));

    if (item.IsLara())
        UpdateLaraRoom(&item, -item.GetObb().Extents.y);
    else
        UpdateItemRoom(item.Index);

    PrevPose = parent->Pose;
}

bool ParentingInfo::IsAttached() const
{
    return ParentObject != NO_VALUE;
}
