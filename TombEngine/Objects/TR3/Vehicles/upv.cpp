#include "framework.h"
#include "Objects/TR3/Vehicles/upv.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/collision/Sphere.h"
#include "Game/control/box.h"
#include "Game/control/los.h"
#include "Game/effects/Bubble.h"
#include "Game/effects/effects.h"
#include "Game/effects/Streamer.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_fire.h"
#include "Game/Lara/lara_flare.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_one_gun.h"
#include "Game/savegame.h"
#include "Game/Setup.h"
#include "Objects/Sink.h"
#include "Objects/TR3/Vehicles/upv_info.h"
#include "Objects/Utils/VehicleHelpers.h"
#include "Sound/sound.h"
#include "Specific/level.h"
#include "Specific/Input/Input.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Collision::Sphere;
using namespace TEN::Effects::Bubble;
using namespace TEN::Effects::Streamer;
using namespace TEN::Input;

// TODO:
// Redo water surface dismount.
// Calibrate rotation control to work well on both keyboard and gamepad.
// Improve deflection.
// Try to improve room collision.

namespace TEN::Entities::Vehicles
{
	constexpr auto UPV_RADIUS = 300;
	constexpr auto UPV_HEIGHT = 400;
	constexpr auto UPV_LENGTH = BLOCK(1);
	constexpr auto UPV_WATER_SURFACE_DISTANCE = 210;
	constexpr auto UPV_MOUNT_DISTANCE = CLICK(2);
	constexpr auto UPV_DISMOUNT_DISTANCE = BLOCK(1);

	constexpr int UPV_VELOCITY_ACCEL = 4 * VEHICLE_VELOCITY_SCALE;
	constexpr int UPV_VELOCITY_FRICTION_DECEL = 1.5f * VEHICLE_VELOCITY_SCALE;
	constexpr int UPV_VELOCITY_MAX = 64 * VEHICLE_VELOCITY_SCALE;

	constexpr int UPV_HARPOON_RELOAD_TIME = 15;
	constexpr int UPV_HARPOON_VELOCITY = CLICK(1);
	constexpr int UPV_SHIFT = 128;

	// TODO: These should probably be done in the wad. @Sezz 2022.06.24
	constexpr auto UPV_DEATH_FRAME_1 = 16;
	constexpr auto UPV_DEATH_FRAME_2 = 17;
	constexpr auto UPV_MOUNT_WATER_SURFACE_SOUND_FRAME = 30;
	constexpr auto UPV_MOUNT_WATER_SURFACE_CONTROL_FRAME = 50;
	constexpr auto UPV_DISMOUNT_WATER_SURFACE_FRAME = 51;
	constexpr auto UPV_MOUNT_UNDERWATER_SOUND_FRAME = 30;
	constexpr auto UPV_MOUNT_UNDERWATER_CONTROL_FRAME = 42;
	constexpr auto UPV_DISMOUNT_UNDERWATER_FRAME = 42;

	constexpr auto UPV_WAKE_OFFSET = Vector3(BLOCK(1 / 3.0f), -BLOCK(1 / 8.0f), BLOCK(1 / 10.0f));

	#define UPV_X_TURN_RATE_DIVE_ACCEL	   ANGLE(5.0f)
	#define UPV_X_TURN_RATE_ACCEL		   ANGLE(0.6f)
	#define UPV_X_TURN_RATE_FRICTION_DECEL ANGLE(0.3f)
	#define UPV_X_TURN_RATE_MAX			   ANGLE(3.25f)

	#define UPV_Y_TURN_RATE_ACCEL		   ANGLE(0.6f)
	#define UPV_Y_TURN_RATE_FRICTION_DECEL ANGLE(0.3f)
	#define UPV_Y_TURN_RATE_MAX			   ANGLE(3.75f)

	#define UPV_X_ORIENT_WATER_SURFACE_MAX ANGLE(30.0f)
	#define UPV_X_ORIENT_DIVE_MAX		   ANGLE(15.0f)
	#define UPV_X_ORIENT_MAX			   ANGLE(85.0f)

	#define UPV_DEFLECT_ANGLE		 ANGLE(45.0f)
	#define UPV_DEFLCT_TURN_RATE_MAX ANGLE(2.0f)

	#define UPV_LEAN_RATE ANGLE(0.6f)
	#define UPV_LEAN_MAX  ANGLE(10.0f)

	const CreatureBiteInfo UPVBites[6] =
	{
		CreatureBiteInfo(Vector3(0, 0, 0), 3),
		CreatureBiteInfo(Vector3(0, 96, 256), 0),
		CreatureBiteInfo(Vector3(-128, 0, 64), 1),
		CreatureBiteInfo(Vector3(0, 0, -64), 1),
		CreatureBiteInfo(Vector3(128, 0, 64), 2),
		CreatureBiteInfo(Vector3(0, 0, -64), 2)
	};

	const std::vector<VehicleMountType> UPVMountTypes =
	{
		VehicleMountType::LevelStart,
		VehicleMountType::Back
	};

	enum UPVState
	{
		UPV_STATE_DEATH = 0,
		UPV_STATE_COLLIDE = 1,
		UPV_STATE_DISMOUNT_WATER_SURFACE = 2,
		UPV_STATE_UNUSED_1 = 3, // Unused.
		UPV_STATE_MOVE = 4,
		UPV_STATE_IDLE = 5,
		UPV_STATE_UNUSED_2 = 6, // Unused.
		UPV_STATE_UNUSED_3 = 7, // Unused.
		UPV_STATE_MOUNT = 8,
		UPV_STATE_DISMOUNT_UNDERWATER = 9
	};

