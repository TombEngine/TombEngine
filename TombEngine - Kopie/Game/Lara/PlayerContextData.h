#pragma once

namespace TEN::Entities::Player
{
	struct GroundMovementSetupData
	{
		short HeadingAngle	  = 0;
		int	  LowerFloorBound = 0;
		int	  UpperFloorBound = 0;

		bool TestSteepFloorBelow = true;
		bool TestSteepFloorAbove = true;
		bool TestDeathFloor		 = true;
	};

	struct MonkeySwingMovementSetupData
	{
		short HeadingAngle		= 0;
		int	  LowerCeilingBound = 0.0f;
		int	  UpperCeilingBound = 0.0f;
	};

	struct JumpSetupData
	{
		short HeadingAngle = 0;
		float Distance	   = 0.0f;

		bool TestWadeStatus = true;
	};
}
