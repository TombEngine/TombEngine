#include "framework.h"
#include "Objects/TR3/Vehicles/rubber_boat.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/Point.h"
#include "Game/effects/Bubble.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Setup.h"
#include "Objects/TR3/Vehicles/rubber_boat_info.h"
#include "Objects/TR3/Vehicles/upv.h"
#include "Objects/Utils/VehicleHelpers.h"
#include "Renderer/RendererEnums.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Sound/sound.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Effects::Bubble;
using namespace TEN::Input;

namespace TEN::Entities::Vehicles
{
	constexpr auto RBOAT_RADIUS = 500;
	constexpr auto RBOAT_FRONT = 750;
	constexpr auto RBOAT_SIDE = 300;
	constexpr auto RBOAT_SLIP = 10;
	constexpr auto RBOAT_SIDE_SLIP = 30;
	constexpr auto RBOAT_MOUNT_DISTANCE = CLICK(2.25f);

	constexpr auto RBOAT_VELOCITY_ACCEL = 5;
	constexpr auto RBOAT_VELOCITY_DECEL = 1;
	constexpr auto RBOAT_VELOCITY_BRAKE_DECEL = 5;
	constexpr auto RBOAT_REVERSE_VELOCITY_DECEL = 2;

	constexpr auto RBOAT_VELOCITY_MIN = 20;
	constexpr auto RBOAT_SLOW_VELOCITY_MAX = 37;
	constexpr auto RBOAT_NORMAL_VELOCITY_MAX = 110;
	constexpr auto RBOAT_FAST_VELOCITY_MAX = 185;
	constexpr auto RBOAT_REVERSE_VELOCITY_MAX = 20;

	constexpr auto RBOAT_TURN_RATE_ACCEL = ANGLE(0.25f / 2);
	constexpr auto RBOAT_TURN_RATE_DECEL = ANGLE(0.25f);
	constexpr auto RBOAT_TURN_RATE_MAX	 = ANGLE(4.0f);

	constexpr auto RBOAT_WAKE_OFFSET = Vector3(RBOAT_SIDE * 1.1f, 0.0f, RBOAT_FRONT / 2);

	const std::vector<VehicleMountType> RubberBoatMountTypes =
	{
		VehicleMountType::LevelStart,
		VehicleMountType::Left,
		VehicleMountType::Right,
		VehicleMountType::Jump
	};

	enum RubberBoatState
	{
		RBOAT_STATE_MOUNT = 0,
		RBOAT_STATE_IDLE = 1,
		RBOAT_STATE_MOVING = 2,
		RBOAT_STATE_JUMP_RIGHT = 3,
		RBOAT_STATE_JUMP_LEFT = 4,
		RBOAT_STATE_HIT = 5,
		RBOAT_STATE_FALL = 6,
		RBOAT_STATE_TURN_RIGHT = 7,
		RBOAT_STATE_DEATH = 8,
		RBOAT_STATE_TURN_LEFT = 9
	};

	enum RubberBoatAnim
	{
		RBOAT_ANIM_MOUNT_LEFT = 0,
		RBOAT_ANIM_IDLE = 1,
		RBOAT_ANIM_UNK_1 = 2,
		RBOAT_ANIM_UNK_2 = 3,
		RBOAT_ANIM_UNK_3 = 4,
		RBOAT_ANIM_DISMOUNT_LEFT = 5,
		RBOAT_ANIM_MOUNT_JUMP = 6,
		RBOAT_ANIM_DISMOUNT_RIGHT = 7,
		RBOAT_ANIM_MOUNT_RIGHT = 8,
		RBOAT_ANIM_TURN_LEFT_CONTINUE = 9,
		RBOAT_ANIM_TURN_LEFT_END = 10,
		RBOAT_ANIM_HIT_RIGHT = 11,
		RBOAT_ANIM_HIT_LEFT = 12,
		RBOAT_ANIM_HIT_FRONT = 13,
		RBOAT_ANIM_HIT_BACK = 14,
		RBOAT_ANIM_LEAP_START = 15,
		RBOAT_ANIM_LEAP_CONTINUE = 16,
		RBOAT_ANIM_LEAP_END = 17,
		RBOAT_ANIM_IDLE_DEATH = 18,
		RBOAT_ANIM_TURN_RIGHT_CONTINUE = 19,
		RBOAT_ANIM_TURN_RIGHT_END = 20,
		RBOAT_ANIM_TURN_LEFT_START = 21,
		RBOAT_ANIM_TURN_RIGHT_START = 22
	};

	RubberBoatInfo* GetRubberBoatInfo(ItemInfo* rBoatItem)
	{
		return (RubberBoatInfo*)rBoatItem->Data;
	}

	void InitializeRubberBoat(short itemNumber)
	{
		auto* rBoatItem = &g_Level.Items[itemNumber];
		rBoatItem->Data = RubberBoatInfo();
		auto* rBoat = GetRubberBoatInfo(rBoatItem);
	}

	void RubberBoatPlayerCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* rBoatItem = &g_Level.Items[itemNumber];
		auto* lara = GetLaraInfo(laraItem);

		if (laraItem->HitPoints < 0 || lara->Context.Vehicle != NO_VALUE)
			return;

