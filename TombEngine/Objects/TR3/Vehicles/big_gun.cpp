#include "framework.h"
#include "Objects/TR3/Vehicles/big_gun.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/control/box.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_flare.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_struct.h"
#include "Game/Setup.h"
#include "Objects/TR3/Vehicles/big_gun_info.h"
#include "Objects/Utils/VehicleHelpers.h"
#include "Sound/sound.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Input;

namespace TEN::Entities::Vehicles
{
	constexpr auto BGUN_ROCKET_SPAWN_DISTANCE = CLICK(1);
	constexpr auto BGUN_MOUNT_DISTANCE		  = CLICK(2);

	constexpr auto BGUN_RECOIL_TIME		= 26;
	constexpr auto BGUN_RECOIL_Z		= 25;
	constexpr auto BGUN_ROCKET_VELOCITY = 16;
	constexpr auto BGUN_ROCKET_TIMER	= 1000;
	constexpr auto BGUN_SMOKE_DURATION	= 32;

	constexpr auto BGUN_X_ORIENT_NUM_FRAMES	  = 59;
	constexpr auto BGUN_X_ORIENT_MIDDLE_FRAME = 30;

	constexpr auto BGUN_TURN_RATE_ACCEL = ANGLE(0.5f);
	constexpr auto BGUN_TURN_RATE_MAX	= ANGLE(4.0f);
	constexpr auto BGUN_X_ORIENT_STEP	= ANGLE(80.0f) / BGUN_X_ORIENT_NUM_FRAMES;
	constexpr auto BGUN_X_ORIENT_MAX	= ANGLE(40.0f);

	const std::vector<VehicleMountType> BigGunMountTypes =
	{
		VehicleMountType::LevelStart,
		VehicleMountType::Back
	};

	const auto BigGunBite = CreatureBiteInfo(Vector3(0, 0, BGUN_ROCKET_SPAWN_DISTANCE), 2);

	enum BigGunState
	{
		BGUN_STATE_MOUNT = 0,
		BGUN_STATE_DISMOUNT = 1,
		BGUN_STATE_ROTATE_VERTICALLY = 2,
		BGUN_STATE_RECOIL = 3
	};

	enum BigGunAnim
	{
		BGUN_ANIM_MOUNT = 0,
		BGUN_ANIM_DISMOUNT = 1,
		BGUN_ANIM_ROTATE_VERTICALLY = 2,
		BGUN_ANIM_RECOIL = 3
	};

	enum BigGunFlags
	{
		BGUN_FLAG_UP_DOWN = (1 << 0),
		BGUN_FLAG_AUTO_ROT = (1 << 2),
		BGUN_FLAG_DISMOUNT = (1 << 3),
		BGUN_FLAG_FIRE = (1 << 4)
	};

	BigGunInfo* GetBigGunInfo(ItemInfo* bigGunItem)
	{
		return (BigGunInfo*)bigGunItem->Data;
	}

	void BigGunInitialize(short itemNumber)
	{
		auto* bigGunItem = &g_Level.Items[itemNumber];
		bigGunItem->Data = BigGunInfo();

		auto* bigGun = GetBigGunInfo(bigGunItem);
		bigGun->BaseOrientation = bigGunItem->Pose.Orientation;
		bigGun->XOrientFrame = BGUN_X_ORIENT_MIDDLE_FRAME;
	}

	static bool BigGunTestMount(ItemInfo* bigGunItem, ItemInfo* laraItem)
	{
		// TODO: If Lara global is not used, the game crashes upon level load. Not sure why. @Sezz 2022.01.09
		auto* lara = &Lara/* GetLaraInfo(laraItem)*/;

		if (!IsHeld(In::Action) ||
			lara->Control.HandStatus != HandStatus::Free ||
			laraItem->Animation.IsAirborne)
		{
			return false;
		}

		int x = laraItem->Pose.Position.x - bigGunItem->Pose.Position.x;
		int y = laraItem->Pose.Position.y - bigGunItem->Pose.Position.y;
		int z = laraItem->Pose.Position.z - bigGunItem->Pose.Position.z;

		int distance = SQUARE(x) + SQUARE(y) + SQUARE(z);
		if (distance > BLOCK(30))
			return false;

		short deltaAngle = abs(laraItem->Pose.Orientation.y - bigGunItem->Pose.Orientation.y);
		if (deltaAngle > ANGLE(35.0f) || deltaAngle < -ANGLE(35.0f))
			return false;

		return true;
	}

