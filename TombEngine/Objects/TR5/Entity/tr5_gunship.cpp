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

	constexpr float BANK_LERP_SPEED = 8.0f;
	constexpr int MAX_BANK_ANGLE = ANGLE(5);
	constexpr int MAX_REVERSE_PITCH = ANGLE(15);

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
		static float currentPitch = 0.0f;
		static float currentRoll = 0.0f;
		static int storedPitchAngle = 0;
		static int storedRollAngle = 0;
		
		// Reset tilt angles on first frame to prevent initial spin
		if (item->ItemFlags[0] <= 1)
		{
			currentPitch = 0.0f;
			currentRoll = 0.0f;
			storedPitchAngle = 0;
			storedRollAngle = 0;
		}

		// Pitch (forward/back tilt) based on whether Lara is in front or behind the heli
		// Roll (left/right tilt) based on lateral offset from the heli-lara plane
		const float PITCH_MULTIPLIER = 2.5f;
		
		// Use yaw angle to determine if Lara is in front (-Z) or behind (+Z) relative to heli orientation
		float hdx = LaraItem->Pose.Position.x - item->Pose.Position.x;
		float hdz = LaraItem->Pose.Position.z - item->Pose.Position.z;
		float hLen = sqrtf(hdx * hdx + hdz * hdz);
		
		float pitchDirZ = 0.0f;
		if (hLen > 0.1f)
			pitchDirZ = hdz / hLen;
		
		float targetPitch = 0.0f;
		if (moveAmount != 0.0f && hLen > 100.0f)
		{
			float zComponent = pitchDirZ * moveAmount;
			if (zComponent > 0.2f)
			{
				float pitchFactor = zComponent;
				if (hLen < 300.0f)
					pitchFactor *= 0.4f;
				else if (zComponent > 0.8f)
					pitchFactor *= 0.7f;
				targetPitch = MAX_BANK_ANGLE * PITCH_MULTIPLIER * pitchFactor;
			}
			else if (zComponent < -0.2f)
			{
				float pitchFactor = zComponent;
				if (hLen < 300.0f)
					pitchFactor *= 0.4f;
				else if (zComponent < -0.8f)
					pitchFactor *= 0.7f;
				targetPitch = -MAX_REVERSE_PITCH * PITCH_MULTIPLIER * pitchFactor;
			}
		}

		float targetRoll = 0.0f;
		float xComponent = direction.x * (-moveAmount);
		if (xComponent > 0.2f)
			targetRoll = -MAX_BANK_ANGLE * 0.5f * xComponent;
		else if (xComponent < -0.2f)
			targetRoll = MAX_BANK_ANGLE * 0.5f * (-xComponent);

		float tiltLerpAlpha = 1.0f / powf(2.0f, BANK_LERP_SPEED);
		currentPitch += (targetPitch - currentPitch) * tiltLerpAlpha;
		currentRoll  += (targetRoll - currentRoll)  * tiltLerpAlpha;

		// Clamp pitch and roll angles to prevent excessive rotation
		if (currentPitch > MAX_BANK_ANGLE) currentPitch = MAX_BANK_ANGLE;
		if (currentPitch < -MAX_REVERSE_PITCH) currentPitch = -MAX_REVERSE_PITCH;
		if (currentRoll > MAX_BANK_ANGLE) currentRoll = MAX_BANK_ANGLE;
		if (currentRoll < -MAX_BANK_ANGLE) currentRoll = -MAX_BANK_ANGLE;

		int prevStoredPitchAngle = storedPitchAngle;
		storedPitchAngle = (int)currentPitch;
		int pitchDelta = storedPitchAngle - prevStoredPitchAngle;

		// Calculate the delta for roll (z-axis) this frame
		int prevStoredRollAngle = storedRollAngle;
		storedRollAngle = (int)currentRoll;
		int rollDelta = storedRollAngle - prevStoredRollAngle;

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
			float hdx = LaraItem->Pose.Position.x - item->Pose.Position.x;
			float hdz = LaraItem->Pose.Position.z - item->Pose.Position.z;
			float hLen = sqrtf(hdx * hdx + hdz * hdz);
			if (hLen > 0.0f)
			{
				targetVelocityX = (hdx / hLen) * moveAmount * MAX_MOVE_SPEED;
				targetVelocityZ = (hdz / hLen) * moveAmount * MAX_MOVE_SPEED;
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

		if (moveAmount != 0.0f)
		{
			// Reset tilt angles when not moving
			currentPitch += (0.0f - currentPitch) * tiltLerpAlpha;
			currentRoll  += (0.0f - currentRoll)  * tiltLerpAlpha;
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


		int prevStoredYawAngle = item->Pose.Orientation.y;

		constexpr int TRACK_SPEED = 3;
		float lerpAlpha = 1.0f / powf(2.0f, TRACK_SPEED);

		if (item->ItemFlags[0] == 1)
			lerpAlpha = 1.0f;

		item->Pose.Orientation.Lerp(targetOrient, lerpAlpha);

		// Apply pitch/roll as additive offsets to override LERP for proper tilt orientation
		// Apply pitch (X-axis) and roll (Y-axis) per helicopter local coordinate system
		// pitch: +X = nose up, -X = nose down
		// roll: +Y = right bank, -Y = left bank
		item->Pose.Orientation.x += storedPitchAngle;
		item->Pose.Orientation.z += storedRollAngle;

		// Keep yaw from snapping back to the original LERP target
		// Yaw is on Y-axis for helicopter local coordinate system
		const int deltaYaw = item->Pose.Orientation.y - prevStoredYawAngle;
		if (deltaYaw != 0)
			targetOrient.y += deltaYaw;

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
