#include "framework.h"
#include "Objects/TR5/Entity/tr5_gunship.h"

#include <unordered_map>

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
	constexpr int MAX_BANK_ANGLE_DEG = 8;

	constexpr int SECTOR_SIZE = 1024;
	constexpr float MIN_HORIZONTAL_DIST = 100.0f;
	constexpr float HYSTERESIS_MULTIPLIER = 0.5f;
	constexpr float MAX_MOVE_SPEED = 84.0f;
	constexpr float ACCELERATION = 6.0f;
		constexpr float LARA_TORSO_HEIGHT_OFFSET = 4.0f;
	constexpr float FLOOR_CEILING_DAMPENING_FACTOR = 0.1f;
	constexpr float FLOOR_HEIGHT_OFFSET = 0.0f;
	constexpr float Y_DEADZONE = 1.0f;
	constexpr float SMOOTH_DECAY_FACTOR = 0.999f;
	constexpr float RAD_TO_SHORTS = (float)(65536.0 / (2.0 * PI));

	// Per-item state to avoid sharing static variables across multiple gunships
	struct GunShipState
	{
		bool wasTooClose;
		float currentBankAngle;
		float currentPitch;
		Vector3 smoothedVelocity;
		int shotCounter;

		GunShipState() : wasTooClose(false), currentBankAngle(0.0f), currentPitch(0.0f), smoothedVelocity(0.0f, 0.0f, 0.0f), shotCounter(0) {}
	};

	// Per-gunship state storage keyed by item number
	static std::unordered_map<short, GunShipState>& GetGunShipStates()
	{
		static std::unordered_map<short, GunShipState> states;
		return states;
	}

	void ControlGunShip(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (!TriggerActive(item))
			return;

		SoundEffect(SFX_TR4_HELICOPTER_LOOP, &item->Pose);

		int targetDistanceSectors = item->TriggerFlags;
		if (targetDistanceSectors <= 0)
			targetDistanceSectors = DEFAULT_TARGET_DISTANCE_SECTORS;

		const int targetDistance = targetDistanceSectors * SECTOR_SIZE;

		int dx = LaraItem->Pose.Position.x - item->Pose.Position.x;
		int dy = LaraItem->Pose.Position.y - item->Pose.Position.y;
		int dz = LaraItem->Pose.Position.z - item->Pose.Position.z;

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

		// Hysteresis: prevents oscillation at sector boundaries
		const float hysteresisRange = targetDistance * HYSTERESIS_MULTIPLIER;

		// Horizontal distances for movement and shooting logic
		float hdx = LaraItem->Pose.Position.x - item->Pose.Position.x;
		float hdz = LaraItem->Pose.Position.z - item->Pose.Position.z;
		float hLen = sqrtf(hdx * hdx + hdz * hdz);

		// Horizontal proximity check only (vertical tracking handled by Y-Sync separately)
		bool tooCloseHorizontally = hLen < minimumDistance;

		// Retrieve or initialize per-item state
		GunShipState& state = GetGunShipStates()[itemNumber];

		// Reset state on first frame to prevent spin / carry-over between triggers
		if (item->ItemFlags[0] == 0)
		{
			state.currentBankAngle = 0.0f;
			state.currentPitch = 0.0f;
			state.smoothedVelocity = Vector3(0.0f, 0.0f, 0.0f);
			state.wasTooClose = false;
		}

		// Track frame counter and calculate movement toward/away from Lara
		item->ItemFlags[0]++;

		// Hysteresis logic: track proximity state per-item
		if (tooCloseHorizontally)
		{
			moveAmount = -1.0f;
			state.wasTooClose = true;
		}
		else if (state.wasTooClose && hLen > maxShotsRange + hysteresisRange)
		{
			// Far exceed range after being too close before approaching
			moveAmount = 1.0f;
			state.wasTooClose = false;
		}
		else if (!state.wasTooClose && hLen > maxShotsRange)
		{
			moveAmount = 1.0f;
		}
		else
		{
			moveAmount = 0.0f;
		}

		// Lerp alphas
		const float bankLerpAlpha = 1.0f / powf(2.0f, ROLL_LERP_SPEED);
		const float pitchLerpAlpha = 1.0f / powf(2.0f, PITCH_LERP_SPEED);
		const float accelAlpha = 1.0f / powf(2.0f, ACCELERATION);

		float targetPitch = 0.0f;
		float motion = 0.0f;
		float bankAngle = 0.0f;

		if (moveAmount != 0.0f && hLen > MIN_HORIZONTAL_DIST)
		{
			// Calculate lateral offset for bank angle
			float yawRad = TO_RAD(item->Pose.Orientation.y);
			float fwdX = sinf(yawRad);
			float fwdZ = cosf(yawRad);

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

		// Smooth bank angle and pitch
		state.currentBankAngle += (bankAngle - state.currentBankAngle) * bankLerpAlpha;
		state.currentPitch += (targetPitch - state.currentPitch) * pitchLerpAlpha;

		// Determine target velocity based on movement state
		float targetVelocityX = 0.0f;
		float targetVelocityZ = 0.0f;

		if (moveAmount != 0.0f)
		{
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
		}

		if (moveAmount == 0.0f)
		{
			state.currentPitch *= SMOOTH_DECAY_FACTOR;
			state.currentBankAngle *= SMOOTH_DECAY_FACTOR;
		}

		// Y velocity: track Lara torso height, clamped above floor/ramp level
		const float laraTorsoY = LaraItem->Pose.Position.y + LARA_TORSO_HEIGHT_OFFSET;
		float targetPosY = laraTorsoY;

		FloorInfo* yPosFloorInfo = GetFloor(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z, &item->RoomNumber);
		int yPosFloorHeight = NO_VALUE;
		if (yPosFloorInfo != nullptr)
			yPosFloorHeight = GetFloorHeight(yPosFloorInfo, item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z);

		float targetVelocityY = 0.0f;

		if (hLen < targetDistance)
		{
			targetPosY = laraTorsoY;

			// Higher altitude when flying backwards for obstacle clearance
			if (moveAmount < 0.0f)
				targetPosY += SECTOR_SIZE;

			// Only clamp to floor if it's above Lara: never fly under elevated surfaces
			if (yPosFloorHeight != NO_VALUE && (float)yPosFloorHeight > targetPosY)
				targetPosY = (float)yPosFloorHeight + FLOOR_HEIGHT_OFFSET;

			const float heightDiff = targetPosY - item->Pose.Position.y;

			// Scale Y velocity by horizontal proximity for smoother, more controlled vertical tracking
			float ySpeedScale = 1.0f - (hLen / targetDistance);
			const float maxYSpeed = MAX_MOVE_SPEED * 0.25f;

			if (heightDiff > Y_DEADZONE)
				targetVelocityY = maxYSpeed * ySpeedScale;
			else if (heightDiff < -Y_DEADZONE)
				targetVelocityY = -maxYSpeed * ySpeedScale;
		}

		// Smoothly interpolate velocity toward target (X/Z and Y)
		state.smoothedVelocity.x += (targetVelocityX - state.smoothedVelocity.x) * accelAlpha;
		state.smoothedVelocity.y += (targetVelocityY - state.smoothedVelocity.y) * accelAlpha;
		state.smoothedVelocity.z += (targetVelocityZ - state.smoothedVelocity.z) * accelAlpha;

		// Apply velocity to position
		item->Pose.Position.x += (int)state.smoothedVelocity.x;
		item->Pose.Position.y += (int)state.smoothedVelocity.y;
		item->Pose.Position.z += (int)state.smoothedVelocity.z;

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
		lerpResult.x = (short)(state.currentPitch * RAD_TO_SHORTS);
		lerpResult.z = (short)(state.currentBankAngle * RAD_TO_SHORTS);

		item->Pose.Orientation = lerpResult;

		// Post-position collision: wall sliding first, then floor/ceiling correction
		CollisionInfo coll{};
		auto collObjects = GetCollidedObjects(*item, true, true);

		// Wall collision using ItemPushStatic for sliding along walls
		if (!collObjects.Statics.empty())
		{
			for (const StaticMesh* staticMesh : collObjects.Statics)
				ItemPushStatic(item, *staticMesh, &coll);
		}

		// Floor and ceiling collision check using point collision
		FloorInfo* floorInfo = GetFloor(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z, &item->RoomNumber);
		int floorHeight = NO_VALUE;
		if (floorInfo != nullptr)
			floorHeight = GetFloorHeight(floorInfo, item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z);

		int ceilingHeight = NO_VALUE;
		if (floorInfo != nullptr)
			ceilingHeight = GetCeiling(floorInfo, item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z);

		// Prevent flying through floor (including elevated floor)
		if (floorHeight != NO_VALUE && item->Pose.Position.y > floorHeight)
		{
			item->Pose.Position.y = floorHeight;
			state.smoothedVelocity.y *= FLOOR_CEILING_DAMPENING_FACTOR; // Dampen velocity instead of zeroing
		}

		// Prevent flying through ceiling
		if (ceilingHeight != NO_VALUE && item->Pose.Position.y < ceilingHeight)
		{
			item->Pose.Position.y = ceilingHeight;
			state.smoothedVelocity.y *= FLOOR_CEILING_DAMPENING_FACTOR; // Dampen velocity instead of zeroing
		}

		if (los)
		{
			if (hLen <= maxShotsRange)
			{
				state.shotCounter = 1;

				if (!(GlobalCounter & (FIRE_FIRE_RATE - 1)))
				{
					SoundEffect(SFX_TR4_HK_FIRE, &item->Pose, SoundEnvironment::Land, 0.8f);
				}
			}
			else
			{
				state.shotCounter += (int)(SHOT_COUNTER_SPEED_DEFAULT * ROTOR_ACTIVE_THRESHOLD);
			}
		}
		else
		{
			state.shotCounter += SHOT_COUNTER_MULTIPLIER_NO_LOS;
		}

		if (state.shotCounter <= ROTOR_ACTIVE_THRESHOLD)
			item->MeshBits |= 0x100;
		else
			item->MeshBits &= 0xFEFF;

		AnimateItem(item);
	}
}