	void BigGunFire(ItemInfo* bigGunItem, ItemInfo* laraItem)
	{
		short itemNumber = CreateItem();
		if (itemNumber == NO_VALUE)
			return;
		auto* lara = GetLaraInfo(laraItem);
		auto* bigGun = GetBigGunInfo(bigGunItem);

		auto* projectileItem = &g_Level.Items[itemNumber];
		projectileItem->ObjectNumber = ID_ROCKET;
		auto pos = GetJointPosition(bigGunItem, BigGunBite);
		auto pointColl = GetPointCollision(pos, bigGunItem->RoomNumber);
		projectileItem->RoomNumber = pointColl.GetRoomNumber();
		projectileItem->Pose.Position = pos;
		projectileItem->Pose.Orientation = EulerAngles(
			-((bigGun->XOrientFrame - 32) * ANGLE(1.0f)),
			bigGunItem->Pose.Orientation.y,
			0
		);
		InitializeItem(itemNumber);

		projectileItem->Animation.Velocity.z = BGUN_ROCKET_VELOCITY;
		projectileItem->HitPoints = BGUN_ROCKET_TIMER; // NOTE: Time before it explode, TR5 use it, if 0, it will explode by default.

		AddActiveItem(itemNumber);

		lara->LeftArm.GunSmoke = BGUN_SMOKE_DURATION;
		for (int i = 0; i < 5; i++)
			TriggerGunSmoke(pos.x, pos.y, pos.z, 0, 0, 0, 1, LaraWeaponType::RocketLauncher, lara->LeftArm.GunSmoke);

		SoundEffect(SFX_TR4_EXPLOSION1, &projectileItem->Pose);
	}

	void BigGunCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* bigGunItem = &g_Level.Items[itemNumber];
		auto* bigGun = GetBigGunInfo(bigGunItem);
		auto* lara = GetLaraInfo(laraItem);

		if (laraItem->HitPoints <= 0 || lara->Context.Vehicle != NO_VALUE)
			return;

