#include "framework.h"
#include "Objects/TR4/Vehicles/motorbike.h"
#include "Objects/TR4/Vehicles/motorbike_info.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/Point.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/effects/simple_particle.h"
#include "Game/effects/tomb4fx.h"
#include "Game/Gui.h"
#include "Game/Hud/Hud.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_flare.h"
#include "Game/Lara/lara_one_gun.h"
#include "Game/Setup.h"
#include "Math/Random.h"
#include "Objects/Utils/VehicleHelpers.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Input;
using namespace TEN::Math::Random;
using std::vector;

namespace TEN::Entities::Vehicles
{
	const std::vector<unsigned int> MotorbikeJoints = { 0, 1, 2, 4, 5, 6, 7, 8, 9 };
	const std::vector<unsigned int> MotorbikeBrakeLightJoints = { 10 };
	const std::vector<unsigned int> MotorbikeHeadLightJoints = { 3 };

	const std::vector<VehicleMountType> MotorbikeMountTypes =
	{
		VehicleMountType::LevelStart,
		VehicleMountType::Right
	};

	constexpr auto MOTORBIKE_RADIUS = 500;
	constexpr auto MOTORBIKE_MOUNT_DISTANCE = CLICK(2);
	constexpr auto MOTORBIKE_DISMOUNT_DISTANCE = CLICK(1.5f);
	constexpr auto MOTORBIKE_FRICTION = 384;
	constexpr auto MOTORBIKE_FRONT = 500;
	constexpr auto MOTORBIKE_SIDE = 350;
	constexpr auto MOTORBIKE_SLIP = 100;

	constexpr auto MOTORBIKE_ACCEL_1 = 64 * VEHICLE_VELOCITY_SCALE;
	constexpr auto MOTORBIKE_ACCEL_2 = 112 * VEHICLE_VELOCITY_SCALE;
	constexpr auto MOTORBIKE_ACCEL_MAX = 192 * VEHICLE_VELOCITY_SCALE;
	constexpr auto MOTORBIKE_ACCEL = 128 * VEHICLE_VELOCITY_SCALE;
	constexpr auto MOTORBIKE_BACKING_VEL = 8 * VEHICLE_VELOCITY_SCALE;
	constexpr auto MOTORBIKE_BIG_SLOWDOWN = 48 * VEHICLE_VELOCITY_SCALE;
	constexpr auto MOTORBIKE_SLOWDOWN1 = (int)(4.25f * VEHICLE_VELOCITY_SCALE); // TODO: Float velocities. @Sezz 2022.06.16
	constexpr auto MOTORBIKE_SLOWDOWN2 = 6 * VEHICLE_VELOCITY_SCALE;

	constexpr auto MOTORBIKE_PITCH_SLOWDOWN = 0x8000;
	constexpr auto MOTORBIKE_PITCH_MAX = 0xA000;

	constexpr auto MOTORBIKE_WAKE_OFFSET = Vector3(BLOCK(1 / 16.0f), 0, BLOCK(1 / 8.0f));

	#define MOTORBIKE_FORWARD_TURN_ANGLE ANGLE(1.5f)
	#define MOTORBIKE_BACK_TURN_ANGLE ANGLE(0.5f)
	#define MOTORBIKE_TURN_ANGLE_MAX ANGLE(5.0f)
	#define MOTORBIKE_MOMENTUM_TURN_ANGLE_MIN ANGLE(4.0f)
	#define MOTORBIKE_MOMENTUM_TURN_ANGLE_MAX ANGLE(1.5f)
	#define MOTORBIKE_MOMENTUM_TURN_ANGLE_MAX2 ANGLE(150.0f)

	enum MotorbikeState
	{
		MOTORBIKE_STATE_NONE,
		MOTORBIKE_STATE_MOVING_FRONT,
		MOTORBIKE_STATE_MOVING_LEFT,
		MOTORBIKE_STATE_MOVING_BACK,
		MOTORBIKE_STATE_MOVING_BACK_LOOP,
		MOTORBIKE_STATE_NONE_3,
		MOTORBIKE_STATE_STOP,
		MOTORBIKE_STATE_DEATH,
		MOTORBIKE_STATE_FALLING,
		MOTORBIKE_STATE_MOUNT, // include unlocking state
		MOTORBIKE_STATE_DISMOUNT,
		MOTORBIKE_STATE_HITFRONT,
		MOTORBIKE_STATE_HITBACK,
		MOTORBIKE_STATE_HITRIGHT,
		MOTORBIKE_STATE_HITLEFT,
		MOTORBIKE_STATE_IDLE,
		MOTORBIKE_STATE_LOADING_BOOST, // not used
		MOTORBIKE_STATE_LANDING,
		MOTORBIKE_STATE_ACCELERATE,
		MOTORBIKE_STATE_NONE_5,
		MOTORBIKE_STATE_NONE_6,
		MOTORBIKE_STATE_UNUSED,
		MOTORBIKE_STATE_MOVING_RIGHT
	};

	enum MotorbikeAnim
	{
		MOTORBIKE_ANIM_DEATH = 0,
		MOTORBIKE_ANIM_BRAKE = 1,
		MOTORBIKE_ANIM_MOVE_FORWARD = 2,
		MOTORBIKE_ANIM_START_LEFT = 3,
		MOTORBIKE_ANIM_LEFT = 4,
		MOTORBIKE_ANIM_END_LEFT = 5,
		MOTORBIKE_ANIM_START_FALL = 6,
		MOTORBIKE_ANIM_FALLING = 7,
		MOTORBIKE_ANIM_FALL_LAND = 8,
		MOTORBIKE_ANIM_MOUNT = 9,
		MOTORBIKE_ANIM_EXIT = 10,
		MOTORBIKE_ANIM_FRONT_HIT = 11,
		MOTORBIKE_ANIM_BACK_HIT = 12,
		MOTORBIKE_ANIM_LEFT_HIT = 13,
		MOTORBIKE_ANIM_RIGHT_HIT = 14,
		MOTORBIKE_ANIM_REV = 15, //unused? it looks like she's revving the engine but I've never seen it before
		MOTORBIKE_ANIM_SLOWDOWN = 16,
		MOTORBIKE_ANIM_UNUSED = 17,
		MOTORBIKE_ANIM_IDLE = 18,
		MOTORBIKE_ANIM_START_RIGHT = 19,
		MOTORBIKE_ANIM_RIGHT = 20,
		MOTORBIKE_ANIM_END_RIGHT = 21,
		MOTORBIKE_ANIM_START_JUMP = 22,
		MOTORBIKE_ANIM_JUMPING = 23,
		MOTORBIKE_ANIM_JUMP_LAND = 24,
		MOTORBIKE_ANIM_KICKSTART = 25,
		MOTORBIKE_ANIM_BACK_START = 26,
		MOTORBIKE_ANIM_BACK_LOOP = 27,
		MOTORBIKE_ANIM_UNLOCK = 28
	};

