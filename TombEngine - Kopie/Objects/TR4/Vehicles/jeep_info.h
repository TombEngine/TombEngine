#pragma once

namespace TEN::Entities::Vehicles
{
	struct JeepInfo
	{
		int Velocity = 0;
		int CameraElevation = 0;

		short TurnRate = 0;
		short MomentumAngle = 0;
		short ExtraRotation = 0;
		short ExtraRotationDrift = 0;
		short FrontRightWheelRotation = 0;
		short FrontLeftWheelRotation = 0;
		short BackRightWheelRotation = 0;
		short BackLeftWheelRotation = 0;

		int Revs = 0;
		short EngineRevs = 0;
		int Pitch = 0;
		short TrackMesh = 0;

		short Gear = 0;
		short Flags = 0;
	};
}
