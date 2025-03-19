#include "framework.h"
#include "Objects/TR5/Trap/MovingLaser.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/collision/Sphere.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/effects/light.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Collision::Sphere;

namespace TEN::Entities::Traps
{
	constexpr auto MOVING_LASER_DAMAGE			  = 100;
	constexpr auto MOVING_LASER_VELOCITY_MIN	  = 1.0f;
	constexpr auto MOVING_LASER_ACCEL			  = 1.0f;
	constexpr auto MOVING_LASER_PAUSE_FRAME_COUNT = 30;

	enum class MovingLaserProperty
	{
		Velocity,
		PauseTimer,
		DirectionSign,
		DistanceTraveled,
		VelocityCalc
	};

	void InitializeMovingLaser(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		item.ItemFlags[(int)MovingLaserProperty::DirectionSign] = 1;
		item.ItemFlags[(int)MovingLaserProperty::Velocity] = 10;

		// Offset by 1/4 block to make it dangerous at sector edges.
		item.Pose.Translate(item.Pose.Orientation, -BLOCK(0.25f));
	}

	void ControlMovingLaser(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TriggerActive(&item))
			return;

		// Calculate distances.
		float moveDist = BLOCK(item.TriggerFlags) + BLOCK(0.5f);
		float distPerFrame = (BLOCK(item.ItemFlags[(int)MovingLaserProperty::Velocity]) * 0.25f) / (float)FPS;

		item.Animation.ActiveState = 0;

		// TODO: Use SpawnDynamicPointLight().
		SpawnDynamicLight(
			item.Pose.Position.x, item.Pose.Position.y - 64, item.Pose.Position.z, (Random::GenerateInt() % 2) + 8,
			(Random::GenerateInt() % 4) + 24, Random::GenerateInt() % 4, Random::GenerateInt() % 2);
		/*auto lightPos = item.Pose.Position.ToVector3() + Vector3(0.0f, -64, 0.0f);
		auto lightColor = Color(Random::GenerateFloat(0.1f, 0.2f), Random::GenerateFloat(0.0f, 0.01f), Random::GenerateFloat(Random::GenerateFloat(0.0f, 0.01f)));
		float lightFalloff = ??
		SpawnDynamicPointLight(lightPos, lightPos, lightFalloff);*/

		// TODO: Demagic.
		// Used for flicker.
		item.MeshBits = -1 - (GetRandomControl() & 20);

		if (item.TriggerFlags == 0)
		{
			AnimateItem(item);
			return;
		}

		if (item.ItemFlags[(int)MovingLaserProperty::PauseTimer] > 0)
		{
			item.ItemFlags[(int)MovingLaserProperty::PauseTimer]--;
			if (item.ItemFlags[(int)MovingLaserProperty::PauseTimer] == 0)
			{
				item.ItemFlags[(int)MovingLaserProperty::DirectionSign] *= -1;
				item.ItemFlags[(int)MovingLaserProperty::DistanceTraveled] = 0;
			}

			AnimateItem(item);
			return;
		}

		item.Pose.Translate(item.Pose.Orientation, (item.ItemFlags[(int)MovingLaserProperty::DirectionSign] * item.ItemFlags[(int)MovingLaserProperty::VelocityCalc]));

		item.ItemFlags[(int)MovingLaserProperty::DistanceTraveled] += item.ItemFlags[(int)MovingLaserProperty::VelocityCalc];

		if (item.ItemFlags[(int)MovingLaserProperty::DistanceTraveled] < (moveDist - BLOCK(0.5f)))
		{
			item.ItemFlags[(int)MovingLaserProperty::VelocityCalc] = std::min(distPerFrame, item.ItemFlags[(int)MovingLaserProperty::VelocityCalc] + MOVING_LASER_ACCEL);
		}
		else
		{
			item.ItemFlags[(int)MovingLaserProperty::VelocityCalc] = std::max(MOVING_LASER_VELOCITY_MIN, item.ItemFlags[(int)MovingLaserProperty::VelocityCalc] - MOVING_LASER_ACCEL);
		}

		if (item.ItemFlags[(int)MovingLaserProperty::DistanceTraveled] >= moveDist)
		{
			item.ItemFlags[(int)MovingLaserProperty::PauseTimer] = MOVING_LASER_PAUSE_FRAME_COUNT;
		}

		if (item.ItemFlags[(int)MovingLaserProperty::PauseTimer] == 0)
		{
			SoundEffect(SFX_TR5_MOVING_LASER_LOOP, &item.Pose, SoundEnvironment::Always);
		}

		// Update room if necessary.
		int roomNumber = GetPointCollision(item).GetRoomNumber();
		if (roomNumber != item.RoomNumber)
			ItemNewRoom(itemNumber, roomNumber);

		AnimateItem(item);
	}

	void CollideMovingLaser(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		auto& item = g_Level.Items[itemNumber];

		// Collide with objects.
		if (item.Status == ITEM_ACTIVE)
		{
			if (!TestBoundsCollide(&item, playerItem, coll->Setup.Radius))
				return;

			HandleItemSphereCollision(item, *playerItem);
		}
		else if (item.Status != ITEM_INVISIBLE)
		{
			ObjectCollision(itemNumber, playerItem, coll);
		}
		
		// Damage player.
		if (TestBoundsCollide(&item, playerItem, coll->Setup.Radius))
		{
			DoDamage(playerItem, MOVING_LASER_DAMAGE);
			DoLotsOfBlood(playerItem->Pose.Position.x, playerItem->Pose.Position.y + CLICK(3), playerItem->Pose.Position.z, 4, playerItem->Pose.Orientation.y, playerItem->RoomNumber, 3);
			playerItem->TouchBits.ClearAll();
		}
	}
}
