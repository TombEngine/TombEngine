#include "framework.h"
#include "Game/Animation/PoseCache.h"

#include "Game/items.h"
#include "Specific/level.h"

namespace TEN::Animation
{
	static auto Poses = std::vector<ItemPose>{};
	static int	PoseTick = 0;

	static ItemPose& GetPoseEntry(int itemNumber)
	{
		if (itemNumber >= (int)Poses.size())
			Poses.resize(std::max(itemNumber + 1, (int)g_Level.Items.size()));

		return Poses[itemNumber];
	}

	static void UpdateItemPose(const ItemInfo& item, ItemPose& pose)
	{
		if (pose.CurrentTick != PoseTick)
		{
			GetItemPose(item, pose.Current);
			pose.CurrentTick = PoseTick;
		}

		if (item.DisableInterpolation)
			pose.PreviousTick = NO_VALUE;
	}

	bool ItemPose::HasPrevious() const
	{
		return (PreviousTick != NO_VALUE && PreviousTick == CurrentTick - 1);
	}

	void ClearPoseCache()
	{
		Poses.clear();
	}

	void BeginPoseTick()
	{
		PoseTick++;

		for (auto& pose : Poses)
		{
			if (pose.CurrentTick != PoseTick - 1)
				continue;

			pose.Previous = pose.Current.Transforms;
			pose.PreviousTick = pose.CurrentTick;
		}
	}

	void FinalizePoses()
	{
		for (int i = 0; i < (int)g_Level.Items.size(); i++)
		{
			const auto& item = g_Level.Items[i];
			if (!item.Active && !item.IsLara())
				continue;

			UpdateItemPose(item, GetPoseEntry(i));
		}
	}

	const ItemPose& GetItemPoseCache(const ItemInfo& item)
	{
		auto& pose = GetPoseEntry(item.Index);
		UpdateItemPose(item, pose);
		return pose;
	}
}
