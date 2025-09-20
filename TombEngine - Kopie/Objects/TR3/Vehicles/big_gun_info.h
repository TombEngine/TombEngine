#pragma once

namespace TEN::Entities::Vehicles
{
	struct BigGunInfo
	{
		EulerAngles BaseOrientation = EulerAngles::Identity;
		EulerAngles TurnRate = EulerAngles::Identity;
		EulerAngles Rotation = EulerAngles::Identity;
		short BarrelRotation = 0;
		int XOrientFrame = 0;

		unsigned int FireCount = 0;
		bool IsBarrelRotating = false;

		char Flags = 0;
	};
}