		if (BigGunTestMount(laraItem, bigGunItem))
		{
			SetLaraVehicle(laraItem, bigGunItem);

			DoVehicleFlareDiscard(laraItem);
			SetAnimation(*laraItem, ID_LARA_BIGGUN_ANIM, BGUN_ANIM_MOUNT);
			laraItem->Animation.IsAirborne = false;
			laraItem->Pose = bigGunItem->Pose;
			lara->Control.HandStatus = HandStatus::Busy;
			bigGunItem->HitPoints = 1;
			bigGun->XOrientFrame = BGUN_X_ORIENT_MIDDLE_FRAME;
			bigGun->Flags = 0;
		}
		else
			ObjectCollision(itemNumber, laraItem, coll);
	}

	bool BigGunControl(ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* lara = GetLaraInfo(laraItem);
		auto* bigGunItem = &g_Level.Items[lara->Context.Vehicle];
		auto* bigGun = GetBigGunInfo(bigGunItem);

		if (bigGun->Flags & BGUN_FLAG_UP_DOWN)
		{
			if (bigGun->IsBarrelRotating)
				bigGun->BarrelRotation--;

			if (!bigGun->BarrelRotation)
				bigGun->IsBarrelRotating = false;

			if (IsHeld(In::Brake) || laraItem->HitPoints <= 0)
				bigGun->Flags = BGUN_FLAG_AUTO_ROT;
			else
			{
				if ((IsHeld(In::Accelerate) || IsHeld(In::Fire)) && !bigGun->FireCount)
				{
					BigGunFire(bigGunItem, laraItem);
					bigGun->FireCount = BGUN_RECOIL_TIME;
					bigGun->BarrelRotation = BGUN_RECOIL_Z;
					bigGun->IsBarrelRotating = true;
				}

				if (IsHeld(In::Forward))
				{
					if (bigGun->TurnRate.x < 0)
						bigGun->TurnRate.x /= 2;

					bigGun->TurnRate.x += BGUN_TURN_RATE_ACCEL;
					if (bigGun->TurnRate.x > (BGUN_TURN_RATE_MAX / 2))
						bigGun->TurnRate.x = (BGUN_TURN_RATE_MAX / 2);
				}
				else if (IsHeld(In::Back))
				{
					if (bigGun->TurnRate.x > 0)
						bigGun->TurnRate.x /= 2;

					bigGun->TurnRate.x -= BGUN_TURN_RATE_ACCEL;
					if (bigGun->TurnRate.x < (-BGUN_TURN_RATE_MAX / 2))
						bigGun->TurnRate.x = (-BGUN_TURN_RATE_MAX / 2);
				}
				else
				{
					bigGun->TurnRate.x -= bigGun->TurnRate.x / 3;
					if (abs(bigGun->TurnRate.x) < BGUN_TURN_RATE_ACCEL)
						bigGun->TurnRate.x = 0;
				}

				if (IsHeld(In::Left))
				{
					if (bigGun->TurnRate.y > 0)
						bigGun->TurnRate.y /= 2;

					bigGun->TurnRate.y -= BGUN_TURN_RATE_ACCEL;
					if (bigGun->TurnRate.y < -BGUN_TURN_RATE_MAX)
						bigGun->TurnRate.y = -BGUN_TURN_RATE_MAX;
				}
				else if (IsHeld(In::Right))
				{
					if (bigGun->TurnRate.y < 0)
						bigGun->TurnRate.y /= 2;

					bigGun->TurnRate.y += BGUN_TURN_RATE_ACCEL;
					if (bigGun->TurnRate.y > BGUN_TURN_RATE_MAX)
						bigGun->TurnRate.y = BGUN_TURN_RATE_MAX;
				}
				else
				{
					bigGun->TurnRate.y -= bigGun->TurnRate.y / 3;
					if (abs(bigGun->TurnRate.y) < BGUN_TURN_RATE_ACCEL)
						bigGun->TurnRate.y = 0;
				}

				bigGun->Rotation.x += bigGun->TurnRate.x;
				bigGun->Rotation.y += bigGun->TurnRate.y;

				if (bigGun->Rotation.x > BGUN_X_ORIENT_MAX)
					bigGun->Rotation.x = BGUN_X_ORIENT_MAX;
				else if (bigGun->Rotation.x < -BGUN_X_ORIENT_MAX)
					bigGun->Rotation.x = -BGUN_X_ORIENT_MAX;

				bigGun->XOrientFrame = (int)round((bigGun->Rotation.x + BGUN_X_ORIENT_MAX) / BGUN_X_ORIENT_STEP);
			}
		}

		if (bigGun->Flags & BGUN_FLAG_AUTO_ROT)
		{
			if (bigGun->XOrientFrame == BGUN_X_ORIENT_MIDDLE_FRAME)
			{
				SetAnimation(*laraItem, ID_LARA_BIGGUN_ANIM, BGUN_ANIM_DISMOUNT);
				bigGun->TurnRate.y = 0;
				bigGun->IsBarrelRotating = false;
				bigGun->Flags = BGUN_FLAG_DISMOUNT;
			}
			else if (bigGun->Rotation.x > 0)
				bigGun->Rotation.x -= BGUN_X_ORIENT_STEP;
			else if (bigGun->Rotation.x < 0)
				bigGun->Rotation.x += BGUN_X_ORIENT_STEP;

			bigGun->XOrientFrame = (int)round((bigGun->Rotation.x + BGUN_X_ORIENT_MAX) / BGUN_X_ORIENT_STEP);
		}

		switch (laraItem->Animation.ActiveState)
		{
		case BGUN_STATE_MOUNT:
		case BGUN_STATE_DISMOUNT:
			AnimateItem(*laraItem);
			SyncVehicleAnim(*bigGunItem, *laraItem);

			if (bigGun->Flags & BGUN_FLAG_DISMOUNT && TestLastFrame(*laraItem))
			{
				SetAnimation(*laraItem, LA_STAND_IDLE);
				SetLaraVehicle(laraItem, nullptr);
				lara->Control.HandStatus = HandStatus::Free;
				bigGunItem->HitPoints = 0;
			}

			break;

		case BGUN_STATE_ROTATE_VERTICALLY:
			SyncVehicleAnim(*bigGunItem, *laraItem);

			if (bigGun->FireCount > 0)
				bigGun->FireCount--;
			else
				bigGun->FireCount = 0;

			bigGun->Flags = BGUN_FLAG_UP_DOWN;

			SetAnimation(*laraItem, ID_LARA_BIGGUN_ANIM, BGUN_ANIM_ROTATE_VERTICALLY, bigGun->XOrientFrame);
			break;
		}

		Camera.targetElevation = -(bigGun->Rotation.x + ANGLE(15.0f));

		bigGunItem->Pose.Orientation.y = bigGun->BaseOrientation.y + bigGun->Rotation.y;
		laraItem->Pose.Orientation.y = bigGunItem->Pose.Orientation.y;
		coll->Setup.EnableObjectPush = false;
		coll->Setup.EnableSpasm = false;

		DoObjectCollision(laraItem, coll);

		return true;
	}
}
