#pragma once
#include "framework.h"
#include "Math/Math.h"

namespace TEN::Entities::TR4 
{
	void InitializeCrocgod(short itemNumber);
	void CrocgodControl(short itemNumber);
	void TriggerCrocgodMissileFlame(short fxNumber, short xVel, short yVel, short zVel);
}
