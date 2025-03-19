#include "framework.h"
#include "Objects/TR2/Vehicles/skidoo.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/Point.h"
#include "Game/effects/effects.h"
#include "Game/effects/simple_particle.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_fire.h"
#include "Game/Lara/lara_flare.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_one_gun.h"
#include "Game/Setup.h"
#include "Objects/TR2/Vehicles/skidoo_info.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"
#include "Math/Math.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Sound/sound.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Input;
using namespace TEN::Math;

namespace TEN::Entities::Vehicles
{
	constexpr auto SKIDOO_RADIUS = 500;
	constexpr auto SKIDOO_FRONT = 550;
	constexpr auto SKIDOO_SIDE = 260;
	constexpr auto SKIDOO_SLIP = 100;
	constexpr auto SKIDOO_SLIP_SIDE = 50;
	constexpr auto SKIDOO_SNOW = 500; // Unused.
	constexpr auto SKIDOO_MOUNT_DISTANCE = CLICK(2);
	constexpr auto SKIDOO_DISMOUNT_DISTANCE = 295;

	constexpr auto SKIDOO_VELOCITY_ACCEL = 10;
	constexpr auto SKIDOO_VELOCITY_DECEL = 2;
	constexpr auto SKIDOO_VELOCITY_BRAKE_DECEL = 5;
	constexpr auto SKIDOO_REVERSE_VELOCITY_ACCEL = 5;
	constexpr auto SKIDOO_KICK_MAX = -80;

	constexpr auto SKIDOO_SLOW_VELOCITY_MAX = 50;
	constexpr auto SKIDOO_NORMAL_VELOCITY_MAX = 100;
	constexpr auto SKIDOO_FAST_VELOCITY_MAX = 150;
	constexpr auto SKIDOO_TURN_VELOCITY_MAX = 15;
	constexpr auto SKIDOO_REVERSE_VELOCITY_MAX = 30;

	constexpr auto SKIDOO_STEP_HEIGHT_MAX = CLICK(1); // Unused.
	constexpr auto SKIDOO_MIN_BOUNCE = (SKIDOO_NORMAL_VELOCITY_MAX / 2) / 256;

	constexpr auto SKIDOO_DAMAGE_START = 140;
	constexpr auto SKIDOO_DAMAGE_LENGTH = 14;

	constexpr auto SKIDOO_WAKE_OFFSET = Vector3(SKIDOO_SIDE, 0, SKIDOO_FRONT / 2);

	#define SKIDOO_TURN_RATE_ACCEL			ANGLE(2.5f)
	#define SKIDOO_TURN_RATE_DECEL			ANGLE(2.0f)
	#define SKIDOO_TURN_RATE_MAX			ANGLE(6.0f)
	#define SKIDOO_MOMENTUM_TURN_RATE_ACCEL	ANGLE(3.0f)
	#define SKIDOO_MOMENTUM_TURN_RATE_MAX	ANGLE(150.0f)

	const std::vector<VehicleMountType> SkidooMountTypes =
	{
		VehicleMountType::LevelStart,
		VehicleMountType::Left,
		VehicleMountType::Right
	};

	enum SkidooState
	{
		SKIDOO_STATE_DRIVE = 0,
		SKIDOO_STATE_MOUNT_RIGHT = 1,
		SKIDOO_STATE_LEFT = 2,
		SKIDOO_STATE_RIGHT = 3,
		SKIDOO_STATE_FALL = 4,
		SKIDOO_STATE_HIT = 5,
		SKIDOO_STATE_MOUNT_LEFT = 6,
		SKIDOO_STATE_DISMOUNT_LEFT = 7,
		SKIDOO_STATE_IDLE = 8,
		SKIDOO_STATE_DISMOUNT_RIGHT = 9,
		SKIDOO_STATE_JUMP_OFF = 10,
		SKIDOO_STATE_DEATH = 11,
		SKIDOO_STATE_FALLOFF = 12
	};

	enum SkidooAnim
	{
		SKIDOO_ANIM_DRIVE = 0,
		SKIDOO_ANIM_MOUNT_RIGHT = 1,
		SKIDOO_ANIM_TURN_LEFT_START = 2,
		SKIDOO_ANIM_TURN_LEFT_CONTINUE = 3,
		SKIDOO_ANIM_TURN_LEFT_END = 4,
		SKIDOO_ANIM_TURN_RIGHT_START = 5,
		SKIDOO_ANIM_TURN_RIGHT_CONTINUE = 6,
		SKIDOO_ANIM_TURN_RIGHT_END = 7,
		SKIDOO_ANIM_LEAP_START = 8,
		SKIDOO_ANIM_LEAP_END = 9,
		SKIDOO_ANIM_LEAP_CONTINUE = 10,
		SKIDOO_ANIM_HIT_LEFT = 11,
		SKIDOO_ANIM_HIT_RIGHT = 12,
		SKIDOO_ANIM_HIT_FRONT = 13,
		SKIDOO_ANIM_HIT_BACK = 14,
		SKIDOO_ANIM_IDLE = 15,
		SKIDOO_ANIM_DISMOUNT_RIGHT = 16,
		SKIDOO_ANIM_UNK = 17, // TODO
		SKIDOO_ANIM_MOUNT_LEFT = 18,
		SKIDOO_ANIM_DISMOUNT_LEFT = 19,
		SKIDOO_ANIM_FALL_OFF = 20,
		SKIDOO_ANIM_IDLE_DEATH = 21,
		SKIDOO_ANIM_FALL_DEATH = 22
	};

