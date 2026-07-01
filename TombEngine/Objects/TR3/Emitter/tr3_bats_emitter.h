#pragma once

#include "Game/items.h"

namespace TEN::Entities::TR3
{
	constexpr auto NUM_TR3_BATS = 64;
	constexpr auto TR3_BATS_PER_EMITTER = 32;
	constexpr float TR3_BAT_WING_SPEED = 1.0f;

	struct Tr3BatData
	{
		bool On = false;
		Pose Pose = {};
		short RoomNumber = NO_VALUE;

		short Velocity = 0;
		short Counter = 0;
		int FrameOffset = 0;

		Matrix Transform = Matrix::Identity;
		Matrix PrevTransform = Matrix::Identity;

		void StoreInterpolationData()
		{
			PrevTransform = Transform;
		}
	};

	extern Tr3BatData Tr3Bats[NUM_TR3_BATS];

	void ClearTr3Bats();
	void InitializeTr3BatsEmitter(short itemNumber);
	void Tr3BatsEmitterControl(short itemNumber);
	void TriggerTr3Bats(ItemInfo* item);
	void UpdateTr3Bats();
}