	enum MotorbikeFlags
	{
		MOTORBIKE_FLAG_BOOST = (1 << 0),
		MOTORBIKE_FLAG_NITRO = (1 << 1),
		MOTORBIKE_FLAG_FALLING = (1 << 6),
		MOTORBIKE_FLAG_DEATH = (1 << 7)
	};

	MotorbikeInfo* GetMotorbikeInfo(ItemInfo* motorbikeItem)
	{
		return (MotorbikeInfo*)motorbikeItem->Data;
	}

	void InitializeMotorbike(short itemNumber)
	{
		auto* motorbikeItem = &g_Level.Items[itemNumber];
		motorbikeItem->Data = MotorbikeInfo();
		auto* motorbike = GetMotorbikeInfo(motorbikeItem);

		motorbikeItem->MeshBits.Set(MotorbikeJoints);
		motorbike->MomentumAngle = motorbikeItem->Pose.Orientation.y;

		motorbikeItem->MeshBits.Clear(MotorbikeHeadLightJoints);
	}

	void MotorbikePlayerCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* motorbikeItem = &g_Level.Items[itemNumber];
		auto* motorbike = GetMotorbikeInfo(motorbikeItem);
		auto* lara = GetLaraInfo(laraItem);

		if (laraItem->HitPoints < 0 || lara->Context.Vehicle != NO_VALUE)
			return;

