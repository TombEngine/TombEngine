#pragma once
#include "Game/items.h"

constexpr auto NUM_SPIDERS = 64;

struct SpiderData
{
	unsigned char On;
	Pose Pose;
	short RoomNumber;

	short Velocity;
	short VerticalVelocity;

	unsigned char Flags;
	
	Matrix Transform	 = Matrix::Identity;
	Matrix PrevTransform = Matrix::Identity;

	void StoreInterpolationData()
	{
		PrevTransform = Transform;
	}
};

extern int NextSpider;
extern SpiderData Spiders[NUM_SPIDERS];

short GetNextSpider();
void ClearSpiders();
void InitializeSpiders(short itemNumber);
void SpidersEmitterControl(short itemNumber);
void UpdateSpiders();