		auto mountType = GetVehicleMountType(rBoatItem, laraItem, coll, RubberBoatMountTypes, RBOAT_MOUNT_DISTANCE, LARA_HEIGHT);
		if (mountType == VehicleMountType::None)
		{
			coll->Setup.EnableObjectPush = true;
			ObjectCollision(itemNumber, laraItem, coll);
		}
		else
		{
			lara->Context.Vehicle = itemNumber;
			DoRubberBoatMount(rBoatItem, laraItem, mountType);

			if (g_Level.Items[itemNumber].Status != ITEM_ACTIVE)
			{
				AddActiveItem(itemNumber);
				g_Level.Items[itemNumber].Status = ITEM_ACTIVE;
			}
		}
	}

	void DoRubberBoatMount(ItemInfo* rBoatItem, ItemInfo* laraItem, VehicleMountType mountType)
	{
		auto* lara = GetLaraInfo(laraItem);

		switch (mountType)
		{
		default:
		case VehicleMountType::LevelStart:
			SetAnimation(*laraItem, ID_RUBBER_BOAT_LARA_ANIMS, RBOAT_ANIM_IDLE);
			break;

		case VehicleMountType::Left:
			SetAnimation(*laraItem, ID_RUBBER_BOAT_LARA_ANIMS, RBOAT_ANIM_MOUNT_LEFT);
			break;

		case VehicleMountType::Right:
			SetAnimation(*laraItem, ID_RUBBER_BOAT_LARA_ANIMS, RBOAT_ANIM_MOUNT_RIGHT);
			break;

		case VehicleMountType::Jump:
			SetAnimation(*laraItem, ID_RUBBER_BOAT_LARA_ANIMS, RBOAT_ANIM_MOUNT_JUMP);
			break;
		}

		if (laraItem->RoomNumber != rBoatItem->RoomNumber)
			ItemNewRoom(laraItem->Index, rBoatItem->RoomNumber);

		laraItem->Pose.Position = rBoatItem->Pose.Position;
		laraItem->Pose.Position.y -= 5;
		laraItem->Pose.Orientation = EulerAngles(0, rBoatItem->Pose.Orientation.y, 0);
		laraItem->Animation.IsAirborne = false;
		laraItem->Animation.Velocity.z = 0;
		laraItem->Animation.Velocity.y = 0;
		lara->Control.WaterStatus = WaterStatus::Dry;

		AnimateItem(*laraItem);
	}

	void DrawRubberBoat(ItemInfo* rBoatItem)
	{
		/* TODO: WTF?
		RUBBER_BOAT_INFO *b;

		b = item->data;
		item->data = &b->propRot;
		DrawAnimatingItem(item);
		item->data = b;
		*/
	}

	static void DoRubberBoatShift(int itemNumber, ItemInfo* laraItem)
	{
		auto* boatItem = &g_Level.Items[itemNumber];
		auto* lara = GetLaraInfo(laraItem);

		int itemNumber2 = g_Level.Rooms[boatItem->RoomNumber].itemNumber;
		while (itemNumber2 != NO_VALUE)
		{
			auto* item = &g_Level.Items[itemNumber2];

			if (item->ObjectNumber == ID_RUBBER_BOAT && itemNumber2 != itemNumber && lara->Context.Vehicle != itemNumber2)
			{
				int x = item->Pose.Position.x - boatItem->Pose.Position.x;
				int z = item->Pose.Position.z - boatItem->Pose.Position.z;

				int distance = pow(x, 2) + pow(z, 2);
				if (distance < 1000000)
				{
					boatItem->Pose.Position.x = item->Pose.Position.x - x * 1000000 / distance;
					boatItem->Pose.Position.z = item->Pose.Position.z - z * 1000000 / distance;
				}

				return;
			}

			itemNumber2 = item->NextItem;
		}
	}

	static int DoRubberBoatShift2(ItemInfo* rBoatItem, Vector3i* pos, Vector3i* old)
	{
		int x = pos->x / BLOCK(1);
		int z = pos->z / BLOCK(1);

		int xOld = old->x / BLOCK(1);
		int zOld = old->z / BLOCK(1);

		int xShift = pos->x & WALL_MASK;
		int zShift = pos->z & WALL_MASK;

		if (x == xOld)
		{
			if (z == zOld)
			{
				rBoatItem->Pose.Position.z += (old->z - pos->z);
				rBoatItem->Pose.Position.x += (old->x - pos->x);
			}
			else if (z > zOld)
			{
				rBoatItem->Pose.Position.z -= zShift + 1;
				return (pos->x - rBoatItem->Pose.Position.x);
			}
			else
			{
				rBoatItem->Pose.Position.z += BLOCK(1) - zShift;
				return (rBoatItem->Pose.Position.x - pos->x);
			}
		}
		else if (z == zOld)
		{
			if (x > xOld)
			{
				rBoatItem->Pose.Position.x -= xShift + 1;
				return (rBoatItem->Pose.Position.z - pos->z);
			}
			else
			{
				rBoatItem->Pose.Position.x += BLOCK(1) - xShift;
				return (pos->z - rBoatItem->Pose.Position.z);
			}
		}
		else
		{
			x = 0;
			z = 0;

			int height = GetPointCollision(Vector3i(old->x, pos->y, pos->z), rBoatItem->RoomNumber).GetFloorHeight();
			if (height < (old->y - CLICK(1)))
			{
				if (pos->z > old->z)
					z = -zShift - 1;
				else
					z = BLOCK(1) - zShift;
			}

			height = GetPointCollision(Vector3i(pos->x, pos->y, old->z), rBoatItem->RoomNumber).GetFloorHeight();
			if (height < (old->y - CLICK(1)))
			{
				if (pos->x > old->x)
					x = -xShift - 1;
				else
					x = BLOCK(1) - xShift;
			}

			if (x && z)
			{
				rBoatItem->Pose.Position.z += z;
				rBoatItem->Pose.Position.x += x;
			}
			else if (z)
			{
				rBoatItem->Pose.Position.z += z;
				if (z > 0)
					return (rBoatItem->Pose.Position.x - pos->x);
				else
					return (pos->x - rBoatItem->Pose.Position.x);
			}
			else if (x)
			{
				rBoatItem->Pose.Position.x += x;
				if (x > 0)
					return (pos->z - rBoatItem->Pose.Position.z);
				else
					return (rBoatItem->Pose.Position.z - pos->z);
			}
			else
			{
				rBoatItem->Pose.Position.z += (old->z - pos->z);
				rBoatItem->Pose.Position.x += (old->x - pos->x);
			}
		}

		return 0;
	}

	static int GetRubberBoatCollisionAnim(ItemInfo* rBoatItem, Vector3i* moved)
	{
		moved->x = rBoatItem->Pose.Position.x - moved->x;
		moved->z = rBoatItem->Pose.Position.z - moved->z;

		if (moved->x || moved->z)
		{
			float sinY = phd_sin(rBoatItem->Pose.Orientation.y);
			float cosY = phd_cos(rBoatItem->Pose.Orientation.y);

			long front = (moved->z * cosY) + (moved->x * sinY);
			long side = (moved->z * -sinY) + (moved->x * cosY);

			if (abs(front) > abs(side))
			{
				if (front > 0)
					return RBOAT_ANIM_HIT_BACK;
				else
					return RBOAT_ANIM_HIT_FRONT;
			}
			else
			{
				if (side > 0)
					return RBOAT_ANIM_HIT_RIGHT;
				else
					return RBOAT_ANIM_HIT_LEFT;
			}
		}

		return 0;
	}

	static int RubberBoatDynamics(short itemNumber, ItemInfo* laraItem)
	{
		auto* rBoatItem = &g_Level.Items[itemNumber];
		auto* rBoat = GetRubberBoatInfo(rBoatItem);
		auto* lara = GetLaraInfo(laraItem);

		rBoatItem->Pose.Orientation.z -= rBoat->LeanAngle;

		Vector3i frontLeftOld, frontRightOld, backLeftOld, backRightOld, frontOld;
		int heightFrontLeftOld = GetVehicleWaterHeight(rBoatItem, RBOAT_FRONT, -RBOAT_SIDE, true, &frontLeftOld);
		int heightFrontRightOld = GetVehicleWaterHeight(rBoatItem, RBOAT_FRONT, RBOAT_SIDE, true, &frontRightOld);
		int heightBackLeftOld = GetVehicleWaterHeight(rBoatItem, -RBOAT_FRONT, -RBOAT_SIDE, true, &backLeftOld);
		int heightBackRightOld = GetVehicleWaterHeight(rBoatItem, -RBOAT_FRONT, RBOAT_SIDE, true, &backRightOld);
		int heightFrontOld = GetVehicleWaterHeight(rBoatItem, 1000, 0, true, &frontOld);
	
		Vector3i old;
		old.x = rBoatItem->Pose.Position.x;
		old.y = rBoatItem->Pose.Position.y;
		old.z = rBoatItem->Pose.Position.z;

		rBoatItem->Pose.Orientation.y += rBoat->TurnRate + rBoat->ExtraRotation;
		rBoat->LeanAngle = rBoat->TurnRate * 6;

		rBoatItem->Pose.Position.z += rBoatItem->Animation.Velocity.z * phd_cos(rBoatItem->Pose.Orientation.y);
		rBoatItem->Pose.Position.x += rBoatItem->Animation.Velocity.z * phd_sin(rBoatItem->Pose.Orientation.y);
		if (rBoatItem->Animation.Velocity.z >= 0)
			rBoat->PropellerRotation += (rBoatItem->Animation.Velocity.z * ANGLE(3.0f)) + ANGLE(2.0f);
		else
			rBoat->PropellerRotation += ANGLE(33.0f);

		int slip = RBOAT_SIDE_SLIP * phd_sin(rBoatItem->Pose.Orientation.z);
		if (!slip && rBoatItem->Pose.Orientation.z)
			slip = (rBoatItem->Pose.Orientation.z > 0) ? 1 : -1;

		rBoatItem->Pose.Position.z -= slip * phd_sin(rBoatItem->Pose.Orientation.y);
		rBoatItem->Pose.Position.x += slip * phd_cos(rBoatItem->Pose.Orientation.y);

		slip = RBOAT_SLIP * phd_sin(rBoatItem->Pose.Orientation.x);
		if (!slip && rBoatItem->Pose.Orientation.x)
			slip = (rBoatItem->Pose.Orientation.x > 0) ? 1 : -1;

		rBoatItem->Pose.Position.z -= slip * phd_cos(rBoatItem->Pose.Orientation.y);
		rBoatItem->Pose.Position.x -= slip * phd_sin(rBoatItem->Pose.Orientation.y);

		Vector3i moved;
		moved.x = rBoatItem->Pose.Position.x;
		moved.z = rBoatItem->Pose.Position.z;

		DoRubberBoatShift(itemNumber, laraItem);

		Vector3i frontLeft, frontRight, backRight, backLeft, front;
		short rotation = 0;

		int heightBackLeft = GetVehicleWaterHeight(rBoatItem, -RBOAT_FRONT, -RBOAT_SIDE, false, &backLeft);
		if (heightBackLeft < (backLeftOld.y - CLICK(0.5f)))
			rotation = DoRubberBoatShift2(rBoatItem, &backLeft, &backLeftOld);

		int heightBackRight = GetVehicleWaterHeight(rBoatItem, -RBOAT_FRONT, RBOAT_SIDE, false, &backRight);
		if (heightBackRight < (backRightOld.y - CLICK(0.5f)))
			rotation += DoRubberBoatShift2(rBoatItem, &backRight, &backRightOld);

		int heightFrontLeft = GetVehicleWaterHeight(rBoatItem, RBOAT_FRONT, -RBOAT_SIDE, false, &frontLeft);
		if (heightFrontLeft < (frontLeftOld.y - CLICK(0.5f)))
			rotation += DoRubberBoatShift2(rBoatItem, &frontLeft, &frontLeftOld);

		int heightFrontRight = GetVehicleWaterHeight(rBoatItem, RBOAT_FRONT, RBOAT_SIDE, false, &frontRight);
		if (heightFrontRight < (frontRightOld.y - CLICK(0.5f)))
			rotation += DoRubberBoatShift2(rBoatItem, &frontRight, &frontRightOld);

		if (!slip)
		{
			int heightFront = GetVehicleWaterHeight(rBoatItem, 1000, 0, false, &front);
			if (heightFront < (frontOld.y - CLICK(0.5f)))
				DoRubberBoatShift2(rBoatItem, &front, &frontOld);
		}

		short roomNumber = rBoatItem->RoomNumber;
		auto floor = GetFloor(rBoatItem->Pose.Position.x, rBoatItem->Pose.Position.y, rBoatItem->Pose.Position.z, &roomNumber);
		int height = GetPointCollision(rBoatItem->Pose.Position, roomNumber).GetWaterTopHeight();

		if (height == NO_HEIGHT)
			height = GetFloorHeight(floor, rBoatItem->Pose.Position.x, rBoatItem->Pose.Position.y, rBoatItem->Pose.Position.z);

		if (height < (rBoatItem->Pose.Position.y - CLICK(0.5f)))
			DoRubberBoatShift2(rBoatItem, (Vector3i*)&rBoatItem->Pose, &old);

		DoVehicleCollision(rBoatItem, RBOAT_RADIUS);

		rBoat->ExtraRotation = rotation;
		int collide = GetRubberBoatCollisionAnim(rBoatItem, &moved);

		if (slip || collide)
		{
			int newVelocity = (rBoatItem->Pose.Position.z - old.z) * phd_cos(rBoatItem->Pose.Orientation.y) + (rBoatItem->Pose.Position.x - old.x) * phd_sin(rBoatItem->Pose.Orientation.y);

			if (lara->Context.Vehicle == itemNumber &&
				rBoatItem->Animation.Velocity.z > (RBOAT_NORMAL_VELOCITY_MAX + RBOAT_VELOCITY_ACCEL) &&
				newVelocity < rBoatItem->Animation.Velocity.z - 10)
			{
				DoDamage(laraItem, rBoatItem->Animation.Velocity.z);
				SoundEffect(SFX_TR4_LARA_INJURY, &laraItem->Pose);
				newVelocity /= 2;
				rBoatItem->Animation.Velocity.z /= 2;
			}

			if (slip)
			{
				if (rBoatItem->Animation.Velocity.z <= RBOAT_NORMAL_VELOCITY_MAX + 10)
					rBoatItem->Animation.Velocity.z = newVelocity;
			}
			else
			{
				if (rBoatItem->Animation.Velocity.z > 0 && newVelocity < rBoatItem->Animation.Velocity.z)
					rBoatItem->Animation.Velocity.z = newVelocity;
				else if (rBoatItem->Animation.Velocity.z < 0 && newVelocity > rBoatItem->Animation.Velocity.z)
					rBoatItem->Animation.Velocity.z = newVelocity;
			}

			if (rBoatItem->Animation.Velocity.z < -RBOAT_REVERSE_VELOCITY_MAX)
				rBoatItem->Animation.Velocity.z = -RBOAT_REVERSE_VELOCITY_MAX;
		}

		return collide;
	}

	static int DoRubberBoatDynamics(int height, int verticalVelocity, int* y)
	{
		if (height > *y)
		{
			*y += verticalVelocity;
			if (*y > height)
			{
				*y = height;
				verticalVelocity = 0;
			}
			else
			{
				verticalVelocity += g_GameFlow->GetSettings()->Physics.Gravity;
			}
		}
		else
		{
			verticalVelocity += (height - *y - verticalVelocity) / 8;
			if (verticalVelocity < -20)
				verticalVelocity = -20;

			if (*y > height)
				*y = height;
		}

		return verticalVelocity;
	}

	bool RubberBoatUserControl(ItemInfo* rBoatItem, ItemInfo* laraItem)
	{
		auto* rBoat = GetRubberBoatInfo(rBoatItem);
		auto* lara = GetLaraInfo(laraItem);

		bool noTurn = true;

		if (rBoatItem->Pose.Position.y >= (rBoat->Water - 128) && 
			rBoat->Water != NO_HEIGHT)
		{
			if (!IsHeld(In::Brake) && !IsHeld(In::Look) || rBoatItem->Animation.Velocity.z)
			{
				if ((IsHeld(In::Left) && !IsHeld(In::Reverse)) ||
					(IsHeld(In::Right) && IsHeld(In::Reverse)))
				{
					if (rBoat->TurnRate > 0)
						rBoat->TurnRate -= RBOAT_TURN_RATE_DECEL;
					else
					{
						rBoat->TurnRate -= RBOAT_TURN_RATE_ACCEL;
						if (rBoat->TurnRate < -RBOAT_TURN_RATE_MAX)
							rBoat->TurnRate = -RBOAT_TURN_RATE_MAX;
					}

					noTurn = false;
				}
				else if ((IsHeld(In::Right) && !IsHeld(In::Reverse)) ||
					(IsHeld(In::Left) && IsHeld(In::Reverse)))
				{
					if (rBoat->TurnRate < 0)
						rBoat->TurnRate += RBOAT_TURN_RATE_DECEL;
					else
					{
						rBoat->TurnRate += RBOAT_TURN_RATE_ACCEL;
						if (rBoat->TurnRate > RBOAT_TURN_RATE_MAX)
							rBoat->TurnRate = RBOAT_TURN_RATE_MAX;
					}

					noTurn = false;
				}

				if (IsHeld(In::Reverse))
				{
					if (rBoatItem->Animation.Velocity.z > 0)
						rBoatItem->Animation.Velocity.z -= RBOAT_VELOCITY_BRAKE_DECEL;
					else if (rBoatItem->Animation.Velocity.z > -RBOAT_REVERSE_VELOCITY_MAX)
						rBoatItem->Animation.Velocity.z -= RBOAT_REVERSE_VELOCITY_DECEL;
				}
				else if (IsHeld(In::Accelerate))
				{
					int maxVelocity;
					if (IsHeld(In::Faster))
						maxVelocity = RBOAT_FAST_VELOCITY_MAX;
					else
						maxVelocity = (IsHeld(In::Slower)) ? RBOAT_SLOW_VELOCITY_MAX : RBOAT_NORMAL_VELOCITY_MAX;

					if (rBoatItem->Animation.Velocity.z < maxVelocity)
						rBoatItem->Animation.Velocity.z += (RBOAT_VELOCITY_ACCEL / 2 + 1) + (RBOAT_VELOCITY_ACCEL * rBoatItem->Animation.Velocity.z) / (maxVelocity * 2);
					else if (rBoatItem->Animation.Velocity.z > (maxVelocity + RBOAT_VELOCITY_DECEL))
						rBoatItem->Animation.Velocity.z -= RBOAT_VELOCITY_DECEL;

				}
				else if (IsHeld(In::Left) || IsHeld(In::Right) &&
					rBoatItem->Animation.Velocity.z >= 0 &&
					rBoatItem->Animation.Velocity.z < RBOAT_VELOCITY_MIN)
				{
					if (!IsHeld(In::Brake) && rBoatItem->Animation.Velocity.z == 0)
						rBoatItem->Animation.Velocity.z = RBOAT_VELOCITY_MIN;
				}
				else if (rBoatItem->Animation.Velocity.z > RBOAT_VELOCITY_DECEL)
					rBoatItem->Animation.Velocity.z -= RBOAT_VELOCITY_DECEL;
				else
					rBoatItem->Animation.Velocity.z = 0;
			}
			else
			{
				if (IsHeld(In::Left) || IsHeld(In::Right) &&
					rBoatItem->Animation.Velocity.z >= 0 &&
					rBoatItem->Animation.Velocity.z < RBOAT_VELOCITY_MIN)
				{
					if (!IsHeld(In::Brake) && rBoatItem->Animation.Velocity.z == 0)
						rBoatItem->Animation.Velocity.z = RBOAT_VELOCITY_MIN;
				}
				else if (rBoatItem->Animation.Velocity.z > RBOAT_VELOCITY_DECEL)
					rBoatItem->Animation.Velocity.z -= RBOAT_VELOCITY_DECEL;
				else
					rBoatItem->Animation.Velocity.z = 0;

				lara->Control.Look.Mode = (rBoatItem->Animation.Velocity.z == 0.0f) ? LookMode::Horizontal : LookMode::Free;
			}
		}

		return noTurn;
	}

	static bool TestRubberBoatDismount(ItemInfo* laraItem, int direction)
	{
		auto* lara = GetLaraInfo(laraItem);
		auto* sBoatItem = &g_Level.Items[lara->Context.Vehicle];

		short angle;
		if (direction < 0)
			angle = sBoatItem->Pose.Orientation.y - ANGLE(90.0f);
		else
			angle = sBoatItem->Pose.Orientation.y + ANGLE(90.0f);

		int x = sBoatItem->Pose.Position.x + BLOCK(1) * phd_sin(angle);
		int y = sBoatItem->Pose.Position.y;
		int z = sBoatItem->Pose.Position.z + BLOCK(1) * phd_cos(angle);

		auto pointColl = GetPointCollision(Vector3i(x, y, z), sBoatItem->RoomNumber);

		if ((pointColl.GetFloorHeight() - sBoatItem->Pose.Position.y) < -512)
			return false;

		if (pointColl.IsSteepFloor() || pointColl.GetFloorHeight() == NO_HEIGHT)
			return false;

		if ((pointColl.GetCeilingHeight() - sBoatItem->Pose.Position.y) > -LARA_HEIGHT ||
			(pointColl.GetFloorHeight() - pointColl.GetCeilingHeight()) < LARA_HEIGHT)
		{
			return false;
		}

		return true;
	}

	void RubberBoatAnimation(ItemInfo* rBoatItem, ItemInfo* laraItem, int collide)
	{
		auto* rBoat = GetRubberBoatInfo(rBoatItem);

		if (laraItem->HitPoints <= 0)
		{
			if (laraItem->Animation.ActiveState != RBOAT_STATE_DEATH)
				SetAnimation(*laraItem, ID_RUBBER_BOAT_LARA_ANIMS, RBOAT_ANIM_IDLE_DEATH);
		}
		else if (rBoatItem->Pose.Position.y < (rBoat->Water - CLICK(0.5f)) &&
			rBoatItem->Animation.Velocity.y > 0)
		{
			if (laraItem->Animation.ActiveState != RBOAT_STATE_FALL)
				SetAnimation(*laraItem, ID_RUBBER_BOAT_LARA_ANIMS, RBOAT_ANIM_LEAP_START);
		}
		else if (collide)
		{
			if (laraItem->Animation.ActiveState != RBOAT_STATE_HIT)
				SetAnimation(*laraItem, ID_RUBBER_BOAT_LARA_ANIMS, collide);
		}
		else
		{
			switch (laraItem->Animation.ActiveState)
			{
			case RBOAT_STATE_IDLE:
				if (IsHeld(In::Brake))
				{
					if (rBoatItem->Animation.Velocity.z == 0)
					{
						if (IsHeld(In::Right) && TestRubberBoatDismount(laraItem, rBoatItem->Pose.Orientation.y + ANGLE(90.0f)))
							laraItem->Animation.TargetState = RBOAT_STATE_JUMP_RIGHT;
						else if (IsHeld(In::Left) && TestRubberBoatDismount(laraItem, rBoatItem->Pose.Orientation.y - ANGLE(90.0f)))
							laraItem->Animation.TargetState = RBOAT_STATE_JUMP_LEFT;
					}
				}

				if (rBoatItem->Animation.Velocity.z > 0)
					laraItem->Animation.TargetState = RBOAT_STATE_MOVING;

				break;

			case RBOAT_STATE_MOVING:
				if (rBoatItem->Animation.Velocity.z <= 0)
					laraItem->Animation.TargetState = RBOAT_STATE_IDLE;

				if (IsHeld(In::Right))
					laraItem->Animation.TargetState = RBOAT_STATE_TURN_RIGHT;
				else if (IsHeld(In::Left))
					laraItem->Animation.TargetState = RBOAT_STATE_TURN_LEFT;
			
				break;

			case RBOAT_STATE_FALL:
				laraItem->Animation.TargetState = RBOAT_STATE_MOVING;
				break;

			case RBOAT_STATE_TURN_RIGHT:
				if (rBoatItem->Animation.Velocity.z <= 0)
					laraItem->Animation.TargetState = RBOAT_STATE_IDLE;
				else if (!IsHeld(In::Right))
					laraItem->Animation.TargetState = RBOAT_STATE_MOVING;

				break;

			case RBOAT_STATE_TURN_LEFT:
				if (rBoatItem->Animation.Velocity.z <= 0)
					laraItem->Animation.TargetState = RBOAT_STATE_IDLE;
				else if (!IsHeld(In::Left))
					laraItem->Animation.TargetState = RBOAT_STATE_MOVING;

				break;
			}
		}
	}

	static void TriggerRubberBoatMist(long x, long y, long z, long velocity, short angle, long snow)
	{
		auto* sptr = GetFreeParticle();

		sptr->on = 1;
		sptr->sR = 0;
		sptr->sG = 0;
		sptr->sB = 0;

		if (snow)
		{
			sptr->dR = 255;
			sptr->dG = 255;
			sptr->dB = 255;
		}
		else
		{
			sptr->dR = 64;
			sptr->dG = 64;
			sptr->dB = 64;
		}

		sptr->colFadeSpeed = 4 + (GetRandomControl() & 3);
		sptr->fadeToBlack = 12 - (snow * 8);
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
		{
			sptr->flags = SP_SCALE | SP_DEF | SP_EXPDEF;
		}

		if (!snow)
		{
			sptr->scalar = 4;
			sptr->gravity = sptr->maxYvel = 0;

			float size = (GetRandomControl() & 7) + (velocity / 2) + 16;
			sptr->size =
			sptr->sSize = size / 4;
			sptr->dSize = size;
		}
	}

	void DoRubberBoatDismount(ItemInfo* rBoatItem, ItemInfo* laraItem)
	{
		auto* lara = GetLaraInfo(laraItem);

		if ((laraItem->Animation.ActiveState == RBOAT_STATE_JUMP_RIGHT || laraItem->Animation.ActiveState == RBOAT_STATE_JUMP_LEFT) &&
			TestLastFrame(*laraItem))
		{
			if (laraItem->Animation.ActiveState == RBOAT_STATE_JUMP_LEFT)
				laraItem->Pose.Orientation.y -= ANGLE(90.0f);
			else
				laraItem->Pose.Orientation.y += ANGLE(90.0f);

			SetAnimation(*laraItem, LA_JUMP_FORWARD);
			laraItem->Pose.Orientation.x = 0;
			laraItem->Pose.Orientation.z = 0;
			laraItem->Animation.IsAirborne = true;
			laraItem->Animation.Velocity.z = 20;
			laraItem->Animation.Velocity.y = -40;
			lara->Context.Vehicle = NO_VALUE; // Leave vehicle itself active for inertia.

			int x = laraItem->Pose.Position.x + 360 * phd_sin(laraItem->Pose.Orientation.y);
			int y = laraItem->Pose.Position.y - 90;
			int z = laraItem->Pose.Position.z + 360 * phd_cos(laraItem->Pose.Orientation.y);

			auto probe = GetPointCollision(Vector3i(x, y, z), laraItem->RoomNumber);
			if (probe.GetFloorHeight() >= (y - CLICK(1)))
			{
				laraItem->Pose.Position.x = x;
				laraItem->Pose.Position.z = z;

				if (probe.GetRoomNumber() != laraItem->RoomNumber)
					ItemNewRoom(laraItem->Index, probe.GetRoomNumber());
			}
			laraItem->Pose.Position.y = y;

			SetAnimation(*rBoatItem, RBOAT_ANIM_MOUNT_LEFT);
		}
	}

	void RubberBoatControl(short itemNumber)
	{
		auto* rBoatItem = &g_Level.Items[itemNumber];
		auto* rBoat = GetRubberBoatInfo(rBoatItem);
		auto* laraItem = LaraItem;
		auto* lara = GetLaraInfo(laraItem);

		bool noTurn = true;
		bool drive = false;

		int pitch, height, ofs;

		Vector3i frontLeft, frontRight;
		int collide = RubberBoatDynamics(itemNumber, laraItem);
		int heightFrontLeft = GetVehicleWaterHeight(rBoatItem, RBOAT_FRONT, -RBOAT_SIDE, true, &frontLeft);
		int heightFrontRight = GetVehicleWaterHeight(rBoatItem, RBOAT_FRONT, RBOAT_SIDE, true, &frontRight);

		if (lara->Context.Vehicle == itemNumber)
		{
			TestTriggers(rBoatItem, false);
			TestTriggers(rBoatItem, true);
		}

		auto probe = GetPointCollision(*rBoatItem);
		int water = GetPointCollision(rBoatItem->Pose.Position, probe.GetRoomNumber()).GetWaterTopHeight();
		rBoat->Water = water;

		if (lara->Context.Vehicle == itemNumber && laraItem->HitPoints > 0)
		{
			switch (laraItem->Animation.ActiveState)
			{
			case RBOAT_STATE_MOUNT:
			case RBOAT_STATE_JUMP_RIGHT:
			case RBOAT_STATE_JUMP_LEFT:
				break;

			default:
				drive = true;
				noTurn = RubberBoatUserControl(rBoatItem, laraItem);
				break;
			}
		}
		else
		{
			if (rBoatItem->Animation.Velocity.z > RBOAT_VELOCITY_DECEL)
				rBoatItem->Animation.Velocity.z -= RBOAT_VELOCITY_DECEL;
			else
				rBoatItem->Animation.Velocity.z = 0;
		}

		if (noTurn)
		{
			if (rBoat->TurnRate < -RBOAT_TURN_RATE_DECEL)
				rBoat->TurnRate += RBOAT_TURN_RATE_DECEL;
			else if (rBoat->TurnRate > RBOAT_TURN_RATE_DECEL)
				rBoat->TurnRate -= RBOAT_TURN_RATE_DECEL;
			else
				rBoat->TurnRate = 0;
		}

		height = probe.GetFloorHeight();

		rBoatItem->Floor = height - 5;
		if (rBoat->Water == NO_HEIGHT)
			rBoat->Water = height;
		else
			rBoat->Water -= 5;

		rBoat->LeftVerticalVelocity = DoRubberBoatDynamics(heightFrontLeft, rBoat->LeftVerticalVelocity, (int*)&frontLeft.y);
		rBoat->RightVerticalVelocity = DoRubberBoatDynamics(heightFrontRight, rBoat->RightVerticalVelocity, (int*)&frontRight.y);
		ofs = rBoatItem->Animation.Velocity.y;
		rBoatItem->Animation.Velocity.y = DoRubberBoatDynamics(rBoat->Water, rBoatItem->Animation.Velocity.y, (int*)&rBoatItem->Pose.Position.y);

		height = frontLeft.y + frontRight.y;
		if (height < 0)
			height = -(abs(height) / 2);
		else
			height = height / 2;

		short xRot = phd_atan(RBOAT_FRONT, rBoatItem->Pose.Position.y - height);
		short rRot = phd_atan(RBOAT_SIDE, height - frontLeft.y);

		rBoatItem->Pose.Orientation.x += ((xRot - rBoatItem->Pose.Orientation.x) / 2);
		rBoatItem->Pose.Orientation.z += ((rRot - rBoatItem->Pose.Orientation.z) / 2);

		if (!xRot && abs(rBoatItem->Pose.Orientation.x) < 4)
			rBoatItem->Pose.Orientation.x = 0;
		if (!rRot && abs(rBoatItem->Pose.Orientation.z) < 4)
			rBoatItem->Pose.Orientation.z = 0;

		if (lara->Context.Vehicle == itemNumber)
		{
			RubberBoatAnimation(rBoatItem, laraItem, collide);

			if (probe.GetRoomNumber() != rBoatItem->RoomNumber)
			{
				ItemNewRoom(itemNumber, probe.GetRoomNumber());
				ItemNewRoom(laraItem->Index, probe.GetRoomNumber());
			}

			rBoatItem->Pose.Orientation.z += rBoat->LeanAngle;
			laraItem->Pose = rBoatItem->Pose;

			AnimateItem(*laraItem);

			if (laraItem->HitPoints > 0)
				SyncVehicleAnim(*rBoatItem, *laraItem);

			Camera.targetElevation = -ANGLE(20.0f);
			Camera.targetDistance = BLOCK(2);
		}
		else
		{
			if (probe.GetRoomNumber() != rBoatItem->RoomNumber)
				ItemNewRoom(itemNumber, probe.GetRoomNumber());

			rBoatItem->Pose.Orientation.z += rBoat->LeanAngle;
		}

		pitch = rBoatItem->Animation.Velocity.z;
		rBoat->Pitch += ((pitch - rBoat->Pitch) / 4);

		if (rBoatItem->Animation.Velocity.z > 8)
			SoundEffect(SFX_TR3_VEHICLE_RUBBERBOAT_MOVING, &rBoatItem->Pose, SoundEnvironment::Land, 0.5f + (float)abs(rBoat->Pitch) / (float)RBOAT_NORMAL_VELOCITY_MAX);
		else if (drive)
			SoundEffect(SFX_TR3_VEHICLE_RUBBERBOAT_IDLE, &rBoatItem->Pose, SoundEnvironment::Land, 0.5f + (float)abs(rBoat->Pitch) / (float)RBOAT_NORMAL_VELOCITY_MAX);

		if (lara->Context.Vehicle != itemNumber)
			return;

		DoRubberBoatDismount(rBoatItem, laraItem);

		short probedRoomNumber = GetPointCollision(Vector3i(rBoatItem->Pose.Position.x, rBoatItem->Pose.Position.y + 128, rBoatItem->Pose.Position.z), rBoatItem->RoomNumber).GetRoomNumber();
		height = GetPointCollision(Vector3i(rBoatItem->Pose.Position.x, rBoatItem->Pose.Position.y + 128, rBoatItem->Pose.Position.z), probedRoomNumber).GetWaterTopHeight();
		if (height > rBoatItem->Pose.Position.y + 32 || height == NO_HEIGHT)
			height = 0;
		else
			height = 1;

		auto prop = GetJointPosition(rBoatItem, 2, Vector3i(0, 0, -80));
		probedRoomNumber = GetPointCollision(prop, rBoatItem->RoomNumber).GetRoomNumber();

		if (rBoatItem->Animation.Velocity.z &&
			height < prop.y &&
			height != NO_HEIGHT)
		{
			TriggerRubberBoatMist(prop.x, prop.y, prop.z, abs(rBoatItem->Animation.Velocity.z), rBoatItem->Pose.Orientation.y + ANGLE(180.0f), 0);
			
			int waterHeight = GetPointCollision(*rBoatItem).GetWaterTopHeight();
			SpawnVehicleWake(*rBoatItem, RBOAT_WAKE_OFFSET, waterHeight);

			if ((GetRandomControl() & 1) == 0)
			{
				auto pos = Vector3(
					prop.x + (GetRandomControl() & 63) - 32,
					prop.y + (GetRandomControl() & 15),
					prop.z + (GetRandomControl() & 63) - 32);

				short roomNumber = rBoatItem->RoomNumber;
				GetFloor(pos.x, pos.y, pos.z, &roomNumber);

				for (int i = 0; i < 5; i++)
					SpawnBubble(pos, roomNumber);
			}
		}
		else
		{
			height = GetPointCollision(prop, rBoatItem->RoomNumber).GetFloorHeight();
			if (prop.y > height &&
				!TestEnvironment(ENV_FLAG_WATER, probedRoomNumber))
			{
				GameVector pos;
				pos.x = prop.x;
				pos.y = prop.y;
				pos.z = prop.z;

				long cnt = (GetRandomControl() & 3) + 3;
				for (;cnt>0;cnt--)
					TriggerRubberBoatMist(prop.x, prop.y, prop.z, ((GetRandomControl() & 15) + 96) * 16, rBoatItem->Pose.Orientation.y + 0x4000 + GetRandomControl(), 1);
			}
		}
	}
}
