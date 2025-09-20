#pragma once
#include "Math/Math.h"

namespace TEN::Entities::Vehicles
{
	struct KayakInfo
	{
		int TurnRate = 0;

		int Velocity = 0;
		int FrontVerticalVelocity = 0;
		int LeftVerticalVelocity = 0;
		int RightVerticalVelocity = 0;

		Pose OldPose = Pose();
		unsigned int LeftRightPaddleCount = 0;
		int WaterHeight = 0;
		bool Turn = false;
		bool Forward = false;
		bool TrueWater = false;

		char Flags = 0;
	};
}
