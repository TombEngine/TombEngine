#include "framework.h"
#include "Objects/TR4/Vehicles/jeep.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/Point.h"
#include "Game/effects/effects.h"
#include "Game/effects/simple_particle.h"
#include "Game/effects/tomb4fx.h"
#include "Game/Gui.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_flare.h"
#include "Game/Lara/lara_one_gun.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/TR4/Vehicles/jeep_info.h"
#include "Objects/Utils/VehicleHelpers.h"
#include "Renderer/RendererEnums.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Sound/sound.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Input;

namespace TEN::Entities::Vehicles
{
	char JeepSmokeStart;
	bool JeepNoGetOff;

	const std::vector<unsigned int> JeepJoints = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16 };
	const std::vector<unsigned int> JeepBrakeLightJoints = { 15, 16 };

	const std::vector<VehicleMountType> JeepMountTypes =
	{
		VehicleMountType::LevelStart,
		VehicleMountType::Left,
		VehicleMountType::Right
	};

	constexpr auto JEEP_MOUNT_DISTANCE = CLICK(2);
	constexpr auto JEEP_DISMOUNT_DISTANCE = 512;

	constexpr auto JEEP_FRONT = 550;
	constexpr auto JEEP_SIDE = 280;
	constexpr auto JEEP_SLIP = 100;
	constexpr auto JEEP_SLIP_SIDE = 128;

	constexpr auto JEEP_VELOCITY_MAX = 128 * VEHICLE_VELOCITY_SCALE;
	constexpr auto JEEP_REVERSE_VELOCITY_MAX = 64 * VEHICLE_VELOCITY_SCALE;

	constexpr auto JEEP_CRASH_VELOCITY = 10922;

	constexpr auto JEEP_WAKE_OFFSET = Vector3(BLOCK(0.25f), 0.0f, BLOCK(0.3f));

	#define JEEP_TURN_RATE_DECEL ANGLE(0.5f)

	enum JeepState
	{
		JS_IDLE = 0,
		JS_DRIVE_FORWARD = 1,
		JS_CRASH_RIGHT = 2,
		JS_CRASH_BACK = 3,
		JS_CRASH_LEFT = 4,
		JS_CRASH_FORWARD = 5,
		JS_BRAKE = 6,//?
		JS_FWD_LEFT = 7,//FWD = forwards
		JS_FWD_RIGHT = 8,
		JS_MOUNT = 9,
		JS_DISMOUNT = 10,
		JS_JUMP = 11,
		JS_LAND = 12,
		JS_BACK = 13,//when she's turning back, and when idle back.. hmm
		JS_BACK_LEFT = 14,
		JS_BACK_RIGHT = 15,
		JS_DEATH = 16,
		JS_DRIVE_BACK = 17
	};

	enum JeepAnim
	{
		JA_DEATH = 0,
		JA_BRAKE = 1,
		JA_DRIVE_FORWARD = 2,
		JA_FWD_LEFT_START = 3,//FWD = forwards
		JA_FWD_LEFT = 4,
		JA_FWD_LEFT_END = 5,
		JA_FWD_JUMP_START = 6,
		JA_JUMP = 7,
		JA_FWD_JUMP_LAND = 8,
		JA_GETIN_RIGHT = 9,
		JA_CRASH_FORWARD = 10,
		JA_CRASH_LEFT = 11,
		JA_CRASH_RIGHT = 12,
		JA_CRASH_BACK = 13,
		JA_IDLE = 14,
		JA_FWD_RIGHT_START = 15,
		JA_FWD_RIGHT = 16,
		JA_FWD_RIGHT_END = 17,
		JA_GETIN_LEFT = 18,
		JA_GETOFF = 19,
		JA_BACK_JUMP_START = 20,
		JA_BACK_JUMP = 21,
		JA_BACK_JUMP_LAND = 22,
		JA_REVERSE_START = 23,
		JA_REVERSE = 24,
		JA_REVERSE_END = 25, //aka turn head back forwards
		JA_BACK_RIGHT_START = 26,
		JA_BACK_RIGHT = 27,
		JA_BACK_RIGHT_END = 28,
		JA_BACK_LEFT_START = 29,
		JA_BACK_LEFT = 30,
		JA_BACK_LEFT_END = 31,
		JA_IDLE_RIGHT_START = 32,//turn steering whel right while idle
		JA_IDLE_LEFT_START = 33,// blah blah left while idle
		JA_IDLE_RIGHT_END = 34,//turn steering wheel straight from right while idle
		JA_IDLE_LEFT_END = 35,//blah blah straight from left while idle
		JA_IDLE_RIGHT_BACK_START = 36,//same as 32 but in reverse
		JA_IDLE_LEFT_BACK_START = 37,//same as 33 but in reverse
		JA_IDLE_BACK_RIGHT_END = 38,//same as 34 but in reverse
		JA_IDLE_BACK_LEFT_END = 39,//same as 35 but in reverse
		JA_IDLE_REVERSE_RIGHT = 40,//"change to reverse gear with the wheels still turned left"
		JA_IDLE_REVERSE_LEFT = 41,//"change to reverse gear with the wheels still turned right"
		JA_BACK_IDLE = 42,
		JA_IDLE_FWD_LEFT = 43,//aka the opposite of 41. "change to forward gear with the wheels still turned left"
		JA_IDLE_FWD_RIGHT = 44,//aka the opposite of 42. "change to forward gear with the wheels still turned right"
	};

	enum JeepFlags
	{
		JEEP_FLAG_FALLING = (1 << 6),
		JEEP_FLAG_DEAD = (1 << 7)
	};

	JeepInfo* GetJeepInfo(ItemInfo* jeepItem)
	{
		return (JeepInfo*)jeepItem->Data;
	}

	void InitializeJeep(short itemNumber)
	{
		auto* jeepItem = &g_Level.Items[itemNumber];
		jeepItem->Data = JeepInfo();
		auto* jeep = GetJeepInfo(jeepItem);

		jeepItem->MeshBits.Set(JeepJoints);
		jeepItem->MeshBits.Clear(17);
		jeep->MomentumAngle = jeepItem->Pose.Orientation.y;
	}

	void JeepPlayerCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* jeepItem = &g_Level.Items[itemNumber];
		auto* jeep = GetJeepInfo(jeepItem);
		auto* lara = GetLaraInfo(laraItem);

		if (laraItem->HitPoints <= 0 && lara->Context.Vehicle != NO_VALUE)
			return;

		auto mountType = GetVehicleMountType(jeepItem, laraItem, coll, JeepMountTypes, JEEP_MOUNT_DISTANCE);
		if (mountType == VehicleMountType::None)
			ObjectCollision(itemNumber, laraItem, coll);
		else
		{
			SetLaraVehicle(laraItem, jeepItem);
			DoJeepMount(jeepItem, laraItem, mountType);
		}
	}

	void DoJeepMount(ItemInfo* jeepItem, ItemInfo* laraItem, VehicleMountType mountType)
	{
		auto* jeep = GetJeepInfo(jeepItem);
		auto* lara = GetLaraInfo(laraItem);

		// HACK: Hardcoded jeep keys check.
		/*if (g_Gui.GetInventoryItemChosen() == ID_PUZZLE_ITEM1)
		{
			g_Gui.SetInventoryItemChosen(NO_VALUE);
			return true;
		}
		else
		{
			if (g_Gui.IsObjectInInventory(ID_PUZZLE_ITEM1))
				g_Gui.SetEnterInventory(ID_PUZZLE_ITEM1);

			return false;
		}*/
		
		switch (mountType)
		{
		default:
		case VehicleMountType::LevelStart:
			SetAnimation(*laraItem, ID_JEEP_LARA_ANIMS, JA_IDLE);
			break;

		case VehicleMountType::Left:
			SetAnimation(*laraItem, ID_JEEP_LARA_ANIMS, JA_GETIN_LEFT);
			break;

		case VehicleMountType::Right:
			SetAnimation(*laraItem, ID_JEEP_LARA_ANIMS, JA_GETIN_RIGHT);
			break;
		}

		DoVehicleFlareDiscard(laraItem);
		ResetPlayerFlex(laraItem);
		laraItem->Pose.Position = jeepItem->Pose.Position;
		laraItem->Pose.Orientation.y = jeepItem->Pose.Orientation.y;
		lara->Control.HandStatus = HandStatus::Busy;
		lara->HitDirection = -1;
		jeepItem->HitPoints = 1;
		jeepItem->Flags |= TRIGGERED;
		jeep->Revs = 0;
		jeep->Gear = 0;

		AnimateItem(*laraItem);
	}

	static int DoJeepShift(ItemInfo* jeepItem, Vector3i* pos, Vector3i* old)
	{
		int x = pos->x / BLOCK(1);
		int z = pos->z / BLOCK(1);
		int oldX = old->x / BLOCK(1);
		int oldZ = old->z / BLOCK(1);
		int shiftX = pos->x & WALL_MASK;
		int shiftZ = pos->z & WALL_MASK;

		if (x == oldX)
		{
			if (z == oldZ)
			{
				jeepItem->Pose.Position.z += (old->z - pos->z);
				jeepItem->Pose.Position.x += (old->x - pos->x);
			}
			else if (z > oldZ)
			{
				jeepItem->Pose.Position.z -= shiftZ + 1;
				return (pos->x - jeepItem->Pose.Position.x);
			}
			else
			{
				jeepItem->Pose.Position.z += WALL_MASK - shiftZ;
				return (jeepItem->Pose.Position.x - pos->x);
			}
		}

		if (z == oldZ)
		{
			if (x > oldX)
			{
				jeepItem->Pose.Position.x -= shiftX + 1;
				return (jeepItem->Pose.Position.z - pos->z);
			}
			else
			{
				jeepItem->Pose.Position.x += WALL_MASK - shiftX;
				return (pos->z - jeepItem->Pose.Position.z);
			}
		}
		
		x = 0;
		z = 0;

		short roomNumber = jeepItem->RoomNumber;
		FloorInfo* floor = GetFloor(old->x, pos->y, pos->z, &roomNumber);
		int height = GetFloorHeight(floor, old->x, pos->y, pos->z);

		if (height < old->y - CLICK(1))
		{
			if (pos->z > old->z)
				z = -1 - shiftZ;
			else
				z = BLOCK(1) + 1 - shiftZ;
		}

		roomNumber = jeepItem->RoomNumber;
		floor = GetFloor(pos->x, pos->y, old->z, &roomNumber);
		height = GetFloorHeight(floor, pos->x, pos->y, old->z);

		if (height < old->y - CLICK(1))
		{
			if (pos->x > old->x)
				x = -1 - shiftX;
			else
				x = BLOCK(1) + 1 - shiftX;
		}

		if (x && z)
		{
			jeepItem->Pose.Position.z += z;
			jeepItem->Pose.Position.x += x;
		}
		
		if (z)
		{
			jeepItem->Pose.Position.z += z;
			if (z > 0)
				return (jeepItem->Pose.Position.x - pos->x);
			else
				return (pos->x - jeepItem->Pose.Position.x);
		}

		if (x)
		{
			jeepItem->Pose.Position.x += x;
			if (x > 0)
				return (pos->z - jeepItem->Pose.Position.z);
			else
				return (jeepItem->Pose.Position.z - pos->z);
		}

		jeepItem->Pose.Position.z += (old->z - pos->z);
		jeepItem->Pose.Position.x += (old->x - pos->x);

		return 0;
	}

	static int DoJeepDynamics(ItemInfo* laraItem, int height, int verticalVelocity, int* yPos, int flags)
	{
		// Grounded.
		if (height <= *yPos)
		{
			if (flags)
				return verticalVelocity;
			else
			{
				int kick = height - *yPos;

				if (kick < -80)
					kick = -80;

				verticalVelocity += ((kick - verticalVelocity) / 16);

				if (*yPos > height)
					*yPos = height;
			}
		}
		// IsAirborne.
		else
		{
			*yPos += verticalVelocity;

			if (*yPos <= height - 32)
			{
				if (flags)
				{
					verticalVelocity += flags + (flags / 2);
				}
				else
				{
					verticalVelocity += (int)(g_GameFlow->GetSettings()->Physics.Gravity * 1.5f);
				}
			}
			else
			{
				*yPos = height;

				if (verticalVelocity > 150)
					laraItem->HitPoints += 150 - verticalVelocity;

				verticalVelocity = 0;
			}
		}

		return verticalVelocity;
	}

	static bool JeepCanGetOff(ItemInfo* jeepItem, ItemInfo* laraItem)
	{
		auto* lara = GetLaraInfo(laraItem);

		short angle = jeepItem->Pose.Orientation.y + ANGLE(90.0f);

		int x = jeepItem->Pose.Position.x - JEEP_DISMOUNT_DISTANCE * phd_sin(angle);
		int y = jeepItem->Pose.Position.y;
		int z = jeepItem->Pose.Position.z - JEEP_DISMOUNT_DISTANCE * phd_cos(angle);

		auto probe = GetPointCollision(Vector3i(x, y, z), jeepItem->RoomNumber);

		if (probe.IsSteepFloor() || probe.GetFloorHeight() == NO_HEIGHT)
			return false;

		if (abs(probe.GetFloorHeight() - jeepItem->Pose.Position.y) > BLOCK(1 / 2.0f))
			return false;

		if ((probe.GetCeilingHeight() - jeepItem->Pose.Position.y) > -LARA_HEIGHT ||
			(probe.GetFloorHeight() - probe.GetCeilingHeight()) < LARA_HEIGHT)
		{
			return false;
		}

		return true;
	}

	static void TriggerJeepExhaustSmoke(int x, int y, int z, short angle, short speed, int moving)
	{
		auto* spark = GetFreeParticle();

		spark->dR = 16;
		spark->dG = 16;
		spark->on = 1;
		spark->sR = 0;
		spark->sG = 0;
		spark->sB = 0;
		spark->dB = 32;

		if (moving)
		{
			spark->dR = (spark->dR * speed) / 32;
			spark->dG = (spark->dG * speed) / 32;
			spark->dB = (spark->dB * speed) / 32;
		}

		spark->colFadeSpeed = 4;
		spark->fadeToBlack = 4;
		spark->life = spark->sLife = (GetRandomControl() & 3) - (speed / 4096) + 20;

		if (spark->life < 9)
		{
			spark->life = 9;
			spark->sLife = 9;
		}

		spark->blendMode = BlendMode::Additive;
		spark->x = (GetRandomControl() & 0xF) + x - 8;
		spark->y = (GetRandomControl() & 0xF) + y - 8;
		spark->z = (GetRandomControl() & 0xF) + z - 8;
		spark->xVel = speed * phd_sin(angle) / 4;
		spark->yVel = -8 - (GetRandomControl() & 7);
		spark->zVel = speed * phd_cos(angle) / 4;
		spark->friction = 4;

		if (GetRandomControl() & 1)
		{
			spark->flags = 538;
			spark->rotAng = GetRandomControl() & 0xFFF;

			if (GetRandomControl() & 1)
				spark->rotAdd = -24 - (GetRandomControl() & 7);
			else
				spark->rotAdd = (GetRandomControl() & 7) + 24;
		}
		else
			spark->flags = 522;

		spark->scalar = 1;
		spark->gravity = -4 - (GetRandomControl() & 3);
		spark->maxYvel = -8 - (GetRandomControl() & 7);
		spark->dSize = (GetRandomControl() & 7) + (speed / 128) + 32;
		spark->sSize = spark->dSize / 2;
		spark->size = spark->dSize / 2;
	}

	static int JeepCheckGetOff(ItemInfo* laraItem)
	{
		auto* lara = GetLaraInfo(laraItem);

		if (laraItem->Animation.ActiveState == JS_DISMOUNT)
		{
			if (TestLastFrame(*laraItem))
			{
				laraItem->Pose.Orientation.y += ANGLE(90.0f);
				laraItem->Pose.Translate(laraItem->Pose.Orientation.y, -JEEP_DISMOUNT_DISTANCE);
				SetAnimation(*laraItem, LA_STAND_SOLID);
				laraItem->Pose.Orientation.x = 0;
				laraItem->Pose.Orientation.z = 0;
				SetLaraVehicle(laraItem, nullptr);
				lara->Control.HandStatus = HandStatus::Free;
				return false;
			}
		}

		return true;
	}

	static int GetJeepCollisionAnim(ItemInfo* jeepItem, Vector3i* pos)
	{
		auto* jeep = GetJeepInfo(jeepItem);

		if (jeep->Gear != 0)
			return 0;

		pos->x = jeepItem->Pose.Position.x - pos->x;
		pos->z = jeepItem->Pose.Position.z - pos->z;

		if (pos->x || pos->z)
		{
			float sinY = phd_sin(jeepItem->Pose.Orientation.y);
			float cosY = phd_cos(jeepItem->Pose.Orientation.y);

			int front = (pos->z * cosY) + (pos->x * sinY);
			int side = (pos->z * -sinY) + (pos->x * cosY);

			if (abs(front) > abs(side))
				return ((front > 0) + 13);
			else
				return ((side <= 0) + 11);
		}

		return 0;
	}

	int JeepDynamics(ItemInfo* jeepItem, ItemInfo* laraItem)
	{
		auto* jeep = GetJeepInfo(jeepItem);
		auto* lara = GetLaraInfo(laraItem);

		Vector3i f_old, b_old, mm_old, mt_old, mb_old;

		int hf_old  = GetVehicleHeight(jeepItem, JEEP_FRONT, -JEEP_SIDE, true, &f_old);
		int hb_old  = GetVehicleHeight(jeepItem, JEEP_FRONT, JEEP_SIDE, true, &b_old);
		int hmm_old = GetVehicleHeight(jeepItem, -(JEEP_FRONT + 50), -JEEP_SIDE, true, &mm_old);
		int hmt_old = GetVehicleHeight(jeepItem, -(JEEP_FRONT + 50), JEEP_SIDE, true, &mt_old);
		int hmb_old = GetVehicleHeight(jeepItem, -(JEEP_FRONT + 50), 0, true, &mb_old);

		auto oldPos = jeepItem->Pose.Position;

		short rot = 0;
		JeepNoGetOff = 0;

		if (oldPos.y <= jeepItem->Floor - 8)
		{
			if (jeep->TurnRate < -JEEP_TURN_RATE_DECEL)
				jeep->TurnRate += JEEP_TURN_RATE_DECEL;
			else if (jeep->TurnRate > JEEP_TURN_RATE_DECEL)
				jeep->TurnRate -= JEEP_TURN_RATE_DECEL;
			else
				jeep->TurnRate = 0;

			jeepItem->Pose.Orientation.y += (short)(jeep->TurnRate + jeep->ExtraRotation);
			jeep->MomentumAngle += ((short)(jeepItem->Pose.Orientation.y - jeep->MomentumAngle) / 32);
		}
		else
		{
			if (jeep->TurnRate < -ANGLE(1))
				jeep->TurnRate += ANGLE(1);
			else if (jeep->TurnRate > ANGLE(1))
				jeep->TurnRate -= ANGLE(1);
			else
				jeep->TurnRate = 0;

			jeepItem->Pose.Orientation.y += jeep->TurnRate + jeep->ExtraRotation;

			rot = jeepItem->Pose.Orientation.y - jeep->MomentumAngle;
			short momentum = (short)(728 - ((3 * jeep->Velocity) / 2048));

			if (!IsHeld(In::Action) && jeep->Velocity > 0)
				momentum -= (momentum / 4);

			if (rot < -273)
			{
				if (rot < -ANGLE(75))
				{
					jeepItem->Pose.Position.y -= 41;
					jeepItem->Animation.Velocity.y = -6 - (GetRandomControl() & 3);
					jeep->TurnRate = 0;
					jeep->Velocity -= (jeep->Velocity / 8);
				}

				if (rot < -ANGLE(90))
					jeep->MomentumAngle = jeepItem->Pose.Orientation.y + ANGLE(90);
				else
					jeep->MomentumAngle -= momentum;
			}
			else if (rot > 273)
			{
				if (rot > ANGLE(75))
				{
					jeepItem->Pose.Position.y -= 41;
					jeepItem->Animation.Velocity.y = -6 - (GetRandomControl() & 3);
					jeep->TurnRate = 0;
					jeep->Velocity -= (jeep->Velocity / 8);
				}

				if (rot > ANGLE(90))
					jeep->MomentumAngle = jeepItem->Pose.Orientation.y - ANGLE(90);
				else
					jeep->MomentumAngle += momentum;
			}
			else
				jeep->MomentumAngle = jeepItem->Pose.Orientation.y;
		}
		
		short roomNumber = jeepItem->RoomNumber;
		FloorInfo* floor = GetFloor(jeepItem->Pose.Position.x, jeepItem->Pose.Position.y, jeepItem->Pose.Position.z, &roomNumber);
		int height = GetFloorHeight(floor, jeepItem->Pose.Position.x, jeepItem->Pose.Position.y, jeepItem->Pose.Position.z);

		short speed;
		if (jeepItem->Pose.Position.y < height)
			speed = jeepItem->Animation.Velocity.z;
		else
			speed = jeepItem->Animation.Velocity.z * phd_cos(jeepItem->Pose.Orientation.x);

		jeepItem->Pose.Position.x += speed * phd_sin(jeep->MomentumAngle);
		jeepItem->Pose.Position.z += speed * phd_cos(jeep->MomentumAngle);
	
		if (jeepItem->Pose.Position.y >= height)
		{
			short slip = JEEP_SLIP * phd_sin(jeepItem->Pose.Orientation.x);

			if (abs(slip) > 16)
			{
				JeepNoGetOff = 1;

				if (slip < 0)
					jeep->Velocity += SQUARE(slip + 16) / 2;
				else
					jeep->Velocity -= SQUARE(slip - 16) / 2;
			}

			slip = JEEP_SLIP_SIDE * phd_sin(jeepItem->Pose.Orientation.z);

			if (abs(slip) > JEEP_SLIP_SIDE / 4)
			{
				JeepNoGetOff = 1;

				if (slip >= 0)
				{
					jeepItem->Pose.Position.x += (slip - 24) * phd_sin(jeepItem->Pose.Orientation.y + ANGLE(90));
					jeepItem->Pose.Position.z += (slip - 24) * phd_cos(jeepItem->Pose.Orientation.y + ANGLE(90));
				}
				else
				{
					jeepItem->Pose.Position.x += (slip - 24) * phd_sin(jeepItem->Pose.Orientation.y - ANGLE(90));
					jeepItem->Pose.Position.z += (slip - 24) * phd_cos(jeepItem->Pose.Orientation.y - ANGLE(90));
				}
			}
		}

		if (jeep->Velocity > JEEP_VELOCITY_MAX)
			jeep->Velocity = JEEP_VELOCITY_MAX;
		else if (jeep->Velocity < -JEEP_REVERSE_VELOCITY_MAX)
			jeep->Velocity = -JEEP_REVERSE_VELOCITY_MAX;

		Vector3i movedPos;
		movedPos.x = jeepItem->Pose.Position.x;
		movedPos.z = jeepItem->Pose.Position.z;

		if (!(jeepItem->Flags & IFLAG_INVISIBLE))
			DoVehicleCollision(jeepItem, JEEP_FRONT);

		Vector3i f, b, mm, mt, mb;
	
		int rot1 = 0;
		int rot2 = 0;

		int hf = GetVehicleHeight(jeepItem, JEEP_FRONT, -JEEP_SIDE, false, &f);
		if (hf < f_old.y - CLICK(1))
			rot1 = abs(4 * DoJeepShift(jeepItem, &f, &f_old));

		int hmm = GetVehicleHeight(jeepItem, -(JEEP_FRONT + 50), -JEEP_SIDE, false, &mm);
		if (hmm < mm_old.y - CLICK(1))
		{
			if (rot)
				rot1 += abs(4 * DoJeepShift(jeepItem, &mm, &mm_old));
			else
				rot1 = -abs(4 * DoJeepShift(jeepItem, &mm, &mm_old));
		}

		int hb = GetVehicleHeight(jeepItem, JEEP_FRONT, JEEP_SIDE, false, &b);
		if (hb < b_old.y - CLICK(1))
			rot2 = -abs(4 * DoJeepShift(jeepItem, &b, &b_old));

		int hmb = GetVehicleHeight(jeepItem, -(JEEP_FRONT + 50), 0, false, &mb);
		if (hmb < mb_old.y - CLICK(1))
			DoJeepShift(jeepItem, &mb, &mb_old);
	
		int hmt = GetVehicleHeight(jeepItem, -(JEEP_FRONT + 50), JEEP_SIDE, false, &mt);
		if (hmt < mt_old.y - CLICK(1))
		{
			if (rot2)
				rot2 -= abs(4 * DoJeepShift(jeepItem, &mt, &mt_old));
			else
				rot2 = abs(4 * DoJeepShift(jeepItem, &mt, &mt_old));
		}

		if (!rot1)
			rot1 = rot2;
	   
		roomNumber = jeepItem->RoomNumber;
		floor = GetFloor(jeepItem->Pose.Position.x, jeepItem->Pose.Position.y, jeepItem->Pose.Position.z, &roomNumber);
		if (GetFloorHeight(floor, jeepItem->Pose.Position.x, jeepItem->Pose.Position.y, jeepItem->Pose.Position.z) < jeepItem->Pose.Position.y - CLICK(1))
			DoJeepShift(jeepItem, (Vector3i*)&jeepItem->Pose, &oldPos);

		if (!jeep->Velocity)
			rot1 = 0;

		jeep->ExtraRotationDrift = (jeep->ExtraRotationDrift + rot1) / 2;

		if (abs(jeep->ExtraRotationDrift) < 2)
			jeep->ExtraRotationDrift = 0;

		if (abs(jeep->ExtraRotationDrift - jeep->ExtraRotation) < 4)
			jeep->ExtraRotation = jeep->ExtraRotationDrift;
		else
			jeep->ExtraRotation += ((jeep->ExtraRotationDrift - jeep->ExtraRotation) / 4);

		int anim = GetJeepCollisionAnim(jeepItem, &movedPos);
	
		if (anim)
		{
			int newspeed = (jeepItem->Pose.Position.z - oldPos.z) * phd_cos(jeep->MomentumAngle) + (jeepItem->Pose.Position.x - oldPos.x) * phd_sin(jeep->MomentumAngle);
			newspeed *= 256;

			if ((&g_Level.Items[lara->Context.Vehicle] == jeepItem) && (jeep->Velocity == JEEP_VELOCITY_MAX) && (newspeed < (JEEP_VELOCITY_MAX - 10)))
			{
				DoDamage(laraItem, (JEEP_VELOCITY_MAX - newspeed) / 128);
			}

			if (jeep->Velocity > 0 && newspeed < jeep->Velocity)
				jeep->Velocity = (newspeed < 0) ? 0 : newspeed;
			else if (jeep->Velocity < 0 && newspeed > jeep->Velocity)
				jeep->Velocity = (newspeed > 0) ? 0 : newspeed;

			if (jeep->Velocity < -JEEP_REVERSE_VELOCITY_MAX)
				jeep->Velocity = -JEEP_REVERSE_VELOCITY_MAX;
		}

		return anim;
	}

	static int JeepUserControl(ItemInfo* jeepItem, ItemInfo* laraItem, int height, int* pitch)
	{
		auto* jeep = GetJeepInfo(jeepItem);
		auto* lara = GetLaraInfo(laraItem);

		if (laraItem->Animation.ActiveState == JS_DISMOUNT || laraItem->Animation.TargetState == JS_DISMOUNT)
			ClearAllActions();
	
		if (jeep->Revs <= 16)
			jeep->Revs = 0;
		else
		{
			jeep->Velocity += (jeep->Revs / 16);
			jeep->Revs -= (jeep->Revs / 8);
		}

		int rot1 = 0;
		int rot2 = 0;

		if (jeepItem->Pose.Position.y >= height - CLICK(1))
		{
			lara->Control.Look.Mode = (jeepItem->Animation.Velocity.z == 0.0f) ? LookMode::Horizontal : LookMode::Free;

			if (abs(jeep->Velocity) <= JEEP_VELOCITY_MAX / 2)
			{
				rot1 = ANGLE(5) * abs(jeep->Velocity) / 16384;
				rot2 = 60 * abs(jeep->Velocity) / 16384 + ANGLE(1);
			}
			else
			{
				rot1 = ANGLE(5);
				rot2 = 242;
			}

			if (jeep->Velocity < 0)
			{
				if (IsHeld(In::Right))
				{
					jeep->TurnRate -= rot2;
					if (jeep->TurnRate < -rot1)
						jeep->TurnRate = -rot1;
				}
				else if (IsHeld(In::Left))
				{
					jeep->TurnRate += rot2;
					if (jeep->TurnRate > rot1)
						jeep->TurnRate = rot1;
				}
			}
			else if (jeep->Velocity > 0)
			{
				if (IsHeld(In::Right))
				{
					jeep->TurnRate += rot2;
					if (jeep->TurnRate > rot1)
						jeep->TurnRate = rot1;
				}
				else if (IsHeld(In::Left))
				{
					jeep->TurnRate -= rot2;
					if (jeep->TurnRate < -rot1)
						jeep->TurnRate = -rot1;
				}
			}

			// Brake/reverse
			if (IsHeld(In::Brake))
			{
				if (jeep->Velocity < 0)
				{
					jeep->Velocity += 3 * VEHICLE_VELOCITY_SCALE;
					if (jeep->Velocity > 0)
						jeep->Velocity = 0;
				}
				else if (jeep->Velocity > 0)
				{
					jeep->Velocity -= 3 * VEHICLE_VELOCITY_SCALE;
					if (jeep->Velocity < 0)
						jeep->Velocity = 0;
				}
			}
			// Accelerate
			else if (IsHeld(In::Accelerate))
			{
				if (jeep->Gear)
				{
					if (jeep->Gear == 1 && jeep->Velocity > -JEEP_REVERSE_VELOCITY_MAX)
						jeep->Velocity -= (abs(-JEEP_REVERSE_VELOCITY_MAX - jeep->Velocity) / 8) - 2;
					else if (jeep->Gear == 1 && jeep->Velocity < -JEEP_REVERSE_VELOCITY_MAX)
						jeep->Velocity = -JEEP_REVERSE_VELOCITY_MAX;
				}
				else
				{
					if (jeep->Velocity < JEEP_VELOCITY_MAX)
					{
						if (jeep->Velocity < (64 * VEHICLE_VELOCITY_SCALE))
							jeep->Velocity += 8 + (((64 * VEHICLE_VELOCITY_SCALE) + (8 * VEHICLE_VELOCITY_SCALE) - jeep->Velocity) / 8);
						else if (jeep->Velocity < (112 * VEHICLE_VELOCITY_SCALE))
							jeep->Velocity += 4 + (((112 * VEHICLE_VELOCITY_SCALE) + (8 * VEHICLE_VELOCITY_SCALE) - jeep->Velocity) / 16);
						else if (jeep->Velocity < JEEP_VELOCITY_MAX)
							jeep->Velocity += 2 + ((JEEP_VELOCITY_MAX - jeep->Velocity) / 8);
					}
					else
						jeep->Velocity = JEEP_VELOCITY_MAX;
				}

				jeep->Velocity -= (abs(jeepItem->Pose.Orientation.y - jeep->MomentumAngle) / 64);
			}
			else if (jeep->Velocity > (1 * VEHICLE_VELOCITY_SCALE))
				jeep->Velocity -= 1 * VEHICLE_VELOCITY_SCALE;
			else if (jeep->Velocity < -(1 * VEHICLE_VELOCITY_SCALE))
				jeep->Velocity += 1 * VEHICLE_VELOCITY_SCALE;
			else
				jeep->Velocity = 0;

			jeepItem->Animation.Velocity.z = jeep->Velocity / VEHICLE_VELOCITY_SCALE;
			if (jeep->EngineRevs > 0xC000)
				jeep->EngineRevs = (GetRandomControl() & 0x1FF) + 48896;

			int revs = jeep->Velocity;
			if (jeep->Velocity < 0)
				revs /= 2;

			jeep->EngineRevs += (abs(revs) - jeep->EngineRevs) / 8;
		}

		if (IsHeld(In::Brake))
		{
			auto pos = GetJointPosition(jeepItem, 11, Vector3i(0, -144, -1024));
			SpawnDynamicLight(pos.x, pos.y, pos.z, 10, 64, 0, 0);

			jeepItem->MeshBits.Set(17);
			jeepItem->MeshBits.Clear(JeepBrakeLightJoints);
		}
		else
		{
			jeepItem->MeshBits.Set(JeepBrakeLightJoints);
			jeepItem->MeshBits.Clear({ 17 });
		}
	
		*pitch = jeep->EngineRevs;

		return 1;
	}

	static void AnimateJeep(ItemInfo* jeepItem, ItemInfo* laraItem, int collide, int dead)
	{
		auto* jeep = GetJeepInfo(jeepItem);

		bool dismount;
		if (!dead &&
			jeepItem->Pose.Position.y != jeepItem->Floor &&
			laraItem->Animation.ActiveState != JS_JUMP && 
			laraItem->Animation.ActiveState != JS_LAND)
		{
			if (jeep->Gear == 1)
				SetAnimation(*laraItem, ID_JEEP_LARA_ANIMS, JA_BACK_JUMP_START);
			else
				SetAnimation(*laraItem, ID_JEEP_LARA_ANIMS, JA_FWD_JUMP_START);
		}
		else if (collide && !dead &&
				 laraItem->Animation.ActiveState != 4 && 
				 laraItem->Animation.ActiveState != 5 && 
				 laraItem->Animation.ActiveState != 2 &&
				 laraItem->Animation.ActiveState != 3 &&
				 laraItem->Animation.ActiveState != JS_JUMP &&
				 jeep->Velocity > JEEP_CRASH_VELOCITY)
		{
			switch (collide)
			{
			case 13:
				SetAnimation(*laraItem, ID_JEEP_LARA_ANIMS, JA_CRASH_LEFT);
				break;

			case 14:
				SetAnimation(*laraItem, ID_JEEP_LARA_ANIMS, JA_CRASH_FORWARD);
				break;

			case 11:
				SetAnimation(*laraItem, ID_JEEP_LARA_ANIMS, JA_CRASH_RIGHT);
				break;

			case 12:
				SetAnimation(*laraItem, ID_JEEP_LARA_ANIMS, JA_CRASH_BACK);
				break;
			}
		}
		else
		{
			switch (laraItem->Animation.ActiveState)
			{
			case JS_IDLE:
				if (dead)
					laraItem->Animation.TargetState = JS_DEATH;
				else
				{
					dismount = (IsHeld(In::Brake) && IsHeld(In::Left)) ? true : false;
					if (dismount && !jeep->Velocity && !JeepNoGetOff)
					{
						if (JeepCanGetOff(jeepItem, laraItem))
							laraItem->Animation.TargetState = JS_DISMOUNT;

						break;
					}

					if (IsClicked(In::Walk))
					{
						if (jeep->Gear)
							jeep->Gear--;

						break;
					}
					else if (IsClicked(In::Sprint))
					{
						if (jeep->Gear < 1)
						{
							jeep->Gear++;
							if (jeep->Gear == 1)
								laraItem->Animation.TargetState = JS_DRIVE_BACK;

							break;
						}
					}
					else
					{
						if (IsHeld(In::Accelerate) && !IsHeld(In::Brake))
						{
							laraItem->Animation.TargetState = JS_DRIVE_FORWARD;
							break;
						}
						else if (IsHeld(In::Left))
							laraItem->Animation.TargetState = JS_FWD_LEFT;
						else if (IsHeld(In::Right))
							laraItem->Animation.TargetState = JS_FWD_RIGHT;
					}

	/*				if (!(IsClicked(In::Walk)))
					{
						if (!(IsClicked(In::Sprint)))
						{
							if (IsHeld(In::Accelerate) && !IsHeld(In::Brake))
							{
								laraItem->TargetState = JS_DRIVE_FORWARD;
								break;
							}
							else if (IsHeld(In::Left))
								laraItem->TargetState = JS_FWD_LEFT;
							else if (IsHeld(In::Right))
								laraItem->TargetState = JS_FWD_RIGHT;
						}
						else if (jeep->Gear < 1)
						{
							jeep->Gear++;
							if (jeep->Gear == 1)
								laraItem->TargetState = JS_DRIVE_BACK;

						}
					}
					else
					{
						if (jeep->Gear)
							jeep->Gear--;
					}*/
				}

				break;

			case JS_DRIVE_FORWARD:
				if (dead)
					laraItem->Animation.TargetState = JS_IDLE;
				else
				{
					if (jeep->Velocity & 0xFFFFFF00 ||
						(IsHeld(In::Accelerate) || IsHeld(In::Brake)))
					{
						if (IsHeld(In::Brake))
						{
							if (jeep->Velocity <= 21844)
								laraItem->Animation.TargetState = JS_IDLE;
							else
								laraItem->Animation.TargetState = JS_BRAKE;
						}
						else
						{
							if (IsHeld(In::Left))
								laraItem->Animation.TargetState = JS_FWD_LEFT;
							else if (IsHeld(In::Right))
								laraItem->Animation.TargetState = JS_FWD_RIGHT;
						}
					}
					else
						laraItem->Animation.TargetState = JS_IDLE;
				}

				break;

			case 2:
			case 3:
			case 4:
			case 5:
				if (dead)
					laraItem->Animation.TargetState = JS_IDLE;
				else if (IsHeld(In::Accelerate) || IsHeld(In::Brake))
					laraItem->Animation.TargetState = JS_DRIVE_FORWARD;

				break;

			case JS_BRAKE:
				if (dead)
					laraItem->Animation.TargetState = JS_IDLE;
				else if (jeep->Velocity & 0xFFFFFF00)
				{
					if (IsHeld(In::Left))
						laraItem->Animation.TargetState = JS_FWD_LEFT;
					else if (IsHeld(In::Right))
						laraItem->Animation.TargetState = JS_FWD_RIGHT;
				}
				else
					laraItem->Animation.TargetState = JS_IDLE;

				break;

			case JS_FWD_LEFT:
				if (dead)
					laraItem->Animation.TargetState = JS_IDLE;
				else if (!(IsClicked(In::Walk)))
				{
					if (IsClicked(In::Sprint))
					{
						if (jeep->Gear < 1)
						{
							jeep->Gear++;
							if (jeep->Gear == 1)
							{
								SetAnimation(*laraItem, ID_JEEP_LARA_ANIMS, JA_IDLE_REVERSE_RIGHT);
								break;
							}
						}
					}
					else if (IsHeld(In::Right))
						laraItem->Animation.TargetState = JS_DRIVE_FORWARD;
					else if (IsHeld(In::Left))
						laraItem->Animation.TargetState = JS_FWD_LEFT;
					else if (jeep->Velocity)
						laraItem->Animation.TargetState = JS_DRIVE_FORWARD;
					else
						laraItem->Animation.TargetState = JS_IDLE;
				}
				else
				{
					if (jeep->Gear)
						jeep->Gear--;
				}

				if (laraItem->Animation.AnimNumber == JA_FWD_LEFT &&
					!jeep->Velocity)
				{
					SetAnimation(*laraItem, ID_JEEP_LARA_ANIMS, JA_IDLE_RIGHT_START);
				}
				if (laraItem->Animation.AnimNumber == JA_IDLE_RIGHT_START)
				{
					if (jeep->Velocity)
						SetAnimation(*laraItem, ID_JEEP_LARA_ANIMS, JA_FWD_LEFT);
				}

				break;

			case JS_FWD_RIGHT:
				if (dead)
					laraItem->Animation.TargetState = JS_IDLE;

				if (!(IsClicked(In::Walk)))
				{
					if (IsClicked(In::Sprint))
					{
						if (jeep->Gear < 1)
						{
							jeep->Gear++;
							if (jeep->Gear == 1)
							{
								SetAnimation(*laraItem, ID_JEEP_LARA_ANIMS, JA_IDLE_REVERSE_LEFT);
								break;
							}
						}
					}
					else if (IsHeld(In::Left))
						laraItem->Animation.TargetState = JS_DRIVE_FORWARD;
					else if (IsHeld(In::Right))
						laraItem->Animation.TargetState = JS_FWD_RIGHT;
					else if (jeep->Velocity)
						laraItem->Animation.TargetState = JS_DRIVE_FORWARD;
					else
						laraItem->Animation.TargetState = JS_IDLE;
				}
				else
				{
					if (jeep->Gear)
						jeep->Gear--;
				}

				if (laraItem->Animation.AnimNumber == JA_FWD_RIGHT && !jeep->Velocity)
				{
					SetAnimation(*laraItem, ID_JEEP_LARA_ANIMS, JA_IDLE_LEFT_START, 14);
				}
				if (laraItem->Animation.AnimNumber == JA_IDLE_LEFT_START)
				{
					if (jeep->Velocity)
						SetAnimation(*laraItem, ID_JEEP_LARA_ANIMS, JA_FWD_RIGHT);
				}

				break;

			case JS_JUMP:
				if (jeepItem->Pose.Position.y == jeepItem->Floor)
					laraItem->Animation.TargetState = JS_LAND;
				else if (jeepItem->Animation.Velocity.y > 300)
					jeep->Flags |= JEEP_FLAG_FALLING;

				break;

			case JS_BACK:
				if (dead)
					laraItem->Animation.TargetState = JS_DRIVE_BACK;
				else if (abs(jeep->Velocity) & 0xFFFFFF00)
				{
					if (IsHeld(In::Left))
						laraItem->Animation.TargetState = JS_BACK_RIGHT;
					else if (IsHeld(In::Right))
						laraItem->Animation.TargetState = JS_BACK_LEFT;
				}
				else
					laraItem->Animation.TargetState = JS_DRIVE_BACK;

				break;

			case JS_BACK_LEFT:
				if (dead)
					laraItem->Animation.TargetState = JS_DRIVE_BACK;
				else if (!(IsClicked(In::Walk)))
				{
					if (IsClicked(In::Sprint))
					{
						if (jeep->Gear < 1)
							jeep->Gear++;
					}
					else if (IsHeld(In::Right))
						laraItem->Animation.TargetState = JS_BACK_LEFT;
					else
						laraItem->Animation.TargetState = JS_BACK;
				}
				else
				{
					if (jeep->Gear)
					{
						jeep->Gear--;
						if (!jeep->Gear)
						{
							SetAnimation(*laraItem, ID_JEEP_LARA_ANIMS, JA_IDLE_FWD_RIGHT);
							break;
						}
					}
				}

				if (laraItem->Animation.AnimNumber == JA_BACK_LEFT && !jeep->Velocity)
					SetAnimation(*laraItem, ID_JEEP_LARA_ANIMS, JA_IDLE_LEFT_BACK_START, 14);

				if (laraItem->Animation.AnimNumber == JA_IDLE_LEFT_BACK_START)
				{
					if (jeep->Velocity)
						SetAnimation(*laraItem, ID_JEEP_LARA_ANIMS, JA_BACK_LEFT);
				}

				break;

			case JS_BACK_RIGHT:
				if (dead)
				{
					laraItem->Animation.TargetState = JS_DRIVE_BACK;
				}
				else if (!(IsClicked(In::Walk)))
				{
					if (IsClicked(In::Sprint))
					{
						if (jeep->Gear < 1)
							jeep->Gear++;
					}
					else if (IsHeld(In::Left))
						laraItem->Animation.TargetState = JS_BACK_RIGHT;
					else
						laraItem->Animation.TargetState = JS_BACK;

					if (laraItem->Animation.AnimNumber == JA_BACK_RIGHT && !jeep->Velocity)
						SetAnimation(*laraItem, ID_JEEP_LARA_ANIMS, JA_IDLE_RIGHT_BACK_START, 14);

					if (laraItem->Animation.AnimNumber == JA_IDLE_RIGHT_BACK_START)
					{
						if (jeep->Velocity)
							SetAnimation(*laraItem, ID_JEEP_LARA_ANIMS, JA_BACK_RIGHT);
					}

					break;
				}
				else if (!jeep->Gear || (--jeep->Gear != 0))
				{
					if (laraItem->Animation.AnimNumber == JA_BACK_RIGHT && !jeep->Velocity)
						SetAnimation(*laraItem, ID_JEEP_LARA_ANIMS, JA_IDLE_RIGHT_BACK_START, 14);

					if (laraItem->Animation.AnimNumber == JA_IDLE_RIGHT_BACK_START)
					{
						if (jeep->Velocity)
							SetAnimation(*laraItem, ID_JEEP_LARA_ANIMS, JA_BACK_RIGHT);
					}

					break;
				}

				SetAnimation(*laraItem, ID_JEEP_LARA_ANIMS, JA_IDLE_FWD_RIGHT);
				break;

			case JS_DRIVE_BACK:
				if (dead)
					laraItem->Animation.TargetState = JS_IDLE;
				else
	//			if (jeep->Velocity || JeepNoGetOff)
				{
					if (!(IsClicked(In::Walk)))
					{
						if (IsClicked(In::Sprint))
						{
							if (jeep->Gear < 1)
								jeep->Gear++;
						}
						else if (!IsHeld(In::Accelerate) || IsHeld(In::Brake))
						{
							if (IsHeld(In::Right))
								laraItem->Animation.TargetState = JS_BACK_RIGHT;
							else if (IsHeld(In::Left))
								laraItem->Animation.TargetState = JS_BACK_LEFT;
						}
						else
							laraItem->Animation.TargetState = JS_BACK;
					}
					else
					{
						if (jeep->Gear)
						{
							jeep->Gear--;
							if (!jeep->Gear)
								laraItem->Animation.TargetState = JS_IDLE;
						}
					}
				}

				break;

			default:
				break;
			}
		}
	}

	int JeepControl(ItemInfo* laraItem)
	{
		auto* lara = GetLaraInfo(laraItem);
		auto* jeepItem = &g_Level.Items[lara->Context.Vehicle];
		auto* jeep = GetJeepInfo(jeepItem);

		int drive = -1;
		bool dead = 0;

		int collide = JeepDynamics(jeepItem, laraItem);

		short roomNumber = jeepItem->RoomNumber;
		FloorInfo* floor = GetFloor(jeepItem->Pose.Position.x, jeepItem->Pose.Position.y, jeepItem->Pose.Position.z, &roomNumber);

		int floorHeight = GetFloorHeight(floor, jeepItem->Pose.Position.x, jeepItem->Pose.Position.y, jeepItem->Pose.Position.z);
		int ceiling = GetCeiling(floor, jeepItem->Pose.Position.x, jeepItem->Pose.Position.y, jeepItem->Pose.Position.z);

		Vector3i fl, fr, bc;
		int hfl = GetVehicleHeight(jeepItem, JEEP_FRONT, -JEEP_SIDE, true, &fl);
		int hfr = GetVehicleHeight(jeepItem, JEEP_FRONT, JEEP_SIDE, true, &fr);
		int hbc = GetVehicleHeight(jeepItem, -(JEEP_FRONT + 50), 0, true, &bc);

		roomNumber = jeepItem->RoomNumber;
		floor = GetFloor(jeepItem->Pose.Position.x, jeepItem->Pose.Position.y, jeepItem->Pose.Position.z, &roomNumber);
		floorHeight = GetFloorHeight(floor, jeepItem->Pose.Position.x, jeepItem->Pose.Position.y, jeepItem->Pose.Position.z);

		TestTriggers(jeepItem, true);
		TestTriggers(jeepItem, false);

		if (laraItem->HitPoints <= 0)
		{
			dead = true;
			ClearAction(In::Forward);
			ClearAction(In::Back);
			ClearAction(In::Left);
			ClearAction(In::Right);
		}

		int pitch = 0;
		if (jeep->Flags)
			collide = 0;
		else if (laraItem->Animation.ActiveState == JS_MOUNT)
		{
			drive = -1;
			collide = 0;
		}
		else
		{
			drive = JeepUserControl(jeepItem, laraItem, floorHeight, &pitch);
			HandleVehicleSpeedometer(jeepItem->Animation.Velocity.z, JEEP_VELOCITY_MAX / (float)VEHICLE_VELOCITY_SCALE);
		}

		if (jeep->Velocity || jeep->Revs)
		{
			jeep->Pitch = pitch;
			if (pitch >= -32768)
			{
				if (pitch > 40960)
					jeep->Pitch = 40960;
			}
			else
				jeep->Pitch = -32768;

			SoundEffect(SFX_TR4_VEHICLE_JEEP_MOVING, &jeepItem->Pose, SoundEnvironment::Land, 0.5f + jeep->Pitch / 65535.0f);
		}
		else
		{
			if (drive != -1)
				SoundEffect(SFX_TR4_VEHICLE_JEEP_IDLE, &jeepItem->Pose);
			jeep->Pitch = 0;
		}

		jeepItem->Floor = floorHeight;
		short rotAdd = jeep->Velocity / 4;
		jeep->FrontRightWheelRotation -= rotAdd;
		jeep->FrontLeftWheelRotation -= rotAdd;
		jeep->BackRightWheelRotation -= rotAdd;
		jeep->BackLeftWheelRotation -= rotAdd;

		int oldY = jeepItem->Pose.Position.y;
		jeepItem->Animation.Velocity.y = DoJeepDynamics(laraItem, floorHeight, jeepItem->Animation.Velocity.y, &jeepItem->Pose.Position.y, 0);
		jeep->Velocity = DoVehicleWaterMovement(jeepItem, laraItem, jeep->Velocity, JEEP_FRONT, &jeep->TurnRate, JEEP_WAKE_OFFSET);

		short xRot;
		floorHeight = (fl.y + fr.y) / 2;

		if (bc.y < hbc)
		{
			if (floorHeight < (hfl + hfr) / 2)
			{
				xRot = phd_atan(137, oldY - jeepItem->Pose.Position.y);
				if (jeep->Velocity < 0)
					xRot = -xRot;
			}
			else
				xRot = phd_atan(JEEP_FRONT, jeepItem->Pose.Position.y - floorHeight);
		}
		else
		{
			if (floorHeight < (hfl + hfr) / 2)
				xRot = phd_atan(JEEP_FRONT, hbc - jeepItem->Pose.Position.y);
			else
				xRot = phd_atan(1100, hbc - floorHeight);
		}

		short zRot = phd_atan(350, floorHeight - fl.y);
		jeepItem->Pose.Orientation.x += (xRot - jeepItem->Pose.Orientation.x) / 4;
		jeepItem->Pose.Orientation.z += (zRot - jeepItem->Pose.Orientation.z) / 4;

		if (!(jeep->Flags & JEEP_FLAG_DEAD))
		{
			if (roomNumber != jeepItem->RoomNumber)
			{
				ItemNewRoom(lara->Context.Vehicle, roomNumber);
				ItemNewRoom(laraItem->Index, roomNumber);
			}

			laraItem->Pose = jeepItem->Pose;

			AnimateJeep(jeepItem, laraItem, collide, dead);
			AnimateItem(*laraItem);
			SyncVehicleAnim(*jeepItem, *laraItem);

			Camera.targetElevation = -ANGLE(30.0f);
			Camera.targetDistance = BLOCK(2);

			if (jeep->Gear == 1)
				jeep->CameraElevation += ((32578 - jeep->CameraElevation) / 8);
			else
				jeep->CameraElevation -= (jeep->CameraElevation / 8);

			Camera.targetAngle = jeep->CameraElevation;

			if (jeep->Flags & JEEP_FLAG_FALLING && jeepItem->Pose.Position.y == jeepItem->Floor)
			{
				laraItem->MeshBits = 0;
				ExplodeVehicle(laraItem, jeepItem);
				return 0;
			}
		}

		if (laraItem->Animation.ActiveState == JS_MOUNT ||
			laraItem->Animation.ActiveState == JS_DISMOUNT)
		{
			JeepSmokeStart = 0;
		}
		else
		{
			short speed = 0;
			short angle = 0;

			auto pos = GetJointPosition(jeepItem, 11, Vector3i(90, 0, -500));

			if (jeepItem->Animation.Velocity.z <= 32)
			{
				if (JeepSmokeStart >= 16)
				{
					if (GetRandomControl() & 3)
						speed = 0;
					else
						speed = ((GetRandomControl() & 0xF) + GetRandomControl() & 0x10) * 64;
				}
				else
				{
					speed = ((GetRandomControl() & 7) + GetRandomControl() & 0x10 + 2 * JeepSmokeStart) * 64;
					JeepSmokeStart++;
				}

				TriggerJeepExhaustSmoke(pos.x, pos.y, pos.z, jeepItem->Pose.Orientation.y + -32768, speed, 0);
			}
			else if (jeepItem->Animation.Velocity.z < 64)
				TriggerJeepExhaustSmoke(pos.x, pos.y, pos.z, jeepItem->Pose.Orientation.y - 32768, 64 - jeepItem->Animation.Velocity.z, 1);
		}

		return JeepCheckGetOff(laraItem);
	}
}
