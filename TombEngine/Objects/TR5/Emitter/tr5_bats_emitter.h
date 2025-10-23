#pragma once
#include "Game/items.h"

constexpr auto NUM_BATS = 64;

struct BatData
{
	bool On;
	Pose Pose;
	short RoomNumber;

	short Velocity;
	short Counter;
	short LaraTarget;
	byte XTarget;
	byte ZTarget;

	byte Flags;
	
	Matrix Transform	 = Matrix::Identity;
	Matrix PrevTransform = Matrix::Identity;

	void StoreInterpolationData()
	{
		PrevTransform = Transform;
	}
};

extern int NextBat;
extern BatData Bats[NUM_BATS];

short GetNextBat();
void InitializeLittleBats(short itemNumber);
void LittleBatsControl(short itemNumber);
void TriggerLittleBat(ItemInfo* item);
void UpdateBats();
