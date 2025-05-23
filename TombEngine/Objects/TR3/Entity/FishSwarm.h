#pragma once
#include "Game/items.h"
#include "Math/Math.h"

using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto FISH_COUNT_MAX = 512;

	struct FishData
	{
		int	 MeshIndex	  = 0;
		bool IsPatrolling = false;
		bool IsLethal	  = false;

		Vector3		Position	   = Vector3::Zero;
		int			RoomNumber	   = 0;
		Vector3		PositionTarget = Vector3::Zero;
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
	void ClearFishSwarm();
}
