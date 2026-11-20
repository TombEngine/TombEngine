#pragma once

#include "Game/Animation/Skeleton.h"

struct ItemInfo;

// Poses of the items at the end of each game tick and the ones before it, which the renderer interpolates between. The
// renderer only reads this cache and never poses a skeleton itself.

namespace TEN::Animation
{
	struct ItemPose
	{
		SkeletonPose					   Current	= {};
		std::array<Matrix, BONE_COUNT_MAX> Previous = {};

		int CurrentTick	 = NO_VALUE; // Game tick Current was posed for.
		int PreviousTick = NO_VALUE; // Game tick Previous was posed for.

		// Previous is the pose of the tick before Current; otherwise the item has no pose to interpolate from.
		bool HasPrevious() const;
	};

	void ClearPoseCache();

	// Starts a game tick: the poses of the last tick become the previous ones.
	void BeginPoseTick();

	// Poses the active items from their final state of the game tick.
	void FinalizePoses();

	// Pose of an item at the end of the current game tick, posed now if it was not already.
	const ItemPose& GetItemPoseCache(const ItemInfo& item);
}
