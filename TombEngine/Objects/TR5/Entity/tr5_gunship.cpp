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
	constexpr int FIRE_FIRE_RATE = 4;

	constexpr float SHOT_COUNTER_SPEED_DEFAULT = 1.0f;

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

		if (los && currentDistance > 2048.0f)
		{
			Vector3 vecOrigin = origin.ToVector3();
			Vector3 vecTarget = targetPos.ToVector3();

			EulerAngles targetOrient = Geometry::GetOrientToPoint(vecOrigin, vecTarget);
			targetOrient.z += ANGLE(180.0f);

			const int MAX_PITCH = ANGLE(185.0);
			if (targetOrient.x < -MAX_PITCH) targetOrient.x = -MAX_PITCH;
			if (targetOrient.x > MAX_PITCH) targetOrient.x = MAX_PITCH;

			const int TRACK_SPEED = 3;
			float lerpAlpha = 1.0f / powf(2.0f, TRACK_SPEED);

			if (item->Timer == 0)
				lerpAlpha = 1.0f;

			item->Pose.Orientation.Lerp(targetOrient, lerpAlpha);
		}

		const int MOVE_SPEED = 84;

		const int minimumDistance = BLOCK(item->TriggerFlags);
		bool tooCloseToLara = currentDistance < minimumDistance;

		CollisionInfo coll{};
		CollideSolidStatics(item, &coll);
		const bool blockedByWorld = coll.HitStatic;

		if (!blockedByWorld)
		{
			float verticalFactor = 0.5f;
			if (tooCloseToLara || currentDistance < targetDistance)
			{
				item->Pose.Position.x -= static_cast<int>(direction.x * MOVE_SPEED);
				item->Pose.Position.y -= static_cast<int>(direction.y * MOVE_SPEED * verticalFactor);
				item->Pose.Position.z -= static_cast<int>(direction.z * MOVE_SPEED);
			}
			else
			{
				item->Pose.Position.x += static_cast<int>(direction.x * MOVE_SPEED);
				item->Pose.Position.y += static_cast<int>(direction.y * MOVE_SPEED * verticalFactor);
				item->Pose.Position.z += static_cast<int>(direction.z * MOVE_SPEED);
			}
		}

		if (los)
		{
			const float MAX_SHOT_RANGE = targetDistance + BLOCK(2);
			
			if (currentDistance <= MAX_SHOT_RANGE)
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