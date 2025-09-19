#include "framework.h"
#include "Objects/TR3/Trap/train.h"

#include "Game/animation.h"
#include "Game/camera.h"
#include "Game/control/control.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/Point.h"
#include "Game/collision/floordata.h"
#include "Game/collision/Sphere.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Setup.h"
#include "Objects/Utils/VehicleHelpers.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Collision::Point;
using namespace TEN::Collision::Sphere;

namespace TEN::Entities::Traps
{
	constexpr auto TRAIN_VEL = 260;

	long TrainTestHeight(ItemInfo* item, long x, long z, short* roomNumber)
	{
		float sinX = phd_sin(item->Pose.Orientation.x);
		float sinY = phd_sin(item->Pose.Orientation.y);
		float cosY = phd_cos(item->Pose.Orientation.y);
		float sinZ = phd_sin(item->Pose.Orientation.z);

		auto pos = Vector3i(
			round(item->Pose.Position.x + ((z * sinY) + (x * cosY))),
			round(item->Pose.Position.y - ((z * sinX) + (x * sinZ))),
			round(item->Pose.Position.z + ((z * cosY) - (x * sinY))));
		auto pointColl = GetPointCollision(pos, item->RoomNumber);

		*roomNumber = pointColl.GetRoomNumber();
		return pointColl.GetFloorHeight();
	}

	void ControlTrain(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TriggerActive(&item))
			return;

		if (item.ItemFlags[0] == 0)
			item.ItemFlags[0] = item.ItemFlags[1] = TRAIN_VEL;

		float sinY = phd_sin(item.Pose.Orientation.y);
		float cosY = phd_cos(item.Pose.Orientation.y);

		item.Pose.Position.x += item.ItemFlags[1] * sinY;
		item.Pose.Position.z += item.ItemFlags[1] * cosY;

		short roomNumber;
		long floorHeight = TrainTestHeight(&item, 0, 0, &roomNumber);
		item.Pose.Position.y = floorHeight;

		if (floorHeight == NO_HEIGHT)
		{
			KillItem(itemNumber);
			return;
		}

		auto depth = GameBoundingBox(&item).GetDepth() / 2;
		auto floorNormal = GetPointCollision(item, item.Pose.Orientation.ToDirection(), depth).GetFloorNormal();
		auto localNormal = Vector3::TransformNormal(floorNormal, Matrix::CreateRotationY(-TO_RAD(item.Pose.Orientation.y)));

		auto targetOrient = item.Pose.Orientation;
		targetOrient.x = FROM_RAD(atan2(localNormal.z, localNormal.y)) + ANGLE(180);

		item.Pose.Orientation.InterpolateConstant(targetOrient, ANGLE(1));

		UpdateVehicleRoom(&item);

		depth += CLICK(1);
		SpawnDynamicLight(item.Pose.Position.x + depth * sinY, item.Pose.Position.y - CLICK(0.25f), item.Pose.Position.z + depth * cosY, 16, 64, 64, 64);

		if (item.ItemFlags[1] < item.ItemFlags[0])
		{
			item.ItemFlags[1] -= 48;
			if (item.ItemFlags[1] < 0)
				item.ItemFlags[1] = 0;

			if (!UseForcedFixedCamera)
			{
				ForcedFixedCamera.x = item.Pose.Position.x + BLOCK(8) * sinY;
				ForcedFixedCamera.z = item.Pose.Position.z + BLOCK(8) * cosY;

				ForcedFixedCamera.y = GetPointCollision(Vector3i(ForcedFixedCamera.x, item.Pose.Position.y - CLICK(2), ForcedFixedCamera.z), item.RoomNumber).GetFloorHeight();

				ForcedFixedCamera.RoomNumber = roomNumber;
				UseForcedFixedCamera = true;
			}

			StopSoundEffect(SFX_TR3_TUBE_LOOP);
		}
		else
		{
			SoundEffect(SFX_TR3_TUBE_LOOP, &item.Pose, SoundEnvironment::Always);
		}
	}

	void CollideTrain(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		auto& item = g_Level.Items[itemNumber];
		auto& player = GetLaraInfo(*playerItem);

		if (!TestBoundsCollide(&item, playerItem, coll->Setup.Radius))
			return;

		if (!HandleItemSphereCollision(item, *playerItem))
			return;

		if (item.ItemFlags[1] < item.ItemFlags[0])
		{
			ObjectCollision(itemNumber, playerItem, coll);
			return;
		}

		SoundEffect(SFX_TR4_LARA_GENERAL_DEATH, &playerItem->Pose, SoundEnvironment::Always);
		SoundEffect(SFX_TR4_LARA_HIGH_FALL_DEATH, &playerItem->Pose, SoundEnvironment::Always);

		SetAnimation(*playerItem, ID_LARA_EXTRA_ANIMS, LEA_TRAIN_DEATH_START);
		playerItem->Animation.IsAirborne = false;
		playerItem->Animation.Velocity.y = 0.0f;
		playerItem->Animation.Velocity.z = 0.0f;
		playerItem->Pose.Orientation.y = item.Pose.Orientation.y;

		DoDamage(playerItem, INT_MAX);

		AnimateItem(playerItem);

		player.ExtraAnim = 1;
		player.Control.HandStatus = HandStatus::Busy;
		player.Control.Weapon.GunType = LaraWeaponType::None;
		player.HitDirection = NO_VALUE;
		player.Status.Air = NO_VALUE;

		item.ItemFlags[1] = 160;

		float sinY = phd_sin(item.Pose.Orientation.y);
		float cosY = phd_cos(item.Pose.Orientation.y);

		long x = playerItem->Pose.Position.x + CLICK(1) * sinY;
		long z = playerItem->Pose.Position.z + CLICK(1) * cosY;

		DoLotsOfBlood(x, playerItem->Pose.Position.y - CLICK(2), z, BLOCK(1), item.Pose.Orientation.y, playerItem->RoomNumber, 15);
	}
}
