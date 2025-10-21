#pragma once
#include "Game/items.h"
#include "Math/Math.h"

namespace TEN::Entities::TR4
{
	struct LOCUST_INFO
	{
		bool On;
		Pose Pose;
		short RoomNumber;
		int Target;
		Vector3i Offset;
		short Velocity;
		short Counter;

		Matrix Transform     = Matrix::Identity;
		Matrix PrevTransform = Matrix::Identity;

		void StoreInterpolationData()
		{
			PrevTransform = Transform;
		}
	};

    constexpr auto MAX_LOCUSTS = 64;

    extern LOCUST_INFO Locusts[MAX_LOCUSTS];

	void InitializeLocustEmitter(short itemNumber);
	void LocustEmitterControl(short itemNumber);
    int CreateLocust();
    void SpawnLocust(ItemInfo* item);
    void UpdateLocusts();
    void ClearLocusts();
}