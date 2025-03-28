#pragma once
#include "Game/Lara/lara_struct.h"

// TODO: Merge with Info.h.

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Generic
{
	struct PushableInfo;

	struct PushableAnimSet
	{
		int	 PullAnimNumber = 0;
		int	 PushAnimNumber = 0;
		int	 EdgeAnimNumber = 0;
		bool EnableAnimLoop = 0;

		PushableAnimSet(int pullAnimNumber, int pushAnimNumber, int edgeAnimNumber, bool enableAnimLoop)
		{
			PullAnimNumber = pullAnimNumber;
			PushAnimNumber = pushAnimNumber;
			EdgeAnimNumber = edgeAnimNumber;
			EnableAnimLoop = enableAnimLoop;
		}
	};

	extern std::vector<PushableAnimSet> PushableAnimSets;

	PushableInfo& GetPushableInfo(const ItemInfo& item);
	
	void InitializePushableBlock(int itemNumber);
	void ControlPushableBlock(int itemNumber);
	void CollidePushableBlock(int itemNumber, ItemInfo* playerItem, CollisionInfo* coll);

	int GetPushableHeight(const ItemInfo& item);

	void SetPushableStopperFlag(bool isStopper, const Vector3i& pos, int roomNumber);
}