	enum UPVAnim
	{
		UPV_ANIM_MOVING_DEATH = 0,
		UPV_ANIM_IDLE_DEATH = 1,
		UPV_ANIM_COLLIDE_FRONT = 2,
		UPV_ANIM_MOVE = 3,
		UPV_ANIM_COLLIDE_FRONT_2 = 4, // Unused.
		UPV_ANIM_IDLE = 5,
		UPV_ANIM_IDLE_TO_MOVE = 6,
		UPV_ANIM_MOVE_TO_IDLE = 7,
		UPV_ANIM_DISMOUNT_WATER_SURFACE_START = 8,
		UPV_ANIM_DISMOUNT_WATER_SURFACE_END = 9,
		UPV_ANIM_MOUNT_SURFACE_START = 10,
		UPV_ANIM_MOUNT_SURFACE_END = 11,
		UPV_ANIM_DISMOUNT_UNDERWATER = 12,
		UPV_ANIM_MOUNT_UNDERWATER = 13,
	};

	enum UPVJoint
	{
		UPV_JOINT_LEFT_RUDDER  = 1,
		UPV_JOINT_RIGHT_RUDDER = 2,
		UPV_JOINT_TURBINE	   = 3
	};
	
	enum UPVBiteIndex
	{
		UPV_BITE_TURBINE			= 0,
		UPV_BITE_FRONT_LIGHT		= 1,
		UPV_BITE_LEFT_RUDDER_LEFT   = 2,
		UPV_BITE_LEFT_RUDDER_RIGHT  = 3, // Unused.
		UPV_BITE_RIGHT_RUDDER_RIGHT = 4,
		UPV_BITE_RIGHT_RUDDER_LEFT  = 5	 // Unused.
	};

	UPVInfo* GetUPVInfo(ItemInfo* UPVItem)
	{
		return (UPVInfo*)UPVItem->Data;
	}

	void InitializeUPV(short itemNumber)
	{
		auto* UPVItem = &g_Level.Items[itemNumber];
		UPVItem->Data = UPVInfo();
		auto* UPV = GetUPVInfo(UPVItem);

		UPV->Flags = UPV_FLAG_SURFACE;
	}

	void UPVPlayerCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* UPVItem = &g_Level.Items[itemNumber];
		auto* lara = GetLaraInfo(laraItem);

		if (laraItem->HitPoints <= 0 || lara->Context.Vehicle != NO_VALUE)
			return;

