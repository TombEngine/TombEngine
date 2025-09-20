#pragma once

namespace TEN::Entities::Vehicles
{
	struct MinecartInfo
	{
		int Velocity		 = 0;
		int VerticalVelocity = 0;

		short TurnRot = 0;
		short TurnLen = 0;
		int	  TurnX	  = 0;
		int	  TurnZ	  = 0;

		int Gradient	  = 0;
		int FallTime	  = 0;
		int StopDelayTime = 0;

		int FloorHeightMiddle = 0;
		int FloorHeightFront  = 0;

		int Flags = 0;
	};
}
