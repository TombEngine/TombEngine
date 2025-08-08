#pragma once
#include "Math/Math.h"

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Generic
{
	void TriggerTorchFlame(int fxObject, unsigned char node, Vector3i pos, Vector4 color1, Vector4 color2);
	void DoFlameTorch();
	void GetFlameTorch();
	void TorchControl(short itemNumber);
	void FireCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
