#pragma once
#include "Game/items.h"
#include "Math/Math.h"

using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto FISH_COUNT_MAX = 2048;

	struct FishData
	{
		int	 MeshIndex	  = 0;
		bool IsPatrolling = false;
		bool IsLethal	  = false;

		Vector3i	Position	   = Vector3i::Zero;
		int			RoomNumber	   = 0;
		Vector3i	PositionTarget = Vector3i::Zero;
		EulerAngles Orientation	   = EulerAngles::Identity;
		float		Velocity	   = 0.0f;

		float Life		 = 0.0f;
		float Undulation = 0.0f;

		ItemInfo* TargetItemPtr = nullptr;
		ItemInfo* LeaderItemPtr = nullptr;

		Matrix Transform	 = Matrix::Identity;
		Matrix PrevTransform = Matrix::Identity;

		void StoreInterpolationData()
		{
			PrevTransform = Transform;
		}
	};

	extern std::vector<FishData> FishSwarm;

	void InitializeFishSwarm(short itemNumber);
	void ControlFishSwarm(short itemNumber);

	void UpdateFishSwarm();
	void RemoveFishSwarm(ItemInfo& item);
	void ClearFishSwarm();
}
