#pragma once
#include "Math/Math.h"

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Generic
{
	void DoFlameTorch();
	void GetFlameTorch();
	void TorchControl(short itemNumber);
}
