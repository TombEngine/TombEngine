#include "framework.h"
#include "Objects/TR5/Entity/tr5_gunship.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/control/los.h"
#include "Game/effects/debris.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Game/Lara/lara.h"
#include "Math/Geometry.h"
#include "Objects/Generic/Object/objects.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR5
{

	constexpr int DEFAULT_TARGET_DISTANCE_SECTORS = 3;
	constexpr int SHOT_COUNTER_MULTIPLIER_NO_LOS = 2;
	constexpr int ROTOR_ACTIVE_THRESHOLD = 15;
	constexpr int FIRE_FIRE_RATE = 30;

	constexpr float SHOT_COUNTER_SPEED_DEFAULT = 1.0f;

	constexpr float PITCH_LERP_SPEED = 4.0f;
	constexpr float ROLL_LERP_SPEED = 4.0f;

	constexpr int MAX_REVERSE_PITCH_DEG = 30;
	constexpr int MAX_BANK_ANGLE_DEG = 15;

	int GunShipCounter = 0;

	void ControlGunShip(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (!TriggerActive(item))
			return;

		SoundEffect(SFX_TR4_HELICOPTER_LOOP, &item->Pose);

		int targetDistanceSectors = item->TriggerFlags;
		if (targetDistanceSectors <= 0)
			targetDistanceSectors = DEFAULT_TARGET_DISTANCE_SECTORS;

		const int targetDistance = targetDistanceSectors * 1024;

		int dx = LaraItem->Pose.Position.x - item->Pose.Position.x;
		int dy = LaraItem->Pose.Position.y - item->Pose.Position.y;
		int dz = LaraItem->Pose.Position.z - item->Pose.Position.z;
		float currentDistance = sqrtf(dx * dx + dy * dy + dz * dz);

		Vector3 direction(
			(float)dx,
			(float)dy,
			(float)dz
		);

		float directionLength = direction.Length();
		if (directionLength > 0.0f)
			direction /= directionLength;

		GameVector origin = item->Pose.Position;
		GameVector targetPos = LaraItem->Pose.Position;
		bool los = LOS(&origin, &targetPos);

		// Determine movement direction and calculate target bank angle
		float moveAmount = 0.0f;

		const int minimumDistance = BLOCK(item->TriggerFlags);
		const int maxShotsRange = minimumDistance + BLOCK(2);
		bool tooCloseToLara = currentDistance < minimumDistance;

		if (tooCloseToLara || currentDistance < targetDistance)
		{
			moveAmount = -1.0f;
		}
		else if (currentDistance > maxShotsRange)
		{
			moveAmount = 1.0f;
		}
		else
		{
			moveAmount = 0.0f;
		}

		// Calculate pitch (forward/backward tilt) and roll (left/right tilt) using lerp
		static float currentBankAngle = 0.0f;
		static float currentPitch = 0.0f;

		// Reset tilt angles on first frame to prevent initial spin
		if (item->ItemFlags[0] <= 1)
		{
			currentBankAngle = 0.0f;
			currentPitch = 0.0f;
		}

		// Use yaw angle to determine Lara's position relative to heli orientation
		float hdx = LaraItem->Pose.Position.x - item->Pose.Position.x;
		float hdz = LaraItem->Pose.Position.z - item->Pose.Position.z;
		float hLen = sqrtf(hdx * hdx + hdz * hdz);

		float yawRad = TO_RAD(item->Pose.Orientation.y);
		float fwdX = sinf(yawRad);
		float fwdZ = cosf(yawRad);

		float targetPitch = 0.0f;
		float motion = 0.0f;
		float bankAngle = 0.0f;

		if (moveAmount != 0.0f && hLen > 100.0f)
		{
			// Calculate lateral offset for bank angle
			float rightX = fwdZ;
			float rightZ = -fwdX;
			float sideComponent = (hdx * rightX + hdz * rightZ) / hLen;

			if (sideComponent > 0.2f)
				bankAngle = DEG_TO_RAD(MAX_BANK_ANGLE_DEG) * sideComponent;
			else if (sideComponent < -0.2f)
				bankAngle = -DEG_TO_RAD(MAX_BANK_ANGLE_DEG) * (-sideComponent);

			// Calculate forward component for pitch control
			float fwdComponent = (hdx * fwdX + hdz * fwdZ) / hLen;
			motion = moveAmount * fwdComponent;

			if (motion > 0.2f)
			{
				// Forward flight - nose UP tilt
				float pitchFactor = motion;
				if (hLen < 300.0f)
					pitchFactor *= 0.4f;
				else if (motion > 0.8f)
					pitchFactor *= 0.7f;
				targetPitch = -(float)DEG_TO_RAD(MAX_REVERSE_PITCH_DEG) * pitchFactor;
			}
			else if (motion < -0.2f)
			{
				// Reverse flight - nose DOWN tilt
				float pitchFactor = -motion;
				if (hLen < 300.0f)
					pitchFactor *= 0.4f;
				else if (motion < -0.8f)
					pitchFactor *= 0.7f;
				targetPitch = (float)DEG_TO_RAD(MAX_REVERSE_PITCH_DEG) * pitchFactor;
			}
		}

		float bankLerpAlpha = 1.0f / powf(2.0f, ROLL_LERP_SPEED);
		float pitchLerpAlpha = 1.0f / powf(2.0f, PITCH_LERP_SPEED);
		currentBankAngle += (bankAngle - currentBankAngle) * bankLerpAlpha;
		currentPitch += (targetPitch - currentPitch) * pitchLerpAlpha;

		// Track frame counter and calculate movement toward/away from Lara
		item->ItemFlags[0]++;

		// Smooth movement velocity for natural acceleration and deceleration
		static Vector3 smoothedVelocity(0.0f, 0.0f, 0.0f);
		const float ACCELERATION = 6.0f;
		float accelAlpha = 1.0f / powf(2.0f, ACCELERATION);

		// Track home position for returning to altitude when not maneuvering
		static int homeY = item->Pose.Position.y;
		const float HOME_Y_SMOOTH = 4.0f;

		if (item->ItemFlags[0] == 1)
			homeY = item->Pose.Position.y;

		const float MAX_MOVE_SPEED = 84.0f;

		// Determine target velocity based on movement state
		float targetVelocityX = 0.0f;
		float targetVelocityY = 0.0f;
		float targetVelocityZ = 0.0f;

		if (moveAmount != 0.0f)
		{
			// Use horizontal distance projection for movement to avoid excessive vertical drift
			float hdx2 = LaraItem->Pose.Position.x - item->Pose.Position.x;
			float hdz2 = LaraItem->Pose.Position.z - item->Pose.Position.z;
			float hLen2 = sqrtf(hdx2 * hdx2 + hdz2 * hdz2);
			if (hLen2 > 0.0f)
			{
				targetVelocityX = (hdx2 / hLen2) * moveAmount * MAX_MOVE_SPEED;
				targetVelocityZ = (hdz2 / hLen2) * moveAmount * MAX_MOVE_SPEED;
			}
			else
			{
				targetVelocityX = direction.x * moveAmount * MAX_MOVE_SPEED;
				targetVelocityZ = direction.z * moveAmount * MAX_MOVE_SPEED;
			}
			// Y movement - avoid excessive vertical drift
			targetVelocityY = direction.y * moveAmount * MAX_MOVE_SPEED * 0.15f;

			// Update homeY dynamically when moving vertically
			if (moveAmount != 0.0f && targetVelocityY != 0.0f)
				homeY += (int)(targetVelocityY * 0.5f);
		}
		else
		{
			// When not maneuvering, gently return to homeY altitude
			targetVelocityY += (homeY - item->Pose.Position.y) * 0.02f;
		}

		if (moveAmount == 0.0f)
		{
			currentPitch *= 0.95f;
			currentBankAngle *= 0.95f;
		}

		// Smoothly interpolate velocity toward target
		smoothedVelocity.x += (targetVelocityX - smoothedVelocity.x) * accelAlpha;
		smoothedVelocity.y += (targetVelocityY - smoothedVelocity.y) * accelAlpha;
		smoothedVelocity.z += (targetVelocityZ - smoothedVelocity.z) * accelAlpha;

		// Apply velocity to position
		item->Pose.Position.x += (int)smoothedVelocity.x;
		item->Pose.Position.y += (int)smoothedVelocity.y;
		item->Pose.Position.z += (int)smoothedVelocity.z;

		Vector3 vecOrigin = origin.ToVector3();
		Vector3 vecTarget = targetPos.ToVector3();

		EulerAngles targetOrient = Geometry::GetOrientToPoint(vecOrigin, vecTarget);
		targetOrient.y += ANGLE(180.0f);

		// Lerp yaw from GetOrientToPoint (handles horizontal tracking) but use our controlled pitch/roll
		constexpr int TRACK_SPEED = 3;
		float lerpAlpha = 1.0f / powf(2.0f, TRACK_SPEED);

		if (item->ItemFlags[0] == 1)
			lerpAlpha = 1.0f;

		// Interpolate orientation using static Lerp (returns new EulerAngles)
		EulerAngles lerpResult = EulerAngles::Lerp(item->Pose.Orientation, targetOrient, lerpAlpha);

		// Override pitch/roll with our controlled tilt values (convert radians to short angles)
		constexpr float RAD_TO_SHORTS = (float)(65536.0 / (2.0 * PI));
		lerpResult.x = (short)(currentPitch * RAD_TO_SHORTS);
		lerpResult.z = (short)(currentBankAngle * RAD_TO_SHORTS);

		item->Pose.Orientation = lerpResult;

		CollisionInfo coll{};
		CollideSolidStatics(item, &coll);
		const bool blockedByWorld = coll.HitStatic;

		if (los)
		{
			if (currentDistance <= maxShotsRange)
			{
				GunShipCounter = 1;

				if (!(GlobalCounter & (FIRE_FIRE_RATE - 1)))
				{
					SoundEffect(SFX_TR4_HK_FIRE, &item->Pose, SoundEnvironment::Land, 0.8f);
				}
			}
			else
			{
				GunShipCounter += (int)(SHOT_COUNTER_SPEED_DEFAULT * ROTOR_ACTIVE_THRESHOLD);
			}
		}
		else
		{
			GunShipCounter += SHOT_COUNTER_MULTIPLIER_NO_LOS;
		}

		if (GunShipCounter <= ROTOR_ACTIVE_THRESHOLD)
			item->MeshBits |= 0x100;
		else
			item->MeshBits &= 0xFEFF;

		AnimateItem(item);
	}
}