	SkidooInfo* GetSkidooInfo(ItemInfo* skidooItem)
	{
		return (SkidooInfo*)skidooItem->Data;
	}

	void InitializeSkidoo(short itemNumber)
	{
		auto* skidooItem = &g_Level.Items[itemNumber];
		skidooItem->Data = SkidooInfo{};
		auto* skidoo = GetSkidooInfo(skidooItem);

		if (skidooItem->Status != ITEM_ACTIVE)
		{
			AddActiveItem(itemNumber);
			skidooItem->Status = ITEM_ACTIVE;
		}

		skidoo->MomentumAngle = skidooItem->Pose.Orientation.y;
	}

	void SkidooPlayerCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* skidooItem = &g_Level.Items[itemNumber];
		auto* lara = GetLaraInfo(laraItem);

		if (laraItem->HitPoints < 0 || lara->Context.Vehicle != NO_VALUE)
			return;

		auto mountType = GetVehicleMountType(skidooItem, laraItem, coll, SkidooMountTypes, SKIDOO_MOUNT_DISTANCE);
		if (mountType == VehicleMountType::None)
			ObjectCollision(itemNumber, laraItem, coll);
		else
		{
			SetLaraVehicle(laraItem, skidooItem);
			DoSkidooMount(skidooItem, laraItem, mountType);
		}
	}

	void DoSkidooMount(ItemInfo* skidooItem, ItemInfo* laraItem, VehicleMountType mountType)
	{
		auto* lara = GetLaraInfo(laraItem);

		switch (mountType)
		{
		case VehicleMountType::LevelStart:
			SetAnimation(*laraItem, ID_SNOWMOBILE_LARA_ANIMS, SKIDOO_ANIM_IDLE);
			break;

		case VehicleMountType::Left:
			SetAnimation(*laraItem, ID_SNOWMOBILE_LARA_ANIMS, SKIDOO_ANIM_MOUNT_LEFT);
			break;

		default:
		case VehicleMountType::Right:
			SetAnimation(*laraItem, ID_SNOWMOBILE_LARA_ANIMS, SKIDOO_ANIM_MOUNT_RIGHT);
			break;
		}

		DoVehicleFlareDiscard(laraItem);
		laraItem->Pose.Position = skidooItem->Pose.Position;
		laraItem->Pose.Orientation = EulerAngles(0, skidooItem->Pose.Orientation.y, 0);
		lara->Control.HandStatus = HandStatus::Busy;
		skidooItem->Collidable = true;
	}

	bool TestSkidooDismountOK(ItemInfo* skidooItem, int direction)
	{
		short angle;
		if (direction == SKIDOO_STATE_DISMOUNT_LEFT)
			angle = skidooItem->Pose.Orientation.y + ANGLE(90.0f);
		else
			angle = skidooItem->Pose.Orientation.y - ANGLE(90.0f);

		auto probe = GetPointCollision(*skidooItem, angle, -SKIDOO_DISMOUNT_DISTANCE);

		if ((probe.IsSteepFloor() || probe.GetFloorHeight() == NO_HEIGHT) ||
			abs(probe.GetFloorHeight() - skidooItem->Pose.Position.y) > CLICK(2) ||
			((probe.GetCeilingHeight() - skidooItem->Pose.Position.y) > -LARA_HEIGHT ||
				(probe.GetFloorHeight() - probe.GetCeilingHeight()) < LARA_HEIGHT))
		{
			return false;
		}

		return true;
	}

	bool TestSkidooDismount(ItemInfo* skidooItem, ItemInfo* laraItem)
	{
		auto* lara = GetLaraInfo(laraItem);
		auto* skidoo = GetSkidooInfo(skidooItem);

		if (lara->Context.Vehicle != NO_VALUE)
		{
			if ((laraItem->Animation.ActiveState == SKIDOO_STATE_DISMOUNT_RIGHT || laraItem->Animation.ActiveState == SKIDOO_STATE_DISMOUNT_LEFT) &&
				TestLastFrame(*laraItem))
			{
				if (laraItem->Animation.ActiveState == SKIDOO_STATE_DISMOUNT_LEFT)
					laraItem->Pose.Orientation.y += ANGLE(90.0f);
				else
					laraItem->Pose.Orientation.y -= ANGLE(90.0f);

				SetAnimation(*laraItem, LA_STAND_IDLE);
				laraItem->Pose.Translate(laraItem->Pose.Orientation.y, -SKIDOO_DISMOUNT_DISTANCE);
				laraItem->Pose.Orientation.x = 0;
				laraItem->Pose.Orientation.z = 0;
				lara->Control.HandStatus = HandStatus::Free;

				if (skidoo->Armed)
					lara->Control.Weapon.GunType = lara->Control.Weapon.LastGunType;

				SetLaraVehicle(laraItem, nullptr);
			}
			else if (laraItem->Animation.ActiveState == SKIDOO_STATE_JUMP_OFF &&
				(skidooItem->Pose.Position.y == skidooItem->Floor || TestLastFrame(*laraItem)))
			{
				SetAnimation(*laraItem, LA_FREEFALL);

				if (skidooItem->Pose.Position.y == skidooItem->Floor)
				{
					laraItem->Animation.TargetState = LS_DEATH;
					laraItem->Animation.Velocity.z = 0;
					laraItem->Animation.Velocity.y = SKIDOO_DAMAGE_START + SKIDOO_DAMAGE_LENGTH;
					ExplodeVehicle(laraItem, skidooItem);
				}
				else
				{
					laraItem->Animation.TargetState = LS_FREEFALL;
					laraItem->Pose.Position.y -= 200;
					laraItem->Animation.Velocity.z = skidooItem->Animation.Velocity.z;
					laraItem->Animation.Velocity.y = skidooItem->Animation.Velocity.y;
					SoundEffect(SFX_TR4_LARA_FALL, &laraItem->Pose);
				}

				laraItem->Pose.Orientation.x = 0;
				laraItem->Pose.Orientation.z = 0;
				laraItem->Animation.IsAirborne = true;
				lara->Control.MoveAngle = skidooItem->Pose.Orientation.y;
				lara->Control.HandStatus = HandStatus::Free;
				lara->Control.Weapon.GunType = lara->Control.Weapon.LastGunType;

				if (skidoo->Armed)
					lara->Control.Weapon.GunType = lara->Control.Weapon.LastGunType;

				skidooItem->Collidable = false;
				skidooItem->Flags |= IFLAG_INVISIBLE;

				return false;
			}

			return true;
		}
		else
			return true;
	}

	bool SkidooControl(ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* lara = GetLaraInfo(laraItem);
		auto* skidooItem = &g_Level.Items[lara->Context.Vehicle];
		auto* skidoo = GetSkidooInfo(skidooItem);

		Vector3i frontLeft, frontRight;
		auto collide = SkidooDynamics(skidooItem, laraItem);
		auto heightFrontLeft = GetVehicleHeight(skidooItem, SKIDOO_FRONT, -SKIDOO_SIDE, true, &frontLeft);
		auto heightFrontRight = GetVehicleHeight(skidooItem, SKIDOO_FRONT, SKIDOO_SIDE, true, &frontRight);

		auto probe = GetPointCollision(*skidooItem);

		TestTriggers(skidooItem, true);
		TestTriggers(skidooItem, false);

		bool dead = false;
		int drive = 0;

		if (laraItem->HitPoints <= 0)
		{
			dead = true;
			ClearAction(In::Forward);
			ClearAction(In::Back);
			ClearAction(In::Left);
			ClearAction(In::Right);
		}
		else if (laraItem->Animation.ActiveState == SKIDOO_STATE_JUMP_OFF)
		{
			dead = true;
			collide = 0;
		}

		int height = probe.GetFloorHeight();
		int pitch = 0;

		if (skidooItem->Flags & IFLAG_INVISIBLE)
		{
			drive = 0;
			collide = 0;
		}
		else
		{
			switch (laraItem->Animation.ActiveState)
			{
			case SKIDOO_STATE_MOUNT_RIGHT:
			case SKIDOO_STATE_MOUNT_LEFT:
			case SKIDOO_STATE_DISMOUNT_RIGHT:
			case SKIDOO_STATE_DISMOUNT_LEFT:
			case SKIDOO_STATE_JUMP_OFF:
				drive = -1;
				collide = 0;

				break;

			default:
				drive = SkidooUserControl(skidooItem, laraItem, height, &pitch);
				HandleVehicleSpeedometer(skidooItem->Animation.Velocity.z, SKIDOO_FAST_VELOCITY_MAX);
				break;
			}
		}

		bool banditSkidoo = skidoo->Armed;
		if (drive > 0)
		{
			skidoo->TrackMesh = ((skidoo->TrackMesh & 3) == 1) ? 2 : 1;
			skidoo->Pitch += (pitch - skidoo->Pitch) / 4;

			auto pitch = std::clamp(0.5f + (float)abs(skidoo->Pitch) / (float)SKIDOO_NORMAL_VELOCITY_MAX, 0.6f, 1.4f);
			SoundEffect(skidoo->Pitch ? SFX_TR2_VEHICLE_SNOWMOBILE_MOVING : SFX_TR2_VEHICLE_SNOWMOBILE_ACCELERATE, &skidooItem->Pose, SoundEnvironment::Land, pitch);
		}
		else
		{
			skidoo->TrackMesh = 0;
			if (!drive)
				SoundEffect(SFX_TR2_VEHICLE_SNOWMOBILE_IDLE, &skidooItem->Pose);
			skidoo->Pitch = 0;
		}
		skidooItem->Floor = height;

		skidoo->LeftVerticalVelocity = DoSkidooDynamics(heightFrontLeft, skidoo->LeftVerticalVelocity, (int*)&frontLeft.y);
		skidoo->RightVerticalVelocity = DoSkidooDynamics(heightFrontRight, skidoo->RightVerticalVelocity, (int*)&frontRight.y);
		skidooItem->Animation.Velocity.y = DoSkidooDynamics(height, skidooItem->Animation.Velocity.y, (int*)&skidooItem->Pose.Position.y);
		skidooItem->Animation.Velocity.z = DoVehicleWaterMovement(skidooItem, laraItem, skidooItem->Animation.Velocity.z, SKIDOO_RADIUS, &skidoo->TurnRate, SKIDOO_WAKE_OFFSET);

		height = (frontLeft.y + frontRight.y) / 2;
		short xRot = phd_atan(SKIDOO_FRONT, skidooItem->Pose.Position.y - height);
		short zRot = phd_atan(SKIDOO_SIDE, height - frontLeft.y);

		skidooItem->Pose.Orientation.x += (xRot - skidooItem->Pose.Orientation.x) / 2;
		skidooItem->Pose.Orientation.z += (zRot - skidooItem->Pose.Orientation.z) / 2;

		if (skidooItem->Flags & IFLAG_INVISIBLE)
		{
			if (probe.GetRoomNumber() != skidooItem->RoomNumber)
			{
				ItemNewRoom(lara->Context.Vehicle, probe.GetRoomNumber());
				ItemNewRoom(laraItem->Index, probe.GetRoomNumber());
			}

			AnimateItem(*laraItem);

			if (skidooItem->Pose.Position.y == skidooItem->Floor)
				ExplodeVehicle(laraItem, skidooItem);

			return false;
		}

		SkidooAnimation(skidooItem, laraItem, collide, dead);

		if (probe.GetRoomNumber() != skidooItem->RoomNumber)
		{
			ItemNewRoom(lara->Context.Vehicle, probe.GetRoomNumber());
			ItemNewRoom(laraItem->Index, probe.GetRoomNumber());
		}

		if (laraItem->Animation.ActiveState != SKIDOO_STATE_FALLOFF)
		{
			laraItem->Pose.Position = skidooItem->Pose.Position;
			laraItem->Pose.Orientation.y = skidooItem->Pose.Orientation.y;

			if (drive >= 0)
			{
				laraItem->Pose.Orientation.x = skidooItem->Pose.Orientation.x;
				laraItem->Pose.Orientation.z = skidooItem->Pose.Orientation.z;
			}
			else
				laraItem->Pose.Orientation.x = laraItem->Pose.Orientation.z = 0;
		}
		else
			laraItem->Pose.Orientation.x = laraItem->Pose.Orientation.z = 0;

		AnimateItem(*laraItem);

		if (!dead && drive >= 0 && banditSkidoo)
			SkidooGuns(skidooItem, laraItem);

		if (!dead)
			SyncVehicleAnim(*skidooItem, *laraItem);
		else
			SetAnimation(*skidooItem, SKIDOO_ANIM_IDLE);

		if (skidooItem->Animation.Velocity.z && skidooItem->Floor == skidooItem->Pose.Position.y)
		{
			DoSnowEffect(skidooItem);

			if (skidooItem->Animation.Velocity.z < 50)
				DoSnowEffect(skidooItem);
		}

		return TestSkidooDismount(skidooItem, laraItem);
	}

	bool SkidooUserControl(ItemInfo* skidooItem, ItemInfo* laraItem, int height, int* pitch)
	{
		auto* skidoo = GetSkidooInfo(skidooItem);
		auto* lara = GetLaraInfo(laraItem);

		int maxVelocity = 0;
		bool drive = false;

		if (skidooItem->Pose.Position.y >= (height - CLICK(1)))
		{
			*pitch = skidooItem->Animation.Velocity.z + (height - skidooItem->Pose.Position.y);

			lara->Control.Look.Mode = (skidooItem->Animation.Velocity.z == 0.0f) ? LookMode::Horizontal : LookMode::Free;

			if (IsHeld(In::Left) || IsHeld(In::Right))
				ModulateVehicleTurnRateY(&skidoo->TurnRate, SKIDOO_TURN_RATE_ACCEL, -SKIDOO_TURN_RATE_MAX, SKIDOO_TURN_RATE_MAX);

			if (IsHeld(In::Reverse))
			{
				if (skidooItem->Animation.Velocity.z > 0)
					skidooItem->Animation.Velocity.z -= SKIDOO_VELOCITY_BRAKE_DECEL;
				else
				{
					if (skidooItem->Animation.Velocity.z > -SKIDOO_REVERSE_VELOCITY_MAX)
						skidooItem->Animation.Velocity.z -= SKIDOO_REVERSE_VELOCITY_ACCEL;

					drive = true;
				}
			}
			else if (IsHeld(In::Accelerate))
			{
				if (IsHeld(In::Faster))
					maxVelocity = SKIDOO_FAST_VELOCITY_MAX;
				else if (IsHeld(In::Slower))
					maxVelocity = SKIDOO_SLOW_VELOCITY_MAX;
				else
					maxVelocity = SKIDOO_NORMAL_VELOCITY_MAX;

				if (skidooItem->Animation.Velocity.z < maxVelocity)
					skidooItem->Animation.Velocity.z += (SKIDOO_VELOCITY_ACCEL / 2) + (SKIDOO_VELOCITY_ACCEL * (skidooItem->Animation.Velocity.z / (2 * maxVelocity)));
				else if (skidooItem->Animation.Velocity.z > (maxVelocity + SKIDOO_VELOCITY_DECEL))
					skidooItem->Animation.Velocity.z -= SKIDOO_VELOCITY_DECEL;

				drive = true;
			}
			else if (IsHeld(In::Left) || IsHeld(In::Right) &&
				skidooItem->Animation.Velocity.z >= 0 &&
				skidooItem->Animation.Velocity.z < SKIDOO_TURN_VELOCITY_MAX)
			{
				skidooItem->Animation.Velocity.z = SKIDOO_TURN_VELOCITY_MAX;
				drive = true;
			}
			else if (skidooItem->Animation.Velocity.z > SKIDOO_VELOCITY_DECEL)
			{
				skidooItem->Animation.Velocity.z -= SKIDOO_VELOCITY_DECEL;

				if ((GetRandomControl() & 0x7f) < skidooItem->Animation.Velocity.z)
					drive = true;
			}
			else
				skidooItem->Animation.Velocity.z = 0;
		}
		else if (IsHeld(In::Accelerate) || IsHeld(In::Reverse))
		{
			*pitch = skidoo->Pitch + 50;
			drive = true;
		}

		return drive;
	}

	void SkidooAnimation(ItemInfo* skidooItem, ItemInfo* laraItem, int collide, bool dead)
	{
		auto* skidoo = GetSkidooInfo(skidooItem);

		if (laraItem->Animation.ActiveState != SKIDOO_STATE_FALL &&
			skidooItem->Animation.Velocity.y > 0 &&
			skidooItem->Pose.Position.y != skidooItem->Floor &&
			!dead)
		{
			SetAnimation(*laraItem, ID_SNOWMOBILE_LARA_ANIMS, SKIDOO_ANIM_LEAP_START);
		}
		else if (laraItem->Animation.ActiveState != SKIDOO_STATE_FALL &&
			collide && !dead)
		{
			if (laraItem->Animation.ActiveState != SKIDOO_STATE_HIT)
			{
				if (collide == SKIDOO_ANIM_HIT_FRONT)
					SoundEffect(SFX_TR2_VEHICLE_IMPACT1, &skidooItem->Pose);
				else
					SoundEffect(SFX_TR2_VEHICLE_IMPACT2, &skidooItem->Pose);

				SetAnimation(*laraItem, ID_SNOWMOBILE_LARA_ANIMS, collide);
			}
		}
		else
		{
			switch (laraItem->Animation.ActiveState)
			{
			case SKIDOO_STATE_IDLE:

				if (dead)
				{
					laraItem->Animation.TargetState = SKIDOO_STATE_DEATH;
					break;
				}

				laraItem->Animation.TargetState = SKIDOO_STATE_IDLE;

				if (IsHeld(In::Brake))
				{
					if (IsHeld(In::Right) &&
						TestSkidooDismountOK(skidooItem, SKIDOO_STATE_DISMOUNT_RIGHT))
					{
						laraItem->Animation.TargetState = SKIDOO_STATE_DISMOUNT_RIGHT;
						skidooItem->Animation.Velocity.z = 0;
					}
					else if (IsHeld(In::Left) &&
						TestSkidooDismountOK(skidooItem, SKIDOO_STATE_DISMOUNT_LEFT))
					{
						laraItem->Animation.TargetState = SKIDOO_STATE_DISMOUNT_LEFT;
						skidooItem->Animation.Velocity.z = 0;
					}
				}
				else if (IsHeld(In::Left))
				{
					if (skidooItem->Animation.Velocity.z >= 0)
						laraItem->Animation.TargetState = SKIDOO_STATE_LEFT;
					else
						laraItem->Animation.TargetState = SKIDOO_STATE_RIGHT;
				}
				else if (IsHeld(In::Right))
				{
					if (skidooItem->Animation.Velocity.z >= 0)
						laraItem->Animation.TargetState = SKIDOO_STATE_RIGHT;
					else
						laraItem->Animation.TargetState = SKIDOO_STATE_LEFT;
				}
				else if (IsHeld(In::Accelerate) || IsHeld(In::Reverse))
					laraItem->Animation.TargetState = SKIDOO_STATE_DRIVE;

				break;

			case SKIDOO_STATE_DRIVE:
				if (skidooItem->Animation.Velocity.z == 0)
					laraItem->Animation.TargetState = SKIDOO_STATE_IDLE;

				if (dead)
					laraItem->Animation.TargetState = SKIDOO_STATE_FALLOFF;
				else if (IsHeld(In::Left))
				{
					if (skidooItem->Animation.Velocity.z >= 0)
						laraItem->Animation.TargetState = SKIDOO_STATE_LEFT;
					else
						laraItem->Animation.TargetState = SKIDOO_STATE_RIGHT;
				}
				else if (IsHeld(In::Right))
				{
					if (skidooItem->Animation.Velocity.z >= 0)
						laraItem->Animation.TargetState = SKIDOO_STATE_RIGHT;
					else
						laraItem->Animation.TargetState = SKIDOO_STATE_LEFT;
				}

				break;

			case SKIDOO_STATE_LEFT:
				if (skidooItem->Animation.Velocity.z >= 0)
				{
					if (!IsHeld(In::Left))
						laraItem->Animation.TargetState = SKIDOO_STATE_DRIVE;
				}
				else
				{
					if (!IsHeld(In::Right))
						laraItem->Animation.TargetState = SKIDOO_STATE_DRIVE;
				}

				break;

			case SKIDOO_STATE_RIGHT:
				if (skidooItem->Animation.Velocity.z >= 0)
				{
					if (!IsHeld(In::Right))
						laraItem->Animation.TargetState = SKIDOO_STATE_DRIVE;
				}
				else
				{
					if (!IsHeld(In::Left))
						laraItem->Animation.TargetState = SKIDOO_STATE_DRIVE;
				}

				break;

			case SKIDOO_STATE_FALL:
				if (skidooItem->Animation.Velocity.y <= 0 ||
					skidoo->LeftVerticalVelocity <= 0 ||
					skidoo->RightVerticalVelocity <= 0)
				{
					laraItem->Animation.TargetState = SKIDOO_STATE_DRIVE;
					SoundEffect(SFX_TR2_VEHICLE_IMPACT3, &skidooItem->Pose);
				}
				else if (skidooItem->Animation.Velocity.y > (SKIDOO_DAMAGE_START + SKIDOO_DAMAGE_LENGTH))
					laraItem->Animation.TargetState = SKIDOO_STATE_JUMP_OFF;

				break;
			}
		}
	}

	int GetSkidooCollisionAnim(ItemInfo* skidooItem, Vector3i* moved)
	{
		moved->x = skidooItem->Pose.Position.x - moved->x;
		moved->z = skidooItem->Pose.Position.z - moved->z;

		if (moved->x || moved->z)
		{
			float sinY = phd_sin(skidooItem->Pose.Orientation.y);
			float cosY = phd_cos(skidooItem->Pose.Orientation.y);

			int front = (moved->z * cosY) + (moved->x * sinY);
			int side = (moved->z * -sinY) + (moved->x * cosY);

			if (abs(front) > abs(side))
			{
				if (front > 0)
					return SKIDOO_ANIM_HIT_BACK;
				else
					return SKIDOO_ANIM_HIT_FRONT;
			}
			else
			{
				if (side > 0)
					return SKIDOO_ANIM_HIT_LEFT;
				else
					return SKIDOO_ANIM_HIT_RIGHT;
			}
		}

		return 0;
	}

	void SkidooGuns(ItemInfo* skidooItem, ItemInfo* laraItem)
	{
		auto* skidoo = GetSkidooInfo(skidooItem);
		auto* lara = GetLaraInfo(laraItem);
		auto& weapon = Weapons[(int)LaraWeaponType::Snowmobile];

		FindNewTarget(*laraItem, weapon);
		AimWeapon(*laraItem, lara->RightArm, weapon);

		if (IsHeld(In::Fire) && !skidooItem->ItemFlags[0])
		{
			auto angles = EulerAngles(
				lara->RightArm.Orientation.x,
				lara->RightArm.Orientation.y + laraItem->Pose.Orientation.y,
				0);

			FireWeapon(LaraWeaponType::Snowmobile, lara->TargetEntity, *laraItem, angles);
			FireWeapon(LaraWeaponType::Snowmobile, lara->TargetEntity, *laraItem, angles);
			SoundEffect(weapon.SampleNum, &laraItem->Pose);
			skidooItem->ItemFlags[0] = 4;
		}

		if (skidooItem->ItemFlags[0])
			skidooItem->ItemFlags[0]--;
	}

	void DoSnowEffect(ItemInfo* skidooItem)
	{
		auto pointColl = GetPointCollision(*skidooItem);
		auto material = pointColl.GetBottomSector().GetSurfaceMaterial(pointColl.GetPosition().x, pointColl.GetPosition().z, true);
		if (material != MaterialType::Ice && material != MaterialType::Snow)
			return;

		TEN::Effects::TriggerSnowmobileSnow(skidooItem);
	}

	int DoSkidooDynamics(int height, int verticalVelocity, int* y)
	{
		// Grounded.
		if (height > *y)
		{
			*y += verticalVelocity;
			if (*y > height - SKIDOO_MIN_BOUNCE)
			{
				*y = height;
				verticalVelocity = 0;
			}
			else
			{
				verticalVelocity += g_GameFlow->GetSettings()->Physics.Gravity;
			}
		}
		// Airborne.
		else
		{
			int kick = (height - *y) * 4;
			if (kick < SKIDOO_KICK_MAX)
				kick = SKIDOO_KICK_MAX;

			verticalVelocity += (kick - verticalVelocity) / 8;

			if (*y > height)
				*y = height;
		}

		return verticalVelocity;
	}

	short DoSkidooShift(ItemInfo* skidooItem, Vector3i* pos, Vector3i* old)
	{
		int	x = pos->x / BLOCK(1);
		int z = pos->z / BLOCK(1);
		int xOld = old->x / BLOCK(1);
		int zOld = old->z / BLOCK(1);
		int shiftX = pos->x & WALL_MASK;
		int shiftZ = pos->z & WALL_MASK;

		if (x == xOld)
		{
			if (z == zOld)
			{
				skidooItem->Pose.Position.z += (old->z - pos->z);
				skidooItem->Pose.Position.x += (old->x - pos->x);
			}
			else if (z > zOld)
			{
				skidooItem->Pose.Position.z -= shiftZ + 1;
				return (pos->x - skidooItem->Pose.Position.x);
			}
			else
			{
				skidooItem->Pose.Position.z += BLOCK(1) - shiftZ;
				return (skidooItem->Pose.Position.x - pos->x);
			}
		}
		else if (z == zOld)
		{
			if (x > xOld)
			{
				skidooItem->Pose.Position.x -= shiftX + 1;
				return (skidooItem->Pose.Position.z - pos->z);
			}
			else
			{
				skidooItem->Pose.Position.x += BLOCK(1) - shiftX;
				return (pos->z - skidooItem->Pose.Position.z);
			}
		}
		else
		{
			x = 0;
			z = 0;

			auto probe = GetPointCollision(Vector3i(old->x, pos->y, pos->z), skidooItem->RoomNumber);
			if (probe.GetFloorHeight() < (old->y - CLICK(1)))
			{
				if (pos->z > old->z)
					z = -shiftZ - 1;
				else
					z = BLOCK(1) - shiftZ;
			}

			probe = GetPointCollision(Vector3i(pos->x, pos->y, old->z), skidooItem->RoomNumber);
			if (probe.GetFloorHeight() < (old->y - CLICK(1)))
			{
				if (pos->x > old->x)
					x = -shiftX - 1;
				else
					x = BLOCK(1) - shiftX;
			}

			if (x && z)
			{
				skidooItem->Pose.Position.z += z;
				skidooItem->Pose.Position.x += x;
				skidooItem->Animation.Velocity.z -= 50;
			}
			else if (z)
			{
				skidooItem->Pose.Position.z += z;
				skidooItem->Animation.Velocity.z -= 50;

				if (z > 0)
					return (skidooItem->Pose.Position.x - pos->x);
				else
					return (pos->x - skidooItem->Pose.Position.x);
			}
			else if (x)
			{
				skidooItem->Pose.Position.x += x;
				skidooItem->Animation.Velocity.z -= 50;

				if (x > 0)
					return (pos->z - skidooItem->Pose.Position.z);
				else
					return (skidooItem->Pose.Position.z - pos->z);
			}
			else
			{
				skidooItem->Pose.Position.z += old->z - pos->z;
				skidooItem->Pose.Position.x += old->x - pos->x;
				skidooItem->Animation.Velocity.z -= 50;
			}
		}

		return 0;
	}

	int SkidooDynamics(ItemInfo* skidooItem, ItemInfo* laraItem)
	{
		auto* skidoo = GetSkidooInfo(skidooItem);

		Vector3i frontLeftOld, frontRightOld, backLeftOld, backRightOld;
		auto heightFrontLeftOld = GetVehicleHeight(skidooItem, SKIDOO_FRONT, -SKIDOO_SIDE, true, &frontLeftOld);
		auto heightFrontRightOld = GetVehicleHeight(skidooItem, SKIDOO_FRONT, SKIDOO_SIDE, true, &frontRightOld);
		auto heightBackLeftOld = GetVehicleHeight(skidooItem, -SKIDOO_FRONT, -SKIDOO_SIDE, true, &backLeftOld);
		auto heightBackRightOld = GetVehicleHeight(skidooItem, -SKIDOO_FRONT, SKIDOO_SIDE, true, &backRightOld);

		auto oldPos = skidooItem->Pose.Position;

		short rotation;

		if (skidooItem->Pose.Position.y > (skidooItem->Floor - CLICK(1)))
		{
			if (skidoo->TurnRate < -SKIDOO_TURN_RATE_DECEL)
				skidoo->TurnRate += SKIDOO_TURN_RATE_DECEL;
			else if (skidoo->TurnRate > SKIDOO_TURN_RATE_DECEL)
				skidoo->TurnRate -= SKIDOO_TURN_RATE_DECEL;
			else
				skidoo->TurnRate = 0;
			skidooItem->Pose.Orientation.y += skidoo->TurnRate + skidoo->ExtraRotation;

			rotation = skidooItem->Pose.Orientation.y - skidoo->MomentumAngle;
			if (rotation < -SKIDOO_MOMENTUM_TURN_RATE_ACCEL)
			{
				if (rotation < -SKIDOO_MOMENTUM_TURN_RATE_MAX)
				{
					rotation = -SKIDOO_MOMENTUM_TURN_RATE_MAX;
					skidoo->MomentumAngle = skidooItem->Pose.Orientation.y - rotation;
				}
				else
					skidoo->MomentumAngle -= SKIDOO_MOMENTUM_TURN_RATE_ACCEL;
			}
			else if (rotation > SKIDOO_MOMENTUM_TURN_RATE_ACCEL)
			{
				if (rotation > SKIDOO_MOMENTUM_TURN_RATE_MAX)
				{
					rotation = SKIDOO_MOMENTUM_TURN_RATE_MAX;
					skidoo->MomentumAngle = skidooItem->Pose.Orientation.y - rotation;
				}
				else
					skidoo->MomentumAngle += SKIDOO_MOMENTUM_TURN_RATE_ACCEL;
			}
			else
				skidoo->MomentumAngle = skidooItem->Pose.Orientation.y;
		}
		else
			skidooItem->Pose.Orientation.y += skidoo->TurnRate + skidoo->ExtraRotation;

		skidooItem->Pose.Translate(skidoo->MomentumAngle, skidooItem->Animation.Velocity.z);

		int slip = SKIDOO_SLIP * phd_sin(skidooItem->Pose.Orientation.x);
		if (abs(slip) > (SKIDOO_SLIP / 2))
		{
			skidooItem->Pose.Position.x -= slip * phd_sin(skidooItem->Pose.Orientation.y);
			skidooItem->Pose.Position.z -= slip * phd_cos(skidooItem->Pose.Orientation.y);
		}

		slip = SKIDOO_SLIP_SIDE * phd_sin(skidooItem->Pose.Orientation.z);
		if (abs(slip) > (SKIDOO_SLIP_SIDE / 2))
		{
			skidooItem->Pose.Position.x += slip * phd_cos(skidooItem->Pose.Orientation.y);
			skidooItem->Pose.Position.z -= slip * phd_sin(skidooItem->Pose.Orientation.y);
		}

		Vector3i moved;
		moved.x = skidooItem->Pose.Position.x;
		moved.z = skidooItem->Pose.Position.z;

		if (!(skidooItem->Flags & IFLAG_INVISIBLE))
			DoVehicleCollision(skidooItem, SKIDOO_RADIUS);

		Vector3i frontLeft, frontRight, backRight, backLeft;
		rotation = 0;
		auto heightBackLeft = GetVehicleHeight(skidooItem, -SKIDOO_FRONT, -SKIDOO_SIDE, false, &backLeft);
		if (heightBackLeft < (backLeftOld.y - CLICK(1)))
			rotation = DoSkidooShift(skidooItem, &backLeft, &backLeftOld);

		auto heightBackRight = GetVehicleHeight(skidooItem, -SKIDOO_FRONT, SKIDOO_SIDE, false, &backRight);
		if (heightBackRight < (backRightOld.y - CLICK(1)))
			rotation += DoSkidooShift(skidooItem, &backRight, &backRightOld);

		auto heightFrontLeft = GetVehicleHeight(skidooItem, SKIDOO_FRONT, -SKIDOO_SIDE, false, &frontLeft);
		if (heightFrontLeft < (frontLeftOld.y - CLICK(1)))
			rotation += DoSkidooShift(skidooItem, &frontLeft, &frontLeftOld);

		auto heightFrontRight = GetVehicleHeight(skidooItem, SKIDOO_FRONT, SKIDOO_SIDE, false, &frontRight);
		if (heightFrontRight < (frontRightOld.y - CLICK(1)))
			rotation += DoSkidooShift(skidooItem, &frontRight, &frontRightOld);

		auto probe = GetPointCollision(*skidooItem);
		if (probe.GetFloorHeight() < (skidooItem->Pose.Position.y - CLICK(1)))
			DoSkidooShift(skidooItem, (Vector3i*)&skidooItem->Pose, &oldPos);

		skidoo->ExtraRotation = rotation;

		auto collide = GetSkidooCollisionAnim(skidooItem, &moved);
		if (collide)
		{
			int newVelocity = (skidooItem->Pose.Position.z - oldPos.z) * phd_cos(skidoo->MomentumAngle) + (skidooItem->Pose.Position.x - oldPos.x) * phd_sin(skidoo->MomentumAngle);
			if (skidooItem->Animation.Velocity.z > (SKIDOO_NORMAL_VELOCITY_MAX + SKIDOO_VELOCITY_ACCEL) &&
				newVelocity < (skidooItem->Animation.Velocity.z - 10))
			{
				DoDamage(laraItem, (skidooItem->Animation.Velocity.z - newVelocity) / 2);
			}

			if (skidooItem->Animation.Velocity.z > 0 && newVelocity < skidooItem->Animation.Velocity.z)
				skidooItem->Animation.Velocity.z = (newVelocity < 0) ? 0 : newVelocity;
			else if (skidooItem->Animation.Velocity.z < 0 && newVelocity > skidooItem->Animation.Velocity.z)
				skidooItem->Animation.Velocity.z = (newVelocity > 0) ? 0 : newVelocity;

			if (skidooItem->Animation.Velocity.z < SKIDOO_REVERSE_VELOCITY_MAX)
				skidooItem->Animation.Velocity.z = SKIDOO_REVERSE_VELOCITY_MAX;
		}

		return collide;
	}
}
