#pragma once
#include "Game/items.h"

constexpr auto NUM_RATS = 32;

struct RatData
{
	byte On;
	Pose Pose;
	short RoomNumber;

	short Velocity;
	short VerticalVelocity;

	byte Flags;
	
	Matrix Transform	 = Matrix::Identity;
	Matrix PrevTransform = Matrix::Identity;

	void StoreInterpolationData()
	{
		PrevTransform = Transform;
	}
};

extern int NextRat;
extern RatData Rats[NUM_RATS];

void ClearRats();
short GetNextRat();
void InitializeLittleRats(short itemNumber);
void LittleRatsControl(short itemNumber);
void UpdateRats();