		auto mountType = GetVehicleMountType(motorbikeItem, laraItem, coll, MotorbikeMountTypes, MOTORBIKE_MOUNT_DISTANCE);
		if (mountType == VehicleMountType::None)
			ObjectCollision(itemNumber, laraItem, coll);
		else
		{
			SetLaraVehicle(laraItem, motorbikeItem);
			DoMotorbikeMount(motorbikeItem, laraItem, mountType);
		}
	}

	void DoMotorbikeMount(ItemInfo* motorbikeItem, ItemInfo* laraItem, VehicleMountType mountType)
	{
		auto* motorbike = GetMotorbikeInfo(motorbikeItem);
		auto* lara = GetLaraInfo(laraItem);

		switch (mountType)
		{
		case VehicleMountType::LevelStart:
			SetAnimation(*laraItem, ID_MOTORBIKE_LARA_ANIMS, MOTORBIKE_ANIM_IDLE);
			break;

		default:
		case VehicleMountType::Right:
			// HACK: Hardcoded Nitro item check.
			/*if (g_Gui.GetInventoryItemChosen() == ID_PUZZLE_ITEM1)
			{
				SetAnimation(*laraItem, ID_MOTORBIKE_LARA_ANIMS, MOTORBIKE_ANIM_UNLOCK);
				g_Gui.SetInventoryItemChosen(NO_VALUE);
				motorbike->Flags |= MOTORBIKE_FLAG_NITRO;
			}
			else
			{
				SetAnimation(*laraItem, ID_MOTORBIKE_LARA_ANIMS, MOTORBIKE_ANIM_MOUNT);
			}*/

			SetAnimation(*laraItem, ID_MOTORBIKE_LARA_ANIMS, MOTORBIKE_ANIM_MOUNT);
			break;
		}

		DoVehicleFlareDiscard(laraItem);
		ResetPlayerFlex(laraItem);
		laraItem->Pose.Position = motorbikeItem->Pose.Position;
		laraItem->Pose.Orientation.y = motorbikeItem->Pose.Orientation.y;
		lara->Control.HandStatus = HandStatus::Free;
		lara->HitDirection = -1;
		motorbikeItem->Collidable = true;
		motorbikeItem->HitPoints = 1;
		motorbike->Revs = 0;
		motorbike->LightPower = 0;

		AnimateItem(*laraItem);
	}

	static int DoMotorbikeShift(ItemInfo* motorbikeItem, Vector3i* pos, Vector3i* old)
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
				motorbikeItem->Pose.Position.z += old->z - pos->z;
				motorbikeItem->Pose.Position.x += old->x - pos->x;
			}
			else if (z > oldZ)
			{
				motorbikeItem->Pose.Position.z -= shiftZ + 1;
				return (pos->x - motorbikeItem->Pose.Position.x);
			}
			else
			{
				motorbikeItem->Pose.Position.z += BLOCK(1) - shiftZ;
				return (motorbikeItem->Pose.Position.x - pos->x);
			}
		}
		else if (z == oldZ)
		{
			if (x > oldX)
			{
				motorbikeItem->Pose.Position.x -= shiftX + 1;
				return (motorbikeItem->Pose.Position.z - pos->z);
			}
			else
			{
				motorbikeItem->Pose.Position.x += BLOCK(1) - shiftX;
				return (pos->z - motorbikeItem->Pose.Position.z);
			}
		}
		else
		{
			x = 0;
			z = 0;

			int floorHeight = GetPointCollision(Vector3i(old->x, pos->y, pos->z), motorbikeItem->RoomNumber).GetFloorHeight();
			if (floorHeight < (old->y - CLICK(1)))
			{
				if (pos->z > old->z)
					z = -shiftZ - 1;
				else
					z = BLOCK(1) - shiftZ;
			}

			floorHeight = GetPointCollision(Vector3i(pos->x, pos->y, old->z), motorbikeItem->RoomNumber).GetFloorHeight();
			if (floorHeight < (old->y - CLICK(1)))
			{
				if (pos->x > old->x)
					x = -shiftX - 1;
				else
					x = BLOCK(1) - shiftX;
			}

			if (x && z)
			{
				motorbikeItem->Pose.Position.z += z;
				motorbikeItem->Pose.Position.x += x;
			}
			else if (z)
			{
				motorbikeItem->Pose.Position.z += z;

				if (z > 0)
					return (motorbikeItem->Pose.Position.x - pos->x);
				else
					return (pos->x - motorbikeItem->Pose.Position.x);
			}
			else if (x)
			{
				motorbikeItem->Pose.Position.x += x;

				if (x > 0)
					return (pos->z - motorbikeItem->Pose.Position.z);
				else
					return (motorbikeItem->Pose.Position.z - pos->z);
			}
			else
			{
				motorbikeItem->Pose.Position.z += old->z - pos->z;
				motorbikeItem->Pose.Position.x += old->x - pos->x;
			}
		}

		return 0;
	}

	static void DrawMotorbikeLight(ItemInfo* motorbikeItem)
	{
		auto* motorbike = GetMotorbikeInfo(motorbikeItem);

		if (motorbike->LightPower <= 0)
			return;

		auto origin = GetJointPosition(motorbikeItem, 0, Vector3i(0, -470, 1836));
		auto target = GetJointPosition(motorbikeItem, 0, Vector3i(0, -470, 20780));
		int random = (motorbike->LightPower * 2) - (GetRandomControl() & 0xF);

		// TODO: Use target as direction vector for spotlight.
		SpawnDynamicLight(origin.x, origin.y, origin.z, 8, random, random / 2, 0);
	}

	static void TriggerMotorbikeExhaustSmoke(int x, int y, int z, short angle, short speed, bool moving)
	{
		int random = 0;
		BYTE trans, size;

		auto* sptr = GetFreeParticle();

		sptr->dR = 96;
		sptr->dG = 96;
		sptr->on = 1;
		sptr->sR = 0;
		sptr->sG = 0;
		sptr->sB = 0;
		sptr->dB = 128;

		if (moving)
		{
			trans = speed * 4;
			sptr->dR = trans;
			sptr->dG = trans;
			sptr->dB = trans;
		}

		sptr->colFadeSpeed = 4;
		sptr->fadeToBlack = 4;
		random = (GetRandomControl() & 3) - (speed / 4096) + 20;
		if (random < 9)
		{
			sptr->life = 9;
			sptr->sLife = 9;
		}
		else
		{
			sptr->life = random;
			sptr->sLife = random;
		}

		sptr->blendMode = BlendMode::Additive;
		sptr->x = x + (GetRandomControl() & 0xF) - 8;
		sptr->y = y + (GetRandomControl() & 0xF) - 8;
		sptr->z = z + (GetRandomControl() & 0xF) - 8;
		sptr->xVel = speed * phd_sin(angle) / 4;
		sptr->yVel = (GetRandomControl() & 7) - 8;
		sptr->zVel = speed * phd_cos(angle) / 4;
		sptr->friction = 4;

		if (GetRandomControl() & 1)
		{
			sptr->flags = SP_EXPDEF | SP_ROTATE | SP_DEF | SP_SCALE;
			sptr->rotAng = (GetRandomControl() & 0xFFF);
			if (GetRandomControl() & 1)
				sptr->rotAdd = (GetRandomControl() & 7) - 24;
			else
				sptr->rotAdd = (GetRandomControl() & 7) + 24;
		}
		else
			sptr->flags = SP_EXPDEF | SP_DEF | SP_SCALE;

		sptr->scalar = 1;
		sptr->SpriteSeqID = ID_DEFAULT_SPRITES;
		sptr->SpriteID = 0;
		sptr->gravity = (GetRandomControl() & 3) - 4;
		sptr->maxYvel = (GetRandomControl() & 7) - 8;
		size = (GetRandomControl() & 7) + (speed / 128) + 32;
		sptr->dSize = size;
		sptr->sSize = size / 2;
		sptr->size = size / 2;
	}

	static void DrawMotorBikeSmoke(ItemInfo* motorbikeItem, ItemInfo* laraItem)
	{
		auto* motorbike = GetMotorbikeInfo(motorbikeItem);
		auto* lara = GetLaraInfo(laraItem);

		if (lara->Context.Vehicle == NO_VALUE)
			return;

		if (laraItem->Animation.ActiveState != MOTORBIKE_STATE_MOUNT && laraItem->Animation.ActiveState != MOTORBIKE_STATE_DISMOUNT)
		{
			auto pos = GetJointPosition(motorbikeItem,  0, Vector3i(56, -144, -500));

			int speed = motorbikeItem->Animation.Velocity.z;
			if (speed > 32 && speed < 64)
			{
				TriggerMotorbikeExhaustSmoke(pos.x, pos.y, pos.z, motorbikeItem->Pose.Orientation.y - ANGLE(180.0f), 64 - speed, true);
				return;
			}

			if (motorbike->ExhaustStart >= 16)
			{
				if (GetRandomControl() & 3)
					speed = 0;
				else
					speed = ((GetRandomControl() & 0xF) + (GetRandomControl() & 0x10)) * 64;
			}
			else
				speed = ((GetRandomControl() & 0xF) + (GetRandomControl() & 0x10) + 2 * motorbike->ExhaustStart) * 64;

			TriggerMotorbikeExhaustSmoke(pos.x, pos.y, pos.z, motorbikeItem->Pose.Orientation.y - ANGLE(180.0f), speed, false);
		}
	}

	static int MotorBikeCheckGetOff(ItemInfo* laraItem)
	{
		auto* lara = GetLaraInfo(laraItem);

		if (lara->Context.Vehicle != NO_VALUE)
		{
			auto* item = &g_Level.Items[lara->Context.Vehicle];

			if (laraItem->Animation.ActiveState == MOTORBIKE_STATE_DISMOUNT &&
				TestLastFrame(*laraItem))
			{
				SetAnimation(*laraItem, LA_STAND_SOLID);
				laraItem->Pose.Orientation.x = 0;
				laraItem->Pose.Orientation.y -= ANGLE(90.0f);
				laraItem->Pose.Orientation.z = 0;
				laraItem->Pose.Translate(laraItem->Pose.Orientation.y, -MOTORBIKE_DISMOUNT_DISTANCE);
				lara->Control.HandStatus = HandStatus::Free;
				lara->Status.Stamina = LARA_STAMINA_MAX;
				SetLaraVehicle(laraItem, nullptr);
				return true;
			}

			if (!TestLastFrame(*laraItem))
				return true;

			// exit when falling
			// if (laraItem->state_current == MOTORBIKE_STATE_NONE_6) {

			// }
			// else if (laraItem->state_current == MOTORBIKE_STATE_NONE_5) {
			// lara death when falling too much
			// }

			return false;
		}
		else
			return false;
	}

	static int DoMotorBikeDynamics(int height, int verticalVelocity, int* y, int flags)
	{
		int kick;

		if (height <= *y)
		{
			if (flags)
				return verticalVelocity;
			else
			{
				// On ground.
				kick = (height - *y);

				if (kick < -80)
					kick = -80;

				verticalVelocity += (kick - verticalVelocity) / 16;

				if (*y > height)
					*y = height;
			}
		}
		else
		{
			// In air.
			*y += verticalVelocity;
			if (*y > height - 32)
			{
				*y = height;
				verticalVelocity = 0;
			}
			else
			{
				if (flags)
				{
					verticalVelocity += flags;
				}
				else
				{
					verticalVelocity += g_GameFlow->GetSettings()->Physics.Gravity;
				}
			}
		}

		return verticalVelocity;
	}

	static int GetMotorbikeCollisionAnim(ItemInfo* motorbikeItem, Vector3i* pos)
	{
		pos->x = motorbikeItem->Pose.Position.x - pos->x;
		pos->z = motorbikeItem->Pose.Position.z - pos->z;

		if (pos->x || pos->z)
		{
			float sinY = phd_sin(motorbikeItem->Pose.Orientation.y);
			float cosY = phd_cos(motorbikeItem->Pose.Orientation.y);

			int front = (pos->z * cosY) + (pos->x * sinY);
			int side = (pos->z * -sinY) + (pos->x * cosY);

			if (abs(front) > abs(side))
				return ((front > 0) + 13);
			else
				return ((side <= 0) + 11);
		}

		return 0;
	}

	static int MotorBikeDynamics(ItemInfo* motorbikeItem, ItemInfo* laraItem)
	{
		auto* motorbike = GetMotorbikeInfo(motorbikeItem);
		auto* lara = GetLaraInfo(laraItem);

		Vector3i frontLeft, backLeft, mtf, mtb, backRight;
		Vector3i moved;
		int floorHeight, collide, speed, newSpeed;
		short momentum = 0, rotation;

		motorbike->DisableDismount = false;

		Vector3i backLeftOld, mtb_old, backRightOld, mtf_old, rightLeftOld;
		int hfl_old  = GetVehicleHeight(motorbikeItem, MOTORBIKE_FRONT, -MOTORBIKE_SIDE, true, &rightLeftOld);
		int hmf_old  = GetVehicleHeight(motorbikeItem, MOTORBIKE_FRONT, CLICK(0.5f), true, &mtf_old);
		int hbl_old  = GetVehicleHeight(motorbikeItem, -MOTORBIKE_FRONT, -MOTORBIKE_SIDE, true, &backLeftOld);
		int hbr_old  = GetVehicleHeight(motorbikeItem, -MOTORBIKE_FRONT, CLICK(0.5f), true, &backRightOld);
		int hmtb_old = GetVehicleHeight(motorbikeItem, -MOTORBIKE_FRONT, 0, true, &mtb_old);

		auto oldPos = motorbikeItem->Pose.Position;

		if (motorbikeItem->Pose.Position.y <= (motorbikeItem->Floor - 8))
		{
			if (motorbike->TurnRate < -ANGLE(0.5f))
				motorbike->TurnRate += ANGLE(0.5f);
			else if (motorbike->TurnRate > ANGLE(0.5f))
				motorbike->TurnRate -= ANGLE(0.5f);
			else
				motorbike->TurnRate = 0;

			motorbikeItem->Pose.Orientation.y += motorbike->TurnRate + motorbike->ExtraRotation;
			motorbike->MomentumAngle += (motorbikeItem->Pose.Orientation.y - motorbike->MomentumAngle) / 32;
		}
		else
		{
			if (motorbike->TurnRate >= -ANGLE(1.0f))
			{
				if (motorbike->TurnRate <= ANGLE(1.0f))
					motorbike->TurnRate = 0;
				else
					motorbike->TurnRate -= ANGLE(1.0f);
			}
			else
				motorbike->TurnRate += ANGLE(1.0f);

			motorbikeItem->Pose.Orientation.y += motorbike->TurnRate + motorbike->ExtraRotation;
			rotation = motorbikeItem->Pose.Orientation.y - motorbike->MomentumAngle;
			momentum = MOTORBIKE_MOMENTUM_TURN_ANGLE_MIN - ((2 * motorbike->Velocity) / BLOCK(1));

			if (!IsHeld(In::Accelerate) && motorbike->Velocity > 0)
				momentum += momentum / 2;

			if (rotation < -MOTORBIKE_MOMENTUM_TURN_ANGLE_MAX)
			{
				if (rotation < -MOTORBIKE_MOMENTUM_TURN_ANGLE_MAX2)
				{
					rotation = -MOTORBIKE_MOMENTUM_TURN_ANGLE_MAX2;
					motorbike->MomentumAngle = motorbikeItem->Pose.Orientation.y - rotation;
				}
				else
					motorbike->MomentumAngle -= momentum;
			}
			else if (rotation > MOTORBIKE_MOMENTUM_TURN_ANGLE_MAX)
			{
				if (rotation > MOTORBIKE_MOMENTUM_TURN_ANGLE_MAX2)
				{
					rotation = MOTORBIKE_MOMENTUM_TURN_ANGLE_MAX2;
					motorbike->MomentumAngle = motorbikeItem->Pose.Orientation.y - rotation;
				}
				else
					motorbike->MomentumAngle += momentum;
			}
			else
				motorbike->MomentumAngle = motorbikeItem->Pose.Orientation.y;
		}

		floorHeight = GetPointCollision(*motorbikeItem).GetFloorHeight();
		if (motorbikeItem->Pose.Position.y >= floorHeight)
			speed = motorbikeItem->Animation.Velocity.z * phd_cos(motorbikeItem->Pose.Orientation.x);
		else
			speed = motorbikeItem->Animation.Velocity.z;

		motorbikeItem->Pose.Translate(motorbike->MomentumAngle, speed);

		if (motorbikeItem->Pose.Position.y >= floorHeight)
		{
			short anglex = MOTORBIKE_SLIP * phd_sin(motorbikeItem->Pose.Orientation.x);
			if (abs(anglex) > 16)
			{
				short anglex2 = MOTORBIKE_SLIP * phd_sin(motorbikeItem->Pose.Orientation.x);
				if (anglex < 0)
					anglex2 = -anglex;
				if (anglex2 > 24)
					motorbike->DisableDismount = true;
				anglex *= 16;
				motorbike->Velocity -= anglex;
			}

			short anglez = MOTORBIKE_SLIP * phd_sin(motorbikeItem->Pose.Orientation.z);
			if (abs(anglez) > 32)
			{
				short ang, angabs;
				motorbike->DisableDismount = true;

				if (anglez >= 0)
					ang = motorbikeItem->Pose.Orientation.y + ANGLE(90.0f);
				else
					ang = motorbikeItem->Pose.Orientation.y - ANGLE(90.0f);

				angabs = abs(anglez) - 24;
				motorbikeItem->Pose.Position.x += angabs * phd_sin(ang);
				motorbikeItem->Pose.Position.z += angabs * phd_cos(ang);
			}
		}

		if (motorbike->Velocity <= MOTORBIKE_ACCEL || (motorbike->Flags & MOTORBIKE_FLAG_BOOST))
		{
			if (motorbike->Velocity <= MOTORBIKE_ACCEL_MAX)
			{
				if (motorbike->Velocity < -MOTORBIKE_BIG_SLOWDOWN)
					motorbike->Velocity = -MOTORBIKE_BIG_SLOWDOWN;
			}
			else
				motorbike->Velocity = MOTORBIKE_ACCEL_MAX;
		}
		else
			motorbike->Velocity -= MOTORBIKE_SLOWDOWN1;

		moved.x = motorbikeItem->Pose.Position.x;
		moved.z = motorbikeItem->Pose.Position.z;

		if (!(motorbikeItem->Flags & IFLAG_INVISIBLE))
			DoVehicleCollision(motorbikeItem, MOTORBIKE_RADIUS);

		int rot1 = 0;
		int rot2 = 0;

		int hfl = GetVehicleHeight(motorbikeItem, MOTORBIKE_FRONT, -MOTORBIKE_SIDE, false, &frontLeft);
		if (hfl < rightLeftOld.y - CLICK(1))
		{
			rot1 = abs(4 * DoMotorbikeShift(motorbikeItem, &frontLeft, &rightLeftOld));
		}

		int hbl = GetVehicleHeight(motorbikeItem, -MOTORBIKE_FRONT, -MOTORBIKE_SIDE, false, &backLeft);
		if (hbl < backLeftOld.y - CLICK(1))
		{
			if (rot1)
				rot1 += abs(4 * DoMotorbikeShift(motorbikeItem, &backLeft, &backLeftOld));
			else
				rot1 -= abs(4 * DoMotorbikeShift(motorbikeItem, &backLeft, &backLeftOld));
		}

		int hmtf = GetVehicleHeight(motorbikeItem, MOTORBIKE_FRONT, CLICK(0.5f), false, &mtf);
		if (hmtf < mtf_old.y - CLICK(1))
			rot2 -= abs(4 * DoMotorbikeShift(motorbikeItem, &backLeft, &backLeftOld));

		int hmtb = GetVehicleHeight(motorbikeItem, -MOTORBIKE_FRONT, 0, false, &mtb);
		if (hmtb < mtb_old.y - CLICK(1))
			DoMotorbikeShift(motorbikeItem, &mtb, &mtb_old);

		int hbr = GetVehicleHeight(motorbikeItem, -MOTORBIKE_FRONT, CLICK(0.5f), false, &backRight);
		if (hbr < backRightOld.y - CLICK(1))
		{
			if (rot2)
				rot2 -= abs(4 * DoMotorbikeShift(motorbikeItem, &backLeft, &backLeftOld));
			else
				rot2 += abs(4 * DoMotorbikeShift(motorbikeItem, &backLeft, &backLeftOld));
		}

		if (rot1)
			rot2 = rot1;

		floorHeight = GetPointCollision(*motorbikeItem).GetFloorHeight();
		if (floorHeight < (motorbikeItem->Pose.Position.y - CLICK(1)))
			DoMotorbikeShift(motorbikeItem, (Vector3i*)&motorbikeItem->Pose, &oldPos);

		if (!motorbike->Velocity)
			rot2 = 0;

		motorbike->WallShiftRotation = (motorbike->WallShiftRotation + rot2) / 2;
		if (abs(motorbike->WallShiftRotation) < 2)
			motorbike->WallShiftRotation = 0;

		if (abs(motorbike->WallShiftRotation - motorbike->ExtraRotation) >= 4)
			motorbike->ExtraRotation += ((motorbike->WallShiftRotation - motorbike->ExtraRotation) / 4);
		else
			motorbike->ExtraRotation = motorbike->WallShiftRotation;

		collide = GetMotorbikeCollisionAnim(motorbikeItem, &moved);
		if (collide)
		{
			newSpeed = ((motorbikeItem->Pose.Position.z - oldPos.z) * phd_cos(motorbike->MomentumAngle) + (motorbikeItem->Pose.Position.x - oldPos.x) * phd_sin(motorbike->MomentumAngle)) * 256;
			if (&g_Level.Items[lara->Context.Vehicle] == motorbikeItem &&
				motorbike->Velocity >= MOTORBIKE_ACCEL && newSpeed < (motorbike->Velocity - 10))
			{
				DoDamage(laraItem, (motorbike->Velocity - newSpeed) / 128);
			}

			if (motorbike->Velocity > 0 && newSpeed < motorbike->Velocity)
				motorbike->Velocity = (newSpeed < 0) ? 0 : newSpeed;
			else if (motorbike->Velocity < 0 && newSpeed > motorbike->Velocity)
				motorbike->Velocity = (newSpeed > 0) ? 0 : newSpeed;

			if (motorbike->Velocity < -MOTORBIKE_BIG_SLOWDOWN)
				motorbike->Velocity = -MOTORBIKE_BIG_SLOWDOWN;
		}

		return collide;
	}

	static bool TestMotorbikeDismount(ItemInfo* motorbikeItem, ItemInfo* laraItem)
	{
		auto* lara = GetLaraInfo(laraItem);

		short angle = motorbikeItem->Pose.Orientation.y + ANGLE(90.0f);
		auto collResult = GetPointCollision(*motorbikeItem, angle, MOTORBIKE_RADIUS);

		if (collResult.IsSteepFloor() || collResult.GetFloorHeight() == NO_HEIGHT) // Was previously set to -NO_HEIGHT by TokyoSU -- Lwmte 23.08.21
			return false;

		if (abs(collResult.GetFloorHeight() - motorbikeItem->Pose.Position.y) > CLICK(1))
			return false;

		if ((collResult.GetCeilingHeight() - motorbikeItem->Pose.Position.y) > -LARA_HEIGHT)
			return false;

		if ((collResult.GetFloorHeight() - collResult.GetCeilingHeight()) < LARA_HEIGHT)
			return false;

		return true;
	}

	static void AnimateMotorbike(ItemInfo* motorbikeItem, ItemInfo* laraItem, int collide, bool isDead)
	{
		auto* motorbike = GetMotorbikeInfo(motorbikeItem);

		if (isDead ||
			motorbikeItem->Pose.Position.y == motorbikeItem->Floor ||
			laraItem->Animation.ActiveState == MOTORBIKE_STATE_FALLING ||
			laraItem->Animation.ActiveState == MOTORBIKE_STATE_LANDING ||
			laraItem->Animation.ActiveState == MOTORBIKE_STATE_NONE_6)
		{
			if (isDead || !collide ||
				motorbike->Velocity <= (42 * VEHICLE_VELOCITY_SCALE) ||
				laraItem->Animation.ActiveState == MOTORBIKE_STATE_HITBACK ||
				laraItem->Animation.ActiveState == MOTORBIKE_STATE_HITFRONT ||
				laraItem->Animation.ActiveState == MOTORBIKE_STATE_HITLEFT ||
				laraItem->Animation.ActiveState == MOTORBIKE_STATE_NONE_6)
			{
				switch (laraItem->Animation.ActiveState)
				{
				case MOTORBIKE_STATE_IDLE:
					if (isDead)
						laraItem->Animation.TargetState = MOTORBIKE_STATE_DEATH;
					else 
					{
						bool dismount = false;
						if (IsHeld(In::Right) && IsHeld(In::Brake))
							dismount = true;

						if (!dismount || motorbike->Velocity || motorbike->DisableDismount)
						{
							if (IsHeld(In::Accelerate) && !IsHeld(In::Brake))
								laraItem->Animation.TargetState = MOTORBIKE_STATE_MOVING_FRONT;
							else if (IsHeld(In::Reverse))
								laraItem->Animation.TargetState = MOTORBIKE_STATE_MOVING_BACK;
						}
						else if (dismount && TestMotorbikeDismount(motorbikeItem, laraItem))
							laraItem->Animation.TargetState = MOTORBIKE_STATE_DISMOUNT;
						else
							laraItem->Animation.TargetState = MOTORBIKE_STATE_IDLE;
					}

					break;

				case MOTORBIKE_STATE_MOVING_FRONT:
					if (isDead)
					{
						if (motorbike->Velocity <= MOTORBIKE_ACCEL_1)
							laraItem->Animation.TargetState = MOTORBIKE_STATE_DEATH;
						else
							laraItem->Animation.TargetState = MOTORBIKE_STATE_NONE_5;
					}
					else if ((motorbike->Velocity / VEHICLE_VELOCITY_SCALE) != 0)
					{
						if (IsHeld(In::Left))
							laraItem->Animation.TargetState = MOTORBIKE_STATE_MOVING_LEFT;
						else if (IsHeld(In::Right))
							laraItem->Animation.TargetState = MOTORBIKE_STATE_MOVING_RIGHT;
						else if (IsHeld(In::Brake))
						{
							if (motorbike->Velocity <= 0x5554) // 85.3f * VEHICLE_VELOCITY_SCALE
								laraItem->Animation.TargetState = MOTORBIKE_STATE_NONE_3;
							else
								laraItem->Animation.TargetState = MOTORBIKE_STATE_STOP;
						}
						else if (IsHeld(In::Reverse) && motorbike->Velocity <= MOTORBIKE_BACKING_VEL)
							laraItem->Animation.TargetState = MOTORBIKE_STATE_MOVING_BACK;
						else if (motorbike->Velocity == 0)
							laraItem->Animation.TargetState = MOTORBIKE_STATE_IDLE;
					}
					else
						laraItem->Animation.TargetState = MOTORBIKE_STATE_IDLE;

					break;

				case MOTORBIKE_STATE_MOVING_LEFT:
					if ((motorbike->Velocity / VEHICLE_VELOCITY_SCALE) != 0)
					{
						if (IsHeld(In::Right) || !IsHeld(In::Left))
							laraItem->Animation.TargetState = MOTORBIKE_STATE_MOVING_FRONT;
					}
					else
						laraItem->Animation.TargetState = MOTORBIKE_STATE_IDLE;
					if (motorbike->Velocity == 0)
						laraItem->Animation.TargetState = MOTORBIKE_STATE_IDLE;

					break;

				case MOTORBIKE_STATE_MOVING_BACK:
					if (IsHeld(In::Reverse))
						laraItem->Animation.TargetState = MOTORBIKE_STATE_MOVING_BACK_LOOP;
					else
						laraItem->Animation.TargetState = MOTORBIKE_STATE_IDLE;

					break;

				case MOTORBIKE_STATE_MOVING_RIGHT:
					if ((motorbike->Velocity / VEHICLE_VELOCITY_SCALE) != 0)
					{
						if (IsHeld(In::Left) || !(IsHeld(In::Right)))
							laraItem->Animation.TargetState = MOTORBIKE_STATE_MOVING_FRONT;
					}
					else
						laraItem->Animation.TargetState = MOTORBIKE_STATE_IDLE;

					if (motorbike->Velocity == 0)
						laraItem->Animation.TargetState = MOTORBIKE_STATE_IDLE;

					break;

				case MOTORBIKE_STATE_NONE_3:
				case MOTORBIKE_STATE_STOP:
				case MOTORBIKE_STATE_ACCELERATE:
					if ((motorbike->Velocity / VEHICLE_VELOCITY_SCALE) != 0)
					{
						if (IsHeld(In::Left))
							laraItem->Animation.TargetState = MOTORBIKE_STATE_MOVING_LEFT;

						if (IsHeld(In::Right))
							laraItem->Animation.TargetState = MOTORBIKE_STATE_MOVING_RIGHT;
					}
					else
						laraItem->Animation.TargetState = MOTORBIKE_STATE_IDLE;
					
					break;

				case MOTORBIKE_STATE_FALLING:
					if (motorbikeItem->Pose.Position.y == motorbikeItem->Floor)
					{
						laraItem->Animation.TargetState = MOTORBIKE_STATE_LANDING;

						int fallSpeedDamage = motorbikeItem->Animation.Velocity.y - 140;
						if (fallSpeedDamage > 0)
						{
							if (fallSpeedDamage <= 100)
								DoDamage(laraItem, (-LARA_HEALTH_MAX * fallSpeedDamage * fallSpeedDamage) / 10000);
							else
								DoDamage(laraItem, LARA_HEALTH_MAX);
						}
					}
					else if (motorbikeItem->Animation.Velocity.y > 220)
						motorbike->Flags |= MOTORBIKE_FLAG_FALLING;

					break;

				case MOTORBIKE_STATE_HITFRONT:
				case MOTORBIKE_STATE_HITBACK:
				case MOTORBIKE_STATE_HITRIGHT:
				case MOTORBIKE_STATE_HITLEFT:
					if (IsHeld(In::Accelerate) || IsHeld(In::Brake))
						laraItem->Animation.TargetState = MOTORBIKE_STATE_MOVING_FRONT;

					break;
				}
			}
			else
			{
				switch (collide)
				{
				case 13:
					SetAnimation(*laraItem, ID_MOTORBIKE_LARA_ANIMS, MOTORBIKE_ANIM_BACK_HIT);
					break;

				case 14:
					SetAnimation(*laraItem, ID_MOTORBIKE_LARA_ANIMS, MOTORBIKE_ANIM_FRONT_HIT);
					break;

				case 11:
					SetAnimation(*laraItem, ID_MOTORBIKE_LARA_ANIMS, MOTORBIKE_ANIM_RIGHT_HIT);
					break;

				case 12:
				default:
					SetAnimation(*laraItem, ID_MOTORBIKE_LARA_ANIMS, MOTORBIKE_ANIM_LEFT_HIT);
					break;
				}
			}
		}
		else
		{
			if (motorbike->Velocity >= 0)
				SetAnimation(*laraItem, ID_MOTORBIKE_LARA_ANIMS, MOTORBIKE_ANIM_START_JUMP);
			else
				SetAnimation(*laraItem, ID_MOTORBIKE_LARA_ANIMS, MOTORBIKE_ANIM_START_FALL);
		}
	}

	static int MotorbikeUserControl(ItemInfo* motorbikeItem, ItemInfo* laraItem, int height, int* pitch)
	{
		auto* motorbike = GetMotorbikeInfo(motorbikeItem);
		auto* lara = GetLaraInfo(laraItem);

		if (motorbike->LightPower < 127)
		{
			motorbike->LightPower += (GetRandomControl() & 7) + 3;
			if (motorbike->LightPower > 127)
				motorbike->LightPower = 127;
		}

		if (motorbike->Revs > 0x10)
		{
			motorbike->Velocity += motorbike->Revs / 16;
			motorbike->Revs -= motorbike->Revs / 80;
		}
		else
			motorbike->Revs = 0;

		if (IsHeld(In::Faster) && IsHeld(In::Accelerate) && 
			(motorbike->Flags & MOTORBIKE_FLAG_NITRO))
		{
			if (lara->Status.Stamina > 10)
			{
				motorbike->Flags |= MOTORBIKE_FLAG_BOOST;
				lara->Status.Stamina -= 2;

				if (lara->Status.Stamina <= 0)
				{
					motorbike->Flags &= ~MOTORBIKE_FLAG_BOOST;
					lara->Status.Stamina = 0;
				}
			}
		}
		else
			motorbike->Flags &= ~MOTORBIKE_FLAG_BOOST;

		if (motorbikeItem->Pose.Position.y >= (height - CLICK(1)))
		{
			lara->Control.Look.Mode = (motorbikeItem->Animation.Velocity.z == 0.0f) ? LookMode::Horizontal : LookMode::Free;

			// Moving forward.
			if (motorbike->Velocity > 0)
			{
				if (IsHeld(In::Left))
				{
					if (motorbike->Velocity > MOTORBIKE_ACCEL_1)
						motorbike->TurnRate -= MOTORBIKE_FORWARD_TURN_ANGLE;
					else
						motorbike->TurnRate -= MOTORBIKE_FORWARD_TURN_ANGLE * ((float)motorbike->Velocity / 8192.0f);

					if (motorbike->TurnRate < -MOTORBIKE_TURN_ANGLE_MAX)
						motorbike->TurnRate = -MOTORBIKE_TURN_ANGLE_MAX;
				}
				else if (IsHeld(In::Right))
				{
					if (motorbike->Velocity > MOTORBIKE_ACCEL_1)
						motorbike->TurnRate += MOTORBIKE_FORWARD_TURN_ANGLE;
					else
						motorbike->TurnRate += MOTORBIKE_FORWARD_TURN_ANGLE * ((float)motorbike->Velocity / 8192.0f);

					if (motorbike->TurnRate > MOTORBIKE_TURN_ANGLE_MAX)
						motorbike->TurnRate = MOTORBIKE_TURN_ANGLE_MAX;
				}
			}
			// Moving back.
			else if (motorbike->Velocity < 0)
			{
				if (IsHeld(In::Left))
				{
					motorbike->TurnRate += MOTORBIKE_BACK_TURN_ANGLE;
					if (motorbike->TurnRate > MOTORBIKE_TURN_ANGLE_MAX)
						motorbike->TurnRate = MOTORBIKE_TURN_ANGLE_MAX;
				}
				else if (IsHeld(In::Right))
				{
					motorbike->TurnRate -= MOTORBIKE_BACK_TURN_ANGLE;
					if (motorbike->TurnRate < -MOTORBIKE_TURN_ANGLE_MAX)
						motorbike->TurnRate = -MOTORBIKE_TURN_ANGLE_MAX;
				}
			}

			if (IsHeld(In::Brake))
			{
				auto pos = GetJointPosition(motorbikeItem, 0, Vector3i(0, -144, -1024));
				SpawnDynamicLight(pos.x, pos.y, pos.z, 10, 64, 0, 0);

				motorbikeItem->MeshBits.Set(MotorbikeBrakeLightJoints);
			}
			else
				motorbikeItem->MeshBits.Clear(MotorbikeBrakeLightJoints);

			if (IsHeld(In::Brake))
			{
				if (motorbike->Velocity < 0)
				{
					motorbike->Velocity += 3 * VEHICLE_VELOCITY_SCALE;
					if (motorbike->Velocity > 0)
						motorbike->Velocity = 0;
				}
				else
				{
					motorbike->Velocity -= 3 * VEHICLE_VELOCITY_SCALE;
					if (motorbike->Velocity < 0)
						motorbike->Velocity = 0;
				}
			}
			else if (IsHeld(In::Accelerate))
			{
				if (motorbike->Velocity < MOTORBIKE_ACCEL_MAX)
				{
					if (motorbike->Velocity < MOTORBIKE_ACCEL_1)
						motorbike->Velocity += ((MOTORBIKE_ACCEL_1 + MOTORBIKE_BACKING_VEL - motorbike->Velocity) / 8) + 8;
					else if (motorbike->Velocity < MOTORBIKE_ACCEL_2)
						motorbike->Velocity += ((MOTORBIKE_ACCEL_2 + MOTORBIKE_BACKING_VEL - motorbike->Velocity) / 16) + 4;
					else if (motorbike->Velocity < MOTORBIKE_ACCEL_MAX)
						motorbike->Velocity += ((MOTORBIKE_ACCEL_MAX - motorbike->Velocity) / 16) + 2;

					if (motorbike->Flags & MOTORBIKE_FLAG_BOOST)
						motorbike->Velocity += 1 * VEHICLE_VELOCITY_SCALE;
				}
				else
					motorbike->Velocity = MOTORBIKE_ACCEL_MAX;

				// Apply friction according to turn.
				motorbike->Velocity -= abs(motorbikeItem->Pose.Orientation.y - motorbike->MomentumAngle) / 64;
			}
			else if (motorbike->Velocity > MOTORBIKE_FRICTION)
			{
				motorbike->Velocity -= MOTORBIKE_FRICTION;
				if (motorbike->Velocity < 0)
					motorbike->Velocity = 0;
			}
			else if (motorbike->Velocity < MOTORBIKE_FRICTION)
			{
				motorbike->Velocity += MOTORBIKE_FRICTION;
				if (motorbike->Velocity > 0)
					motorbike->Velocity = 0;
			}
			else
				motorbike->Velocity = 0;

			if (laraItem->Animation.ActiveState == MOTORBIKE_STATE_MOVING_BACK)
			{
				int currentFrame = laraItem->Animation.FrameNumber;
				int frameBase = 0;

				if (currentFrame >= frameBase + 24 &&
					currentFrame <= frameBase + 29)
				{
					if (motorbike->Velocity > -MOTORBIKE_BIG_SLOWDOWN)
						motorbike->Velocity -= MOTORBIKE_SLOWDOWN2;
				}
			}

			motorbikeItem->Animation.Velocity.z = motorbike->Velocity / VEHICLE_VELOCITY_SCALE;

			if (motorbike->EngineRevs > MOTORBIKE_ACCEL_MAX)
				motorbike->EngineRevs = (GetRandomControl() & 0x1FF) + 0xBF00;

			int newPitch = motorbike->Velocity;
			if (motorbike->Velocity < 0)
				newPitch /= 2;

			motorbike->EngineRevs += (abs(newPitch) - 0x2000 - motorbike->EngineRevs) / 8;
			*pitch = motorbike->EngineRevs;
		}
		else
		{
			if (motorbike->EngineRevs < 0xFFFF)
				motorbike->EngineRevs += (motorbike->EngineRevs - 0xFFFF) / 8;

			*pitch = motorbike->EngineRevs;
		}

		return 0;
	}

	// TODO: Unused function?
	void SetLaraOnMotorBike(ItemInfo* motorbikeItem, ItemInfo* laraItem)
	{
		auto* motorbike = GetMotorbikeInfo(motorbikeItem);
		auto* lara = GetLaraInfo(laraItem);

		SetAnimation(*laraItem, ID_MOTORBIKE_LARA_ANIMS, MOTORBIKE_ANIM_IDLE);
		laraItem->Animation.IsAirborne = false;
		lara->Control.HandStatus = HandStatus::Busy;
		lara->HitDirection = -1;

		// Sync vehicle with player animation.
		SyncVehicleAnim(*motorbikeItem, *laraItem);

		motorbikeItem->HitPoints = 1;
		motorbikeItem->Flags = IFLAG_KILLED; // hmm... maybe wrong name (it can be IFLAG_CODEBITS)?
		motorbike->Revs = 0;
	}

	bool MotorbikeControl(ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* lara = GetLaraInfo(laraItem);
		auto* motorbikeItem = &g_Level.Items[lara->Context.Vehicle];
		auto* motorbike = GetMotorbikeInfo(motorbikeItem);

		int collide = MotorBikeDynamics(motorbikeItem, laraItem);
		int drive = -1;

		auto oldPos = motorbikeItem->Pose.Position;

		Vector3i frontLeft, frontRight, frontMiddle;
		int heightFrontLeft = GetVehicleHeight(motorbikeItem, MOTORBIKE_FRONT, -MOTORBIKE_SIDE, true, &frontLeft);
		int heightFrontRight = GetVehicleHeight(motorbikeItem, MOTORBIKE_FRONT, CLICK(0.5f), true, &frontRight);
		int heightFrontMiddle = GetVehicleHeight(motorbikeItem, -MOTORBIKE_FRONT, 0, true, &frontMiddle);

		auto probe = GetPointCollision(*motorbikeItem);

		TestTriggers(motorbikeItem, true);
		TestTriggers(motorbikeItem, false);

		bool isDead;
		if (laraItem->HitPoints <= 0)
		{
			isDead = true;
			ClearAction(In::Forward);
			ClearAction(In::Back);
			ClearAction(In::Left);
			ClearAction(In::Right);
		}
		else
			isDead = false;

		int pitch = 0;

		if (laraItem->Animation.ActiveState < MOTORBIKE_STATE_MOUNT ||
			laraItem->Animation.ActiveState > MOTORBIKE_STATE_DISMOUNT)
		{
			DrawMotorbikeLight(motorbikeItem);
			motorbikeItem->MeshBits.Set(MotorbikeHeadLightJoints);

			drive = MotorbikeUserControl(motorbikeItem, laraItem, probe.GetFloorHeight(), &pitch);
			HandleVehicleSpeedometer(motorbikeItem->Animation.Velocity.z, MOTORBIKE_ACCEL_MAX / (float)VEHICLE_VELOCITY_SCALE);
		}
		else
		{
			motorbikeItem->MeshBits.Clear(MotorbikeHeadLightJoints);
			motorbikeItem->MeshBits.Clear(MotorbikeBrakeLightJoints);

			drive = -1;
			collide = 0;
		}

		if (motorbike->Velocity > 0 || motorbike->Revs)
		{
			motorbike->Pitch = pitch;

			if (motorbike->Pitch < -MOTORBIKE_PITCH_SLOWDOWN) 
				motorbike->Pitch = -MOTORBIKE_PITCH_SLOWDOWN; 
			else if (motorbike->Pitch > MOTORBIKE_PITCH_MAX)
				motorbike->Pitch = MOTORBIKE_PITCH_MAX;

			SoundEffect(SFX_TR4_VEHICLE_MOTORBIKE_MOVING, &motorbikeItem->Pose, SoundEnvironment::Land, 0.7f + motorbike->Pitch / 24756.0f);
		}
		else
		{
			if (drive != -1)
			{
				SoundEffect(SFX_TR4_VEHICLE_MOTORBIKE_IDLE, &motorbikeItem->Pose);
				SoundEffect(SFX_TR4_VEHICLE_MOTORBIKE_MOVING, &motorbikeItem->Pose, SoundEnvironment::Land, 0.7f + motorbike->Pitch / 24756.0f, 0.5f);
			}

			motorbike->Pitch = 0;
		}

		if (motorbike->Velocity < MOTORBIKE_ACCEL_1)
			DrawMotorBikeSmoke(motorbikeItem, laraItem);

		motorbikeItem->Floor = probe.GetFloorHeight();

		int rotation = motorbike->Velocity / 4;
		motorbike->LeftWheelRotation -= rotation;
		motorbike->RightWheelsRotation -= rotation;

		int newY = motorbikeItem->Pose.Position.y;
		motorbikeItem->Animation.Velocity.y = DoMotorBikeDynamics(probe.GetFloorHeight(), motorbikeItem->Animation.Velocity.y, &motorbikeItem->Pose.Position.y, 0);
		motorbike->Velocity = DoVehicleWaterMovement(motorbikeItem, laraItem, motorbike->Velocity, MOTORBIKE_RADIUS, &motorbike->TurnRate, MOTORBIKE_WAKE_OFFSET);

		int r1 = (frontRight.y + frontLeft.y) / 2;
		int r2 = (frontRight.y + frontLeft.y) / 2;
		short xRot = 0;
		short zRot = 0;
		if (frontMiddle.y >= heightFrontMiddle)
		{
			if (r1 >= ((heightFrontLeft + heightFrontRight) / 2))
			{
				xRot = phd_atan(1000, heightFrontMiddle - r1);
				zRot = phd_atan(MOTORBIKE_SIDE, r2 - frontLeft.y);
			}
			else
			{
				xRot = phd_atan(MOTORBIKE_FRONT, heightFrontMiddle - motorbikeItem->Pose.Position.y);
				zRot = phd_atan(MOTORBIKE_SIDE, r2 - frontLeft.y);
			}
		}
		else if (r1 >= ((heightFrontLeft + heightFrontRight) / 2))
		{
			xRot = phd_atan(MOTORBIKE_FRONT, motorbikeItem->Pose.Position.y - r1);
			zRot = phd_atan(MOTORBIKE_SIDE, r2 - frontLeft.y);
		}
		else
		{
			xRot = phd_atan(125, newY - motorbikeItem->Pose.Position.y);
			zRot = phd_atan(MOTORBIKE_SIDE, r2 - frontLeft.y);
		}

		motorbikeItem->Pose.Orientation.x += (xRot - motorbikeItem->Pose.Orientation.x) / 4;
		motorbikeItem->Pose.Orientation.z += (zRot - motorbikeItem->Pose.Orientation.z) / 4;

		if (probe.GetRoomNumber() != motorbikeItem->RoomNumber)
		{
			ItemNewRoom(lara->Context.Vehicle, probe.GetRoomNumber());
			ItemNewRoom(laraItem->Index, probe.GetRoomNumber());
		}

		laraItem->Pose = motorbikeItem->Pose;

		AnimateMotorbike(motorbikeItem, laraItem, collide, isDead);
		AnimateItem(*laraItem);
		SyncVehicleAnim(*motorbikeItem, *laraItem);

		Camera.targetElevation = -ANGLE(30.0f);

		if (motorbike->Flags & MOTORBIKE_FLAG_FALLING)
		{
			if (motorbikeItem->Pose.Position.y == motorbikeItem->Floor)
			{
				ExplodeVehicle(laraItem, motorbikeItem);
				return 0;
			}
		}

		if (laraItem->Animation.ActiveState == MOTORBIKE_STATE_DISMOUNT)
		{
			motorbike->ExhaustStart = false;
			MotorBikeCheckGetOff(laraItem);
			return true;
		}

		MotorBikeCheckGetOff(laraItem);
		return true;
	}
}