		auto mountType = GetVehicleMountType(UPVItem, laraItem, coll, UPVMountTypes, UPV_MOUNT_DISTANCE);
		if (mountType == VehicleMountType::None)
		{
			// HACK: Collision in water behaves differently? @Sezz 2022.06.28
			if (TestBoundsCollide(UPVItem, laraItem, coll->Setup.Radius) && HandleItemSphereCollision(*UPVItem, *laraItem))
				ItemPushItem(UPVItem, laraItem, coll, false, 0);
		}
		else
		{
			SetLaraVehicle(laraItem, UPVItem);
			DoUPVMount(UPVItem, laraItem, mountType);
		}
	}

	void DoUPVMount(ItemInfo* UPVItem, ItemInfo* laraItem, VehicleMountType mountType)
	{
		auto* lara = GetLaraInfo(laraItem);

		switch (mountType)
		{
		default:
		case VehicleMountType::LevelStart:
			SetAnimation(*laraItem, ID_UPV_LARA_ANIMS, UPV_ANIM_IDLE);
			break;

		case VehicleMountType::Back:
			if (lara->Control.WaterStatus == WaterStatus::TreadWater)
				SetAnimation(*laraItem, ID_UPV_LARA_ANIMS, UPV_ANIM_MOUNT_SURFACE_START);
			else
				SetAnimation(*laraItem, ID_UPV_LARA_ANIMS, UPV_ANIM_MOUNT_UNDERWATER);

			break;
		}

		DoVehicleFlareDiscard(laraItem);
		laraItem->Pose = UPVItem->Pose;
		lara->Control.HandStatus = HandStatus::Busy;
		lara->Control.WaterStatus = WaterStatus::Dry;
		UPVItem->HitPoints = 1;

		AnimateItem(*laraItem);
	}

	static void FireUPVHarpoon(ItemInfo* UPVItem, ItemInfo* laraItem)
	{
		auto& upv = *GetUPVInfo(UPVItem);

		auto harpoonPose = Pose(GetJointPosition(UPVItem, UPV_JOINT_TURBINE, Vector3i((upv.HarpoonLeft ? 22 : -22), 24, 230)));
		if (!FireHarpoon(*laraItem, harpoonPose))
			return;

		auto soundID = (upv.Flags & UPV_FLAG_SURFACE) ? SFX_TR4_HARPOON_FIRE_DRY : SFX_TR4_HARPOON_FIRE_UNDERWATER;
		SoundEffect(soundID, &harpoonPose, SoundEnvironment::Always);

		upv.HarpoonLeft = !upv.HarpoonLeft;
	}

	static void TriggerUPVMist(long x, long y, long z, long velocity, short angle)
	{
		auto* sptr = GetFreeParticle();

		sptr->on = 1;
		sptr->sR = 0;
		sptr->sG = 0;
		sptr->sB = 0;

		sptr->dR = 64;
		sptr->dG = 64;
		sptr->dB = 64;

		sptr->colFadeSpeed = 4 + (GetRandomControl() & 3);
		sptr->fadeToBlack = 12;
		sptr->sLife = sptr->life = (GetRandomControl() & 3) + 20;
		sptr->blendMode = BlendMode::Additive;
		sptr->extras = 0;
		sptr->dynamic = -1;

		sptr->x = x + ((GetRandomControl() & 15) - 8);
		sptr->y = y + ((GetRandomControl() & 15) - 8);
		sptr->z = z + ((GetRandomControl() & 15) - 8);
		long zv = velocity * phd_cos(angle) / 4;
		long xv = velocity * phd_sin(angle) / 4;
		sptr->xVel = xv + ((GetRandomControl() & 127) - 64);
		sptr->yVel = 0;
		sptr->zVel = zv + ((GetRandomControl() & 127) - 64);
		sptr->friction = 3;

		if (GetRandomControl() & 1)
		{
			sptr->flags = SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF;
			sptr->rotAng = GetRandomControl() & 4095;

			if (GetRandomControl() & 1)
				sptr->rotAdd = -(GetRandomControl() & 15) - 16;
			else
				sptr->rotAdd = (GetRandomControl() & 15) + 16;
		}
		else
			sptr->flags = SP_SCALE | SP_DEF | SP_EXPDEF;

		sptr->scalar = 3;
		sptr->gravity = sptr->maxYvel = 0;
		long size = (GetRandomControl() & 7) + (velocity / 2) + 16;
		sptr->size = sptr->sSize = size / 4;
		sptr->dSize = size;
	}

	void UPVEffects(short itemNumber)
	{
		if (itemNumber == NO_VALUE)
			return;

		auto* UPVItem = &g_Level.Items[itemNumber];
		auto* UPV = GetUPVInfo(UPVItem);
		auto* laraItem = LaraItem;
		auto* lara = GetLaraInfo(laraItem);

		if (lara->Context.Vehicle == itemNumber)
		{
			UPV->TurbineRotation += UPV->Velocity ? (UPV->Velocity / 8) : ANGLE(2.0f);
			UPV->LeftRudderRotation = (UPV->TurnRate.x + UPV->TurnRate.y) * 8;
			UPV->RightRudderRotation = (UPV->TurnRate.x + -UPV->TurnRate.y) * 8;

			if (UPV->Velocity)
			{
				auto pos = GetJointPosition(UPVItem, UPVBites[UPV_BITE_TURBINE]).ToVector3();
				TriggerUPVMist(pos.x, pos.y + UPV_SHIFT, pos.z, abs(UPV->Velocity) / VEHICLE_VELOCITY_SCALE, UPVItem->Pose.Orientation.y + ANGLE(180.0f));

				auto sphere = BoundingSphere(pos, BLOCK(1 / 32.0f));
				if (Random::TestProbability(1 / 2.0f))
				{
					auto bubblePos = Random::GeneratePointInSphere(sphere);
					int probedRoomNumber = GetPointCollision(bubblePos, UPVItem->RoomNumber).GetRoomNumber();
				
					for (int i = 0; i < 3; i++)
						SpawnBubble(bubblePos, probedRoomNumber, (int)BubbleFlags::HighAmplitude);
				}
			}
		}
	
		for (int lp = 0; lp < 2; lp++)
		{
			int random = 31 - (GetRandomControl() & 3);
			auto pos = GetJointPosition(UPVItem, UPVBites[UPV_BITE_FRONT_LIGHT].BoneID, Vector3i(
				UPVBites[UPV_BITE_FRONT_LIGHT].Position.x,
				UPVBites[UPV_BITE_FRONT_LIGHT].Position.y,
				(int)UPVBites[UPV_BITE_FRONT_LIGHT].Position.z << (lp * 6)
			));

			GameVector origin;
			if (lp == 1)
			{
				auto target = GameVector(pos, UPVItem->RoomNumber);
				LOS(&origin, &target);
				pos = Vector3i(target.x, target.y, target.z);
			}
			else
				origin = GameVector(pos, UPVItem->RoomNumber);

			SpawnDynamicLight(pos.x, pos.y, pos.z, 16 + (lp << 3), random, random, random);
		}

		if (UPV->HarpoonTimer)
			UPV->HarpoonTimer--;
	}

	static bool TestUPVDismount(ItemInfo* UPVItem, ItemInfo* laraItem)
	{
		auto* lara = GetLaraInfo(laraItem);

		if (lara->Context.WaterCurrentPull.x || lara->Context.WaterCurrentPull.z)
			return false;

		short moveAngle = UPVItem->Pose.Orientation.y + ANGLE(180.0f);
		int velocity = UPV_DISMOUNT_DISTANCE * phd_cos(UPVItem->Pose.Orientation.x);
		int x = UPVItem->Pose.Position.x + velocity * phd_sin(moveAngle);
		int z = UPVItem->Pose.Position.z + velocity * phd_cos(moveAngle);
		int y = UPVItem->Pose.Position.y - UPV_DISMOUNT_DISTANCE * phd_sin(-UPVItem->Pose.Orientation.x);

		auto probe = GetPointCollision(Vector3i(x, y, z), UPVItem->RoomNumber);
		if ((probe.GetFloorHeight() - probe.GetCeilingHeight()) < CLICK(1) ||
			probe.GetFloorHeight() < y ||
			probe.GetCeilingHeight() > y ||
			probe.GetFloorHeight() == NO_HEIGHT ||
			probe.GetCeilingHeight() == NO_HEIGHT)
		{
			return false;
		}

		return true;
	}

	static void DoCurrent(ItemInfo* UPVItem, ItemInfo* laraItem)
	{
		auto* lara = GetLaraInfo(laraItem);

		Vector3i target;

		if (!lara->Context.WaterCurrentActive)
		{
			int absVel = abs(lara->Context.WaterCurrentPull.x);
			int shift;
			if (absVel > 16)
				shift = 4;
			else if (absVel > 8)
				shift = 3;
			else
				shift = 2;

			lara->Context.WaterCurrentPull.x -= lara->Context.WaterCurrentPull.x >> shift;

			if (abs(lara->Context.WaterCurrentPull.x) < 4)
				lara->Context.WaterCurrentPull.x = 0;

			absVel = abs(lara->Context.WaterCurrentPull.z);
			if (absVel > 16)
				shift = 4;
			else if (absVel > 8)
				shift = 3;
			else
				shift = 2;

			lara->Context.WaterCurrentPull.z -= lara->Context.WaterCurrentPull.z >> shift;
			if (abs(lara->Context.WaterCurrentPull.z) < 4)
				lara->Context.WaterCurrentPull.z = 0;

			if (lara->Context.WaterCurrentPull.x == 0 && lara->Context.WaterCurrentPull.z == 0)
				return;
		}
		else
		{
			int sinkVal = lara->Context.WaterCurrentActive - 1;
			target = g_Level.Sinks[sinkVal].Position;
		
			int angle = ((Geometry::GetOrientToPoint(laraItem->Pose.Position.ToVector3(), target.ToVector3()).y) / 16) & 4095;

			int dx = target.x - laraItem->Pose.Position.x;
			int dz = target.z - laraItem->Pose.Position.z;

			int velocity = g_Level.Sinks[sinkVal].Strength;
			dx = phd_sin(angle * 16) * velocity * BLOCK(1);
			dz = phd_cos(angle * 16) * velocity * BLOCK(1);

			lara->Context.WaterCurrentPull.x += ((dx - lara->Context.WaterCurrentPull.x) / 16);
			lara->Context.WaterCurrentPull.z += ((dz - lara->Context.WaterCurrentPull.z) / 16);
		}

		lara->Context.WaterCurrentActive = 0;
		UPVItem->Pose.Position.x += lara->Context.WaterCurrentPull.x / CLICK(1);
		UPVItem->Pose.Position.z += lara->Context.WaterCurrentPull.z / CLICK(1);
	}

	static void BackgroundCollision(ItemInfo* UPVItem, ItemInfo* laraItem)
	{
		auto* UPV = GetUPVInfo(UPVItem);
		auto* lara = GetLaraInfo(laraItem);
		CollisionInfo cinfo, * coll = &cinfo; // ??

		coll->Setup.Mode = CollisionProbeMode::Quadrants;
		coll->Setup.Radius = UPV_RADIUS;

		coll->Setup.LowerFloorBound = NO_LOWER_BOUND;
		coll->Setup.UpperFloorBound = -UPV_HEIGHT;
		coll->Setup.LowerCeilingBound = UPV_HEIGHT;
		coll->Setup.UpperCeilingBound = NO_UPPER_BOUND;

		coll->Setup.BlockFloorSlopeUp = false;
		coll->Setup.BlockFloorSlopeDown = false;
		coll->Setup.BlockDeathFloorDown = false;
		coll->Setup.BlockMonkeySwingEdge = false;
		coll->Setup.EnableObjectPush = true;
		coll->Setup.EnableSpasm = false;

		coll->Setup.PrevPosition = UPVItem->Pose.Position;

		if ((UPVItem->Pose.Orientation.x >= -(SHRT_MAX / 2 + 1)) && (UPVItem->Pose.Orientation.x <= (SHRT_MAX / 2 + 1)))
		{
			lara->Control.MoveAngle = UPVItem->Pose.Orientation.y;
			coll->Setup.ForwardAngle = lara->Control.MoveAngle;
		}
		else
		{
			lara->Control.MoveAngle = UPVItem->Pose.Orientation.y - ANGLE(180.0f);
			coll->Setup.ForwardAngle = lara->Control.MoveAngle;
		}

		int height = phd_sin(UPVItem->Pose.Orientation.x) * UPV_LENGTH;
		if (height < 0)
			height = -height;
		if (height < 200)
			height = 200;

		coll->Setup.UpperFloorBound = -height;
		coll->Setup.Height = height;

		GetCollisionInfo(coll, UPVItem, Vector3i(0, height / 2, 0));
		ShiftItem(UPVItem, coll);

		if (coll->CollisionType == CollisionType::Front)
		{
			if (UPV->TurnRate.x > UPV_DEFLECT_ANGLE)
				UPV->TurnRate.x += UPV_DEFLCT_TURN_RATE_MAX;
			else if (UPV->TurnRate.x < -UPV_DEFLECT_ANGLE)
				UPV->TurnRate.x -= UPV_DEFLCT_TURN_RATE_MAX;
			else
			{
				if (abs(UPV->Velocity) >= UPV_VELOCITY_MAX)
				{
					laraItem->Animation.TargetState = UPV_STATE_COLLIDE;
					UPV->Velocity = -UPV->Velocity / 2;
				}
				else
					UPV->Velocity = 0;
			}
		}
		else if (coll->CollisionType == CollisionType::Top)
		{
			if (UPV->TurnRate.x >= -UPV_DEFLECT_ANGLE)
				UPV->TurnRate.x -= UPV_DEFLCT_TURN_RATE_MAX;
		}
		else if (coll->CollisionType == CollisionType::TopFront)
			UPV->Velocity = 0;
		else if (coll->CollisionType == CollisionType::Left)
			UPVItem->Pose.Orientation.y += ANGLE(5.0f);
		else if (coll->CollisionType == CollisionType::Right)
			UPVItem->Pose.Orientation.y -= ANGLE(5.0f);
		else if (coll->CollisionType == CollisionType::Clamp)
		{
			UPVItem->Pose.Position = coll->Setup.PrevPosition;
			UPV->Velocity = 0;
			return;
		}

		if (coll->Middle.Floor < 0)
		{
			UPVItem->Pose.Position.y += coll->Middle.Floor;
			UPV->TurnRate.x += UPV_DEFLCT_TURN_RATE_MAX;
		}
	}

	static void UPVControl(ItemInfo* UPVItem, ItemInfo* laraItem)
	{
		auto* UPV = GetUPVInfo(UPVItem);
		auto* lara = GetLaraInfo(laraItem);

		TestUPVDismount(UPVItem, laraItem);

		int frame = laraItem->Animation.FrameNumber;

		switch (laraItem->Animation.ActiveState)
		{
		case UPV_STATE_MOVE:
			if (laraItem->HitPoints <= 0)
			{
				laraItem->Animation.TargetState = UPV_STATE_DEATH;
				break;
			}

			if (IsHeld(In::Left) || IsHeld(In::Right))
			{
				ModulateVehicleTurnRateY(&UPV->TurnRate.y, UPV_Y_TURN_RATE_ACCEL, -UPV_Y_TURN_RATE_MAX, UPV_Y_TURN_RATE_MAX);
				ModulateVehicleLean(UPVItem, UPV_LEAN_RATE, UPV_LEAN_MAX);
			}

			if (UPV->Flags & UPV_FLAG_SURFACE)
			{
				int xa = UPVItem->Pose.Orientation.x - UPV_X_ORIENT_WATER_SURFACE_MAX;
				int ax = UPV_X_ORIENT_WATER_SURFACE_MAX - UPVItem->Pose.Orientation.x;
				if (xa > 0)
				{
					if (xa > ANGLE(1.0f))
						UPVItem->Pose.Orientation.x -= ANGLE(1.0f);
					else
						UPVItem->Pose.Orientation.x -= ANGLE(0.1f);
				}
				else if (ax)
				{
					if (ax > ANGLE(1.0f))
						UPVItem->Pose.Orientation.x += ANGLE(1.0f);
					else
						UPVItem->Pose.Orientation.x += ANGLE(0.1f);
				}
				else
					UPVItem->Pose.Orientation.x = UPV_X_ORIENT_WATER_SURFACE_MAX;
			}
			else
			{
				if (IsHeld(In::Forward) || IsHeld(In::Back))
					ModulateVehicleTurnRateX(&UPV->TurnRate.x, UPV_X_TURN_RATE_ACCEL, -UPV_X_TURN_RATE_MAX, UPV_X_TURN_RATE_MAX);
			}

			if (IsHeld(In::Accelerate))
			{
				if (IsHeld(In::Forward) &&
					UPV->Flags & UPV_FLAG_SURFACE &&
					UPVItem->Pose.Orientation.x > -UPV_X_ORIENT_DIVE_MAX)
				{
					UPV->Flags |= UPV_FLAG_DIVE;
				}

				UPV->Velocity += UPV_VELOCITY_ACCEL;
			}
			else
				laraItem->Animation.TargetState = UPV_STATE_IDLE;

			break;

		case UPV_STATE_IDLE:
			if (laraItem->HitPoints <= 0)
			{
				laraItem->Animation.TargetState = UPV_STATE_DEATH;
				break;
			}
			
			if (IsHeld(In::Left) || IsHeld(In::Right))
			{
				ModulateVehicleTurnRateY(&UPV->TurnRate.y, UPV_Y_TURN_RATE_ACCEL, -UPV_Y_TURN_RATE_MAX, UPV_Y_TURN_RATE_MAX);
				ModulateVehicleLean(UPVItem, UPV_LEAN_RATE, UPV_LEAN_MAX);
			}

			if (UPV->Flags & UPV_FLAG_SURFACE)
			{
				int xa = UPVItem->Pose.Orientation.x - UPV_X_ORIENT_WATER_SURFACE_MAX;
				int ax = UPV_X_ORIENT_WATER_SURFACE_MAX - UPVItem->Pose.Orientation.x;
				if (xa > 0)
				{
					if (xa > ANGLE(1.0f))
						UPVItem->Pose.Orientation.x -= ANGLE(1.0f);
					else
						UPVItem->Pose.Orientation.x -= ANGLE(0.1f);
				}
				else if (ax)
				{
					if (ax > ANGLE(1.0f))
						UPVItem->Pose.Orientation.x += ANGLE(1.0f);
					else
						UPVItem->Pose.Orientation.x += ANGLE(0.1f);
				}
				else
					UPVItem->Pose.Orientation.x = UPV_X_ORIENT_WATER_SURFACE_MAX;
			}
			else
			{
				if (IsHeld(In::Forward) || IsHeld(In::Back))
					ModulateVehicleTurnRateX(&UPV->TurnRate.x, UPV_X_TURN_RATE_ACCEL, -UPV_X_TURN_RATE_MAX, UPV_X_TURN_RATE_MAX);
			}

			if (IsHeld(In::Brake) && TestUPVDismount(UPVItem, laraItem))
			{
				if (UPV->Velocity > 0)
					UPV->Velocity -= UPV_VELOCITY_ACCEL;
				else
				{
					if (UPV->Flags & UPV_FLAG_SURFACE)
						laraItem->Animation.TargetState = UPV_STATE_DISMOUNT_WATER_SURFACE;
					else
						laraItem->Animation.TargetState = UPV_STATE_DISMOUNT_UNDERWATER;

					//sub->Flags &= ~UPV_FLAG_CONTROL; having this here causes the UPV glitch, moving it directly to the states' code is better

					StopSoundEffect(SFX_TR3_VEHICLE_UPV_LOOP);
					SoundEffect(SFX_TR3_VEHICLE_UPV_STOP, (Pose*)&UPVItem->Pose.Position.x, SoundEnvironment::Always);
				}
			}
			else if (IsHeld(In::Accelerate))
			{
				if (IsHeld(In::Forward) &&
					UPVItem->Pose.Orientation.x > -UPV_X_ORIENT_DIVE_MAX &&
					UPV->Flags & UPV_FLAG_SURFACE)
				{
					UPV->Flags |= UPV_FLAG_DIVE;
				}

				laraItem->Animation.TargetState = UPV_STATE_MOVE;
			}

			break;

		case UPV_STATE_MOUNT:
			if (laraItem->Animation.AnimNumber == UPV_ANIM_MOUNT_SURFACE_END)
			{
				UPVItem->Pose.Position.y += 4;
				UPVItem->Pose.Orientation.x += ANGLE(1.0f);

				if (frame == UPV_MOUNT_WATER_SURFACE_SOUND_FRAME)
					SoundEffect(SFX_TR3_VEHICLE_UPV_LOOP, (Pose*)&UPVItem->Pose.Position.x, SoundEnvironment::Always);

				if (frame == UPV_MOUNT_WATER_SURFACE_CONTROL_FRAME)
					UPV->Flags |= UPV_FLAG_CONTROL;
			}

			else if (laraItem->Animation.AnimNumber == UPV_ANIM_MOUNT_UNDERWATER)
			{
				if (frame == UPV_MOUNT_UNDERWATER_SOUND_FRAME)
					SoundEffect(SFX_TR3_VEHICLE_UPV_LOOP, (Pose*)&UPVItem->Pose.Position.x, SoundEnvironment::Always);

				if (frame == UPV_MOUNT_UNDERWATER_CONTROL_FRAME)
					UPV->Flags |= UPV_FLAG_CONTROL;
			}

			break;

		case UPV_STATE_DISMOUNT_UNDERWATER:
			if (laraItem->Animation.AnimNumber == UPV_ANIM_DISMOUNT_UNDERWATER &&
				frame == UPV_DISMOUNT_UNDERWATER_FRAME)
			{
				UPV->Flags &= ~UPV_FLAG_CONTROL;

				auto vec = GetJointPosition(laraItem, LM_HIPS);

				auto LPos = GameVector(
					vec.x,
					vec.y,
					vec.z,
					UPVItem->RoomNumber
				);

				auto VPos = GameVector(
					UPVItem->Pose.Position.x,
					UPVItem->Pose.Position.y,
					UPVItem->Pose.Position.z,
					UPVItem->RoomNumber
				);
				LOSAndReturnTarget(&VPos, &LPos, 0);

				laraItem->Pose.Position = Vector3i(LPos.x, LPos.y, LPos.z);

				SetAnimation(*laraItem, LA_UNDERWATER_IDLE);
				laraItem->Animation.Velocity.y = 0;
				laraItem->Animation.IsAirborne = false;
				laraItem->Pose.Orientation.x = laraItem->Pose.Orientation.z = 0;

				UpdateLaraRoom(laraItem, 0);

				lara->Control.WaterStatus = WaterStatus::Underwater;
				lara->Control.HandStatus = HandStatus::Free;
				SetLaraVehicle(laraItem, nullptr);

				UPVItem->HitPoints = 0;
			}

			break;

		case UPV_STATE_DISMOUNT_WATER_SURFACE:
			if (laraItem->Animation.AnimNumber == UPV_ANIM_DISMOUNT_WATER_SURFACE_END &&
				frame == UPV_DISMOUNT_WATER_SURFACE_FRAME)
			{
				UPV->Flags &= ~UPV_FLAG_CONTROL;
				int waterDepth, waterHeight, heightFromWater;

				auto pointColl = GetPointCollision(*laraItem);
				waterDepth = pointColl.GetWaterSurfaceHeight();
				waterHeight = pointColl.GetWaterTopHeight();

				if (waterHeight != NO_HEIGHT)
					heightFromWater = laraItem->Pose.Position.y - waterHeight;
				else
					heightFromWater = NO_HEIGHT;

				auto vec = GetJointPosition(laraItem, LM_HIPS);

				laraItem->Pose.Position.x = vec.x;
				//laraItem->Pose.Position.y += -heightFromWater + 1; // Doesn't work as intended.
				laraItem->Pose.Position.y = vec.y;
				laraItem->Pose.Position.z = vec.z;

				SetAnimation(*laraItem, LA_ONWATER_IDLE);
				laraItem->Animation.IsAirborne = false;
				laraItem->Animation.Velocity.y = 0;
				laraItem->Pose.Orientation.x = 0;
				laraItem->Pose.Orientation.z = 0;

				UpdateLaraRoom(laraItem, -LARA_HEIGHT / 2);

				ResetPlayerFlex(laraItem);
				lara->Control.HandStatus = HandStatus::Free;
				lara->Control.WaterStatus = WaterStatus::TreadWater;
				lara->Context.WaterSurfaceDist = -heightFromWater;
				SetLaraVehicle(laraItem, nullptr);

				UPVItem->HitPoints = 0;
			}
			else
			{
				UPV->TurnRate.x -= UPV_X_TURN_RATE_ACCEL;
				if (UPVItem->Pose.Orientation.x < 0)
					UPVItem->Pose.Orientation.x = 0;
			}

			break;

		case UPV_STATE_DEATH:
			if ((laraItem->Animation.AnimNumber == UPV_ANIM_IDLE_DEATH || laraItem->Animation.AnimNumber == UPV_ANIM_MOVING_DEATH) &&
				(frame == UPV_DEATH_FRAME_1 || frame == UPV_DEATH_FRAME_2))
			{
				auto vec = GetJointPosition(laraItem, LM_HIPS);
				laraItem->Pose.Position = vec;
				laraItem->Pose.Orientation.x = 0;
				laraItem->Pose.Orientation.z = 0;

				SetAnimation(*laraItem, LA_UNDERWATER_DEATH, 17);
				laraItem->Animation.IsAirborne = false;
				laraItem->Animation.Velocity.y = 0;
			
				UPV->Flags |= UPV_FLAG_DEAD;
			}

			UPVItem->Animation.Velocity.z = 0;
			break;
		}

		if (UPV->Flags & UPV_FLAG_DIVE)
		{
			if (UPVItem->Pose.Orientation.x > -UPV_X_ORIENT_DIVE_MAX)
				UPVItem->Pose.Orientation.x -= UPV_X_TURN_RATE_DIVE_ACCEL;
			else
				UPV->Flags &= ~UPV_FLAG_DIVE;
		}

		if (UPV->Velocity > 0)
		{
			UPV->Velocity -= UPV_VELOCITY_FRICTION_DECEL;
			if (UPV->Velocity < 0)
				UPV->Velocity = 0;
		}
		else if (UPV->Velocity < 0)
		{
			UPV->Velocity += UPV_VELOCITY_FRICTION_DECEL;
			if (UPV->Velocity > 0)
				UPV->Velocity = 0;
		}

		if (UPV->Velocity > UPV_VELOCITY_MAX)
			UPV->Velocity = UPV_VELOCITY_MAX;
		else if (UPV->Velocity < -UPV_VELOCITY_MAX)
			UPV->Velocity = -UPV_VELOCITY_MAX;

		if (UPV->TurnRate.x > 0)
		{
			UPV->TurnRate.x -= UPV_X_TURN_RATE_FRICTION_DECEL;
			if (UPV->TurnRate.x < 0)
				UPV->TurnRate.x = 0;
		}
		else if (UPV->TurnRate.x < 0)
		{
			UPV->TurnRate.x += UPV_X_TURN_RATE_FRICTION_DECEL;
			if (UPV->TurnRate.x > 0)
				UPV->TurnRate.x = 0;
		}

		if (UPV->TurnRate.y > 0)
		{
			UPV->TurnRate.y -= UPV_Y_TURN_RATE_FRICTION_DECEL;
			if (UPV->TurnRate.y < 0)
				UPV->TurnRate.y = 0;
		}
		else if (UPV->TurnRate.y < 0)
		{
			UPV->TurnRate.y += UPV_Y_TURN_RATE_FRICTION_DECEL;
			if (UPV->TurnRate.y > 0)
				UPV->TurnRate.y = 0;
		}
	}

	bool UPVControl(ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* lara = GetLaraInfo(laraItem);
		auto* UPVItem = &g_Level.Items[lara->Context.Vehicle];
		auto* UPV = GetUPVInfo(UPVItem);
	
		auto oldPos = UPVItem->Pose;
		auto probe = GetPointCollision(*UPVItem);

		if (!(UPV->Flags & UPV_FLAG_DEAD))
		{
			UPVControl(UPVItem, laraItem);

			UPVItem->Animation.Velocity.z = UPV->Velocity / VEHICLE_VELOCITY_SCALE;
			UPVItem->Pose.Orientation += UPV->TurnRate;

			if (UPVItem->Pose.Orientation.x > UPV_X_ORIENT_MAX)
				UPVItem->Pose.Orientation.x = UPV_X_ORIENT_MAX;
			else if (UPVItem->Pose.Orientation.x < -UPV_X_ORIENT_MAX)
				UPVItem->Pose.Orientation.x = -UPV_X_ORIENT_MAX;

			if (!(IsHeld(In::Left) ) && !(IsHeld(In::Right)))
				ResetVehicleLean(UPVItem, 12.0f);

			UPVItem->Pose.Translate(UPVItem->Pose.Orientation, UPVItem->Animation.Velocity.z);
		}

		auto pointColl = GetPointCollision(*UPVItem);
		int newHeight = pointColl.GetFloorHeight();
		int waterHeight = pointColl.GetWaterTopHeight();

		if ((newHeight - waterHeight) < UPV_HEIGHT || (newHeight < UPVItem->Pose.Position.y - UPV_HEIGHT / 2) || 
			(newHeight == NO_HEIGHT) || (waterHeight == NO_HEIGHT))
		{
			UPVItem->Pose.Position = oldPos.Position;
			UPVItem->Animation.Velocity.z = 0;
		}

		UPVItem->Floor = probe.GetFloorHeight();

		if (UPV->Flags & UPV_FLAG_CONTROL && !(UPV->Flags & UPV_FLAG_DEAD))
		{
			if (!TestEnvironment(ENV_FLAG_WATER, UPVItem->RoomNumber) &&
				waterHeight != NO_HEIGHT)
			{
				if ((waterHeight - UPVItem->Pose.Position.y) >= -UPV_WATER_SURFACE_DISTANCE)
					UPVItem->Pose.Position.y = waterHeight + UPV_WATER_SURFACE_DISTANCE;

				if (!(UPV->Flags & UPV_FLAG_SURFACE))
				{
					SoundEffect(SFX_TR4_LARA_BREATH, &laraItem->Pose, SoundEnvironment::Always);
					UPV->Flags &= ~UPV_FLAG_DIVE;
				}

				UPV->Flags |= UPV_FLAG_SURFACE;
			}
			else if ((waterHeight - UPVItem->Pose.Position.y) >= -UPV_WATER_SURFACE_DISTANCE && waterHeight != NO_HEIGHT &&
					 (laraItem->Pose.Position.y - probe.GetCeilingHeight()) >= CLICK(1))
			{
				UPVItem->Pose.Position.y = waterHeight + UPV_WATER_SURFACE_DISTANCE;

				if (!(UPV->Flags & UPV_FLAG_SURFACE))
				{
					SoundEffect(SFX_TR4_LARA_BREATH, &laraItem->Pose, SoundEnvironment::Always);
					UPV->Flags &= ~UPV_FLAG_DIVE;
				}

				UPV->Flags |= UPV_FLAG_SURFACE;
			}
			else
				UPV->Flags &= ~UPV_FLAG_SURFACE;

			if (!(UPV->Flags & UPV_FLAG_SURFACE))
			{
				if (laraItem->HitPoints > 0)
				{
					lara->Status.Air--;
					if (lara->Status.Air < 0)
					{
						lara->Status.Air = -1;
						DoDamage(laraItem, 5);
					}
				}
			}
			else
			{
				if (laraItem->HitPoints >= 0)
				{
					lara->Status.Air += 10;
					if (lara->Status.Air > LARA_AIR_MAX)
						lara->Status.Air = LARA_AIR_MAX;
				}
			}
		}

		TestTriggers(UPVItem, false);
		UPVEffects(lara->Context.Vehicle);

		if (UPV->Velocity || IsDirectionalActionHeld())
		{
			waterHeight = GetPointCollision(*UPVItem).GetWaterTopHeight();
			SpawnVehicleWake(*UPVItem, UPV_WAKE_OFFSET, waterHeight, true);
		}

		if (!(UPV->Flags & UPV_FLAG_DEAD) &&
			lara->Context.Vehicle != NO_VALUE)
		{
			DoCurrent(UPVItem, laraItem);

			if (IsHeld(In::Fire) &&
				UPV->Flags & UPV_FLAG_CONTROL &&
				!UPV->HarpoonTimer)
			{
				if (laraItem->Animation.ActiveState != UPV_STATE_DISMOUNT_UNDERWATER &&
					laraItem->Animation.ActiveState != UPV_STATE_DISMOUNT_WATER_SURFACE &&
					laraItem->Animation.ActiveState != UPV_STATE_MOUNT)
				{
					FireUPVHarpoon(UPVItem, laraItem);
					UPV->HarpoonTimer = UPV_HARPOON_RELOAD_TIME;
				}
			}

			if (probe.GetRoomNumber() != UPVItem->RoomNumber)
			{
				ItemNewRoom(lara->Context.Vehicle, probe.GetRoomNumber());
				ItemNewRoom(laraItem->Index, probe.GetRoomNumber());
			}

			laraItem->Pose = UPVItem->Pose;

			AnimateItem(*laraItem);
			BackgroundCollision(UPVItem, laraItem);
			DoVehicleCollision(UPVItem, UPV_RADIUS);

			if (UPV->Flags & UPV_FLAG_CONTROL)
				SoundEffect(SFX_TR3_VEHICLE_UPV_LOOP, (Pose*)&UPVItem->Pose.Position.x, SoundEnvironment::Always, 1.0f + (float)UPVItem->Animation.Velocity.z / 96.0f);

			SyncVehicleAnim(*UPVItem, *laraItem);

			if (UPV->Flags & UPV_FLAG_SURFACE)
				Camera.targetElevation = -ANGLE(60.0f);
			else
				Camera.targetElevation = 0;

			return true;
		}
		else if (UPV->Flags & UPV_FLAG_DEAD)
		{
			AnimateItem(*laraItem);

			if (probe.GetRoomNumber() != UPVItem->RoomNumber)
				ItemNewRoom(lara->Context.Vehicle, probe.GetRoomNumber());

			BackgroundCollision(UPVItem, laraItem);

			UPV->TurnRate.x = 0;

			SetAnimation(*UPVItem, UPV_ANIM_IDLE);
			UPVItem->Animation.Velocity.y = 0;
			UPVItem->Animation.Velocity.z = 0;
			UPVItem->Animation.IsAirborne = true;
			AnimateItem(*UPVItem);

			return true;
		}
		else
			return false;
	}
}
