#pragma once
#include "Math/Math.h"

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Generic
{
	void TriggerTorchFlame(int fxObject, unsigned char node);
	void DoFlameTorch();
	void GetFlameTorch();
	void TorchControl(short itemNumber);
	void FireCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
