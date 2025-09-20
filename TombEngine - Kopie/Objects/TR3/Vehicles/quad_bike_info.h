#pragma once

namespace TEN::Entities::Vehicles
{
	struct QuadBikeInfo
	{
		short TurnRate = 0;
		short FrontRot = 0;
		short RearRot = 0;
		short MomentumAngle = 0;
		short ExtraRotation = 0;

		int Velocity = 0;
		int LeftVerticalVelocity = 0;
		int RightVerticalVelocity = 0;

		int Revs = 0;
		int EngineRevs = 0;
		int Pitch = 0;

		int SmokeStart = 0;
		bool CanStartDrift = 0;
		bool DriftStarting = 0;
		bool NoDismount = 0;

		char Flags = 0;
	};
}
