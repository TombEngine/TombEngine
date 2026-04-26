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
	short XTarget;
	short ZTarget;

	unsigned char Flags;

	Matrix Transform	  = Matrix::Identity;
	Matrix PrevTransform = Matrix::Identity;

	void StoreInterpolationData()
	{
		PrevTransform = Transform;
	}
};

extern int NextBat;
extern int BatsAnimFrameOffset;
extern BatData Bats[NUM_BATS];
extern BatData Tr3Bats[NUM_BATS];

void ClearBats();
short GetNextBat();
void InitializeLittleBats(short itemNumber);
void LittleBatsControl(short itemNumber);
void TriggerLittleBat(ItemInfo* item);
void TriggerTr3Bats(ItemInfo* item);
void UpdateTr3Bats();
void UpdateBats();
