#include "framework.h"
#include "Objects/TR5/Entity/tr5_guard.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/box.h"
#include "Game/control/los.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR5
{
	constexpr auto GUARD_ALERT_RANGE  = SQUARE(BLOCK(1));
	constexpr auto GUARD_WALK_RANGE	  = SQUARE(BLOCK(3));
	constexpr auto GUARD_ATTACK_RANGE = SQUARE(BLOCK(4));

	constexpr auto GUARD_WALK_TURN_RATE_MAX	   = ANGLE(5.0f);
	constexpr auto GUARD_RUN_TURN_RATE_MAX	   = ANGLE(10.0f);
	constexpr auto GUARD_LARA_ANGLE_FOR_DEATH2 = ANGLE(67.5f);

	constexpr auto GUARD_NO_WEAPON_ON_HAND_SWAPFLAG = 0x2000;
	constexpr auto GUARD_HEAD_MESH = 14;

	const auto SwatGunBite				= CreatureBiteInfo(Vector3(16, 240, 90), 13);
	const auto MafiaGunBite				= CreatureBiteInfo(Vector3(16, 270, 90), 13);
	const auto SniperGunBite			= CreatureBiteInfo(Vector3(0, 480, 110), 13);
	const auto ArmedMafia2GunLeftBite	= CreatureBiteInfo(Vector3(-16, 200, 60), 10);
	const auto ArmedMafia2GunRightBite	= CreatureBiteInfo(Vector3(16, 200, 60), 13);

	// TODO: Revise names of enum elements.

	enum GuardState
	{
		// No state 0.
		GUARD_STATE_IDLE = 1,
		GUARD_STATE_TURN_180_1 = 2,
		GUARD_STATE_SINGLE_FIRE_ATTACK = 3,
		GUARD_STATE_AIM = 4,
		GUARD_STATE_WALK_FORWARD = 5,
		GUARD_STATE_DEATH_1 = 6,
		GUARD_STATE_RUN_FORWARD = 7,
		GUARD_STATE_DEATH_2 = 8,
		GUARD_STATE_RELOAD = 11,
		GUARD_STATE_THROW_GRENADE = 12,
		GUARD_STATE_CROUCH = 13,
		GUARD_STATE_ROPE_DOWN = 14,
		GUARD_STATE_SIT = 15,
		GUARD_STATE_STAND_UP = 16,
		GUARD_STATE_SLEEP_ON_CHAIR_LOOP = 17,
		GUARD_STATE_AWAKE_FROM_SLEEP = 18, // Includes kick to the chair.
		GUARD_STATE_WAITING_ON_WALL = 19,
		GUARD_STATE_VAULT_2_STEPS_UP = 20,
		GUARD_STATE_VAULT_3_STEPS_UP = 21,
		GUARD_STATE_VAULT_4_STEPS_UP = 22,
		GUARD_STATE_VAULT_4_STEPS_DOWN = 23,
		GUARD_STATE_VAULT_3_STEPS_DOWN = 24,
		GUARD_STATE_VAULT_2_STEPS_DOWN = 25,

		GUARD_STATE_IDLE_START_JUMP = 26,
		GUARD_STATE_JUMP_1_BLOCK = 27,
		GUARD_STATE_JUMP_2_BLOCKS = 28,
		GUARD_STATE_LAND = 29,
		GUARD_STATE_HUNT = 30,
		GUARD_STATE_HUNT_TO_IDLE = 31,
		GUARD_STATE_TURN_180_2 = 32,

		GUARD_STATE_RAPID_FIRE_ATTACK = 35,
		GUARD_STATE_INSERT_CODE = 36,
		GUARD_STATE_USE_KEYPAD = 37,
		GUARD_STATE_USE_COMPUTER = 38,
		GUARD_STATE_SURRENDER = 39,
	};

	enum Mafia2State
	{
		// No state 0.
		MAFIA2_STATE_IDLE = 1,
		MAFIA2_STATE_TURN_180_UNDRAW_WEAPON = 2,
		MAFIA2_STATE_FIRE = 3,
		MAFIA2_STATE_AIM = 4,
		MAFIA2_STATE_WALK = 5,
		MAFIA2_STATE_DEATH_1 = 6,
		MAFIA2_STATE_RUN = 7,
		MAFIA2_STATE_DEATH_2 = 8,

		MAFIA2_STATE_IDLE_START_JUMP = 26,
		MAFIA2_STATE_JUMP_1_BLOCK = 27,
		MAFIA2_STATE_JUMP_2_BLOCKS = 28,
		MAFIA2_STATE_LAND = 29,

		MAFIA2_STATE_TURN_180 = 32,

		MAFIA2_STATE_UNDRAW_GUNS = 37
	};

	enum SniperState
	{
		// No state 0.
		SNIPER_STATE_IDLE = 1,
		SNIPER_STATE_UNCOVER = 2,
		SNIPER_STATE_AIM = 3,
		SNIPER_STATE_FIRE = 4,
		SNIPER_STATE_COVER = 5,
		SNIPER_STATE_DEATH = 6
	};

	enum GuardAnim
	{
		GUARD_ANIM_IDLE = 0,
		GUARD_ANIM_TURN_180_1 = 1,
		GUARD_ANIM_SINGLE_FIRE_ATTACK = 2,
		GUARD_ANIM_AIM = 3,
		GUARD_ANIM_START_WALK = 4,
		GUARD_ANIM_WALK_FORWARD = 5,
		GUARD_ANIM_WALK_FORWARD_TO_IDLE = 6,
		GUARD_ANIM_WALK_FORWARD_TO_AIM = 7,
		GUARD_ANIM_AIM_TO_WALK = 8,
		GUARD_ANIM_AIM_TO_STOP = 9,
		GUARD_ANIM_STOP_TO_AIM = 10,
		GUARD_ANIM_DEATH_1 = 11,
		GUARD_ANIM_RUN_FORWARD = 12,
		GUARD_ANIM_AIM_TO_RUN = 13,
		GUARD_ANIM_RUN_FORWARD_TO_AIM = 14,
		GUARD_ANIM_RUN_FORWARD_TO_WALK = 15,
		GUARD_ANIM_DEATH_2 = 16,
		GUARD_ANIM_WALK_FORWARD_TO_RUN = 17,
		GUARD_ANIM_STOP_TO_RUN = 18,
		GUARD_ANIM_CROUCH_AIM_HAND_UP_START = 19,
		GUARD_ANIM_CROUCH_AIM_HAND_UP = 20,
		GUARD_ANIM_CROUCH_HAND_UP_SINGLE_SHOOT = 21,
		GUARD_ANIM_CROUCH_HAND_UP_TO_IDLE = 22,
		GUARD_ANIM_RELOAD = 23,
		GUARD_ANIM_THROW_GRENADE = 24,
		GUARD_ANIM_OPEN_DOOR_KICK = 25,
		GUARD_ANIM_ROPE_DOWN_FAST = 26,
		GUARD_ANIM_ROPE_DOWN_FAST_TO_AIM_FRONT = 27,
		GUARD_ANIM_ROPE_DOWN = 28,
		GUARD_ANIM_ROPE_TO_AIM = 29,
		GUARD_ANIM_SLEEPING = 30,
		GUARD_ANIM_SLEEPING_TO_AIM = 31, // Kick a chair. Must find chair nearby before doing this anim.
		GUARD_ANIM_WAITING_ON_WALL = 32,
		GUARD_ANIM_WAITING_ON_WALL_TO_IDLE = 33,
		GUARD_ANIM_UNKNOWN = 34,
		GUARD_ANIM_VAULT_2_STEPS_UP = 35,
		GUARD_ANIM_VAULT_3_STEPS_UP = 36,
		GUARD_ANIM_VAULT_4_STEPS_UP = 37,
		GUARD_ANIM_VAULT_4_STEPS_DOWN = 38,
		GUARD_ANIM_VAULT_3_STEPS_DOWN = 39,
		GUARD_ANIM_VAULT_2_STEPS_DOWN = 40,
		GUARD_ANIM_JUMP_1_BLOCK = 41,
		GUARD_ANIM_JUMP_END = 42,
		GUARD_ANIM_JUMP_2_BLOCKS = 43,
		GUARD_ANIM_HUNT_WALK_FORWARD = 44,
		GUARD_ANIM_HUNT = 45,
		GUARD_ANIM_HUNT_TO_IDLE = 46,
		GUARD_ANIM_STOP_TO_HUNT = 47,
		GUARD_ANIM_TURN_180_2 = 48,
		GUARD_ANIM_JUMP_START = 49,
		GUARD_ANIM_STOP_TO_CROUCH_AIM_HAND_DOWN = 50,
		GUARD_ANIM_CROUCH_AIM_HAND_DOWN = 51,
		GUARD_ANIM_CROUCH_HAND_DOWN_SHOOT_FAST = 52,
		GUARD_ANIM_CROUCH_HAND_DOWN_TO_IDLE = 53,
		GUARD_ANIM_RAPID_FIRE_ATTACK = 54,
		GUARD_ANIM_INSERT_CODE = 55,
		GUARD_ANIM_USE_KEYPAD = 56,
		GUARD_ANIM_USE_COMPUTER_START = 57,
		GUARD_ANIM_USE_COMPUTER = 58,
		GUARD_ANIM_USE_COMPUTER_TO_IDLE = 59,
		GUARD_ANIM_SURRENDER_START = 60,
		GUARD_ANIM_SURRENDER = 61,
		GUARD_ANIM_SURRENDER_TO_IDLE = 62
	};

	enum class GuardOcb
	{
		None = 0,
		Reload = 1,
		DoorKick = 2,
		RopeDown = 3,
		Sleeping = 4,
		RopeDownFast = 5,
		WaitOnWall = 6,
		UseComputer = 7,
		StartHuntStop = 8,
		UseComputerScientist = 9,
		Idle = 10,
		Run = 11
	};

	void InitializeGuard(short itemNum)
	{
		auto* item = &g_Level.Items[itemNum];
		short roomItemNumber;

		InitializeCreature(itemNum);

		switch ((GuardOcb)item->TriggerFlags)
		{
		default:
		case GuardOcb::Idle:
			SetAnimation(*item, GUARD_ANIM_IDLE);
			break;

		case GuardOcb::Reload:
			SetAnimation(*item, GUARD_ANIM_RELOAD);
			break;

		case GuardOcb::DoorKick:
			SetAnimation(*item, GUARD_ANIM_OPEN_DOOR_KICK);
			item->Animation.TargetState = GUARD_STATE_CROUCH;
			// TODO: item->flags2 ^= (item->flags2 ^ ((item->flags2 & 0xFE) + 2)) & 6;
			break;

		case GuardOcb::RopeDown:
			SetAnimation(*item, GUARD_ANIM_ROPE_DOWN);
			item->SetMeshSwapFlags(9216);

			roomItemNumber = g_Level.Rooms[item->RoomNumber].itemNumber;
			if (roomItemNumber != NO_VALUE)
			{
				ItemInfo* item2 = nullptr;
				while (true)
				{
					item2 = &g_Level.Items[roomItemNumber];
					if (item2->ObjectNumber >= ID_ANIMATING1 &&
						item2->ObjectNumber <= ID_ANIMATING15 &&
						item2->RoomNumber == item->RoomNumber &&
						item2->TriggerFlags == (int)GuardOcb::RopeDown)
					{
						break;
					}

					roomItemNumber = item2->NextItem;
					if (roomItemNumber == NO_VALUE)
					{
						item->Animation.FrameNumber = 0;
						item->Animation.ActiveState = item->Animation.TargetState;
						break;
					}
				}

				item2->MeshBits = -5;
			}

			break;

		case GuardOcb::Sleeping:
			SetAnimation(*item, GUARD_ANIM_SLEEPING);
			item->SetMeshSwapFlags(GUARD_NO_WEAPON_ON_HAND_SWAPFLAG);
			break;

		case GuardOcb::RopeDownFast:
			SetAnimation(*item, GUARD_ANIM_ROPE_DOWN_FAST);
			item->Pose.Position.y = GetPointCollision(*item).GetCeilingHeight() - BLOCK(2);
			break;

		case GuardOcb::WaitOnWall:
			SetAnimation(*item, GUARD_ANIM_WAITING_ON_WALL);
			break;

		case GuardOcb::UseComputer:
		case GuardOcb::UseComputerScientist:
			SetAnimation(*item, GUARD_ANIM_USE_COMPUTER);
			item->Pose.Position.x -= CLICK(2) * phd_sin(item->Pose.Orientation.y);
			item->Pose.Position.z -= CLICK(2) * phd_cos(item->Pose.Orientation.y);
			break;

		case GuardOcb::StartHuntStop:
			SetAnimation(*item, GUARD_ANIM_HUNT_TO_IDLE);
			break;

		case GuardOcb::Run:
			SetAnimation(*item, GUARD_ANIM_RUN_FORWARD);
			break;
		}
	}

	void InitializeSniper(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(*item, 0);
		item->Pose.Position.x += BLOCK(1) * phd_sin(item->Pose.Orientation.y + ANGLE(90.0f));
		item->Pose.Position.y += CLICK(2);
		item->Pose.Position.z += BLOCK(1) * phd_cos(item->Pose.Orientation.y + ANGLE(90.0f));
	}

	void InitializeGuardLaser(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(*item, 6);
	}

	void ControlGuardLaser(short itemNumber)
	{

	}

	// TODO: Deal with LaraItem global.
	void GuardControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short torsoX = 0;
		short torsoY = 0;
		short headY = 0;

		bool canJump1block = CanCreatureJump(*item, JumpDistance::Block1);
		bool canJump2blocks = !canJump1block && CanCreatureJump(*item, JumpDistance::Block2);

		if (item->AIBits)
			GetAITarget(creature);
		else
			creature->Enemy = LaraItem;

		if (creature->MuzzleFlash[0].Delay != 0)
			creature->MuzzleFlash[0].Delay--;

		AI_INFO AI;
		CreatureAIInfo(item, &AI);

		AI_INFO laraAI;
		if (creature->Enemy == LaraItem)
		{
			laraAI.angle = AI.angle;
			laraAI.distance = AI.distance;
		}
		else
		{
			int dx = LaraItem->Pose.Position.x - item->Pose.Position.x;
			int dz = LaraItem->Pose.Position.z - item->Pose.Position.z;
			laraAI.angle = phd_atan(dz, dx) - item->Pose.Orientation.y;
			laraAI.distance = SQUARE(dz) + SQUARE(dx);
		}
	
		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != GUARD_STATE_DEATH_1 &&
				item->Animation.ActiveState != GUARD_STATE_DEATH_2)
			{
				if (laraAI.angle >= GUARD_LARA_ANGLE_FOR_DEATH2 || laraAI.angle <= -GUARD_LARA_ANGLE_FOR_DEATH2)
				{
					SetAnimation(*item, GUARD_ANIM_DEATH_2);
					item->Pose.Orientation.y += laraAI.angle + ANGLE(-180.0f);
				}
				else
				{
					SetAnimation(*item, GUARD_ANIM_DEATH_1);
					item->Pose.Orientation.y += laraAI.angle;
				}

				item->Animation.FrameNumber = 0;
			}
		}
		else
		{
			GetCreatureMood(item, &AI, creature->Enemy != LaraItem);
			if (item->ObjectNumber == ID_SCIENTIST)
			{
				if (item->HitPoints >= Objects[ID_SCIENTIST].HitPoints)
				{
					if (creature->Enemy == LaraItem)
						creature->Mood = MoodType::Bored;
				}
				else
					creature->Mood = MoodType::Escape;
			}

			if (TestEnvironment(ENV_FLAG_NO_LENSFLARE, item->RoomNumber)) // TODO: CHECK
			{
				if (item->ObjectNumber == ID_SWAT_PLUS)
				{
					item->ItemFlags[0]++;
					if (item->ItemFlags[0] > 60 && Random::TestProbability(0.06f))
					{
						SoundEffect(SFX_TR5_BIO_BREATHE_OUT, &item->Pose);
						item->ItemFlags[0] = 0;
					}
				}
				else
				{
					creature->Mood = MoodType::Escape;

					if (!(GlobalCounter & 7))
						item->HitPoints--;

					if (item->HitPoints <= 0)
						SetAnimation(*item, GUARD_ANIM_DEATH_2);
				}
			}

			CreatureMood(item, &AI, creature->Enemy != LaraItem);
			auto* enemy = creature->Enemy;

			angle = CreatureTurn(item, creature->MaxTurn);
			creature->Enemy = LaraItem;

			if ((laraAI.distance < GUARD_ALERT_RANGE && LaraItem->Animation.Velocity.z > 20) ||
				item->HitStatus ||
				TargetVisible(item, &laraAI))
			{
				if (!(item->AIBits & FOLLOW) &&
					item->ObjectNumber != ID_SCIENTIST &&
					abs(item->Pose.Position.y - LaraItem->Pose.Position.y) < BLOCK(1.25f))
				{
					creature->Enemy = LaraItem; // TODO: deal with LaraItem global !
					AlertAllGuards(itemNumber);
				}
			}

			creature->Enemy = enemy;

			auto origin = GameVector(
				item->Pose.Position.x,
				item->Pose.Position.y - CLICK(1.5f),
				item->Pose.Position.z,
				item->RoomNumber);
			
			// TODO: Deal with LaraItem global.
			auto& bounds = GetClosestKeyframe(*LaraItem).BoundingBox;
			auto target = GameVector(
				LaraItem->Pose.Position.x,
				LaraItem->Pose.Position.y + ((bounds.Y2 + 3 * bounds.Y1) / 4),
				LaraItem->Pose.Position.z,
				LaraItem->RoomNumber);

			bool los = !LOS(&origin, &target) && item->TriggerFlags != (int)GuardOcb::Idle;
			ItemInfo* currentItem = nullptr;
			short currentItemNumber;

			switch (item->Animation.ActiveState)
			{
			case GUARD_STATE_IDLE:
				creature->LOT.IsJumping = false;
				creature->Flags = 0;
				headY = laraAI.angle;

				if (AI.ahead)
				{
					if (!(item->AIBits & FOLLOW))
					{
						torsoX = AI.angle / 2;
						torsoY = AI.xAngle;
					}
				}

				if (item->ObjectNumber == ID_SCIENTIST && item == Lara.TargetEntity)
					item->Animation.TargetState = GUARD_STATE_SURRENDER;
				else if (item->Animation.RequiredState != NO_VALUE)
					item->Animation.TargetState = item->Animation.RequiredState;
				else if (item->AIBits & GUARD)
				{
					if (item->AIBits & MODIFY)
						headY = 0;
					else
						headY = AIGuard(creature);
			
					if (item->AIBits & PATROL1)
					{
						item->TriggerFlags--;
						if (item->TriggerFlags <= (int)GuardOcb::None)
							item->AIBits &= ~GUARD;
					}
				}
				else if (creature->Enemy == LaraItem &&
					(laraAI.angle > ANGLE(112.5f) || laraAI.angle < -ANGLE(112.5f)) &&
					item->ObjectNumber != ID_SCIENTIST)
				{
					item->Animation.TargetState = GUARD_STATE_TURN_180_1;
				}
				else if (item->AIBits & PATROL1)
					item->Animation.TargetState = GUARD_STATE_WALK_FORWARD;
				else if (item->AIBits & AMBUSH)
					item->Animation.TargetState = GUARD_STATE_RUN_FORWARD;
				else if (Targetable(item, &AI) && item->ObjectNumber != ID_SCIENTIST)
				{
					if (AI.distance >= GUARD_ATTACK_RANGE  && AI.zoneNumber == AI.enemyZone)
					{
						if (!(item->AIBits & MODIFY))
							item->Animation.TargetState = GUARD_STATE_WALK_FORWARD;
					}
					else
						item->Animation.TargetState = GUARD_STATE_AIM;
				}
				else if (canJump1block || canJump2blocks)
				{
					creature->MaxTurn = 0;
					SetAnimation(*item, GUARD_ANIM_JUMP_START);

					if (canJump1block)
						item->Animation.TargetState = GUARD_STATE_JUMP_1_BLOCK;
					else
						item->Animation.TargetState = GUARD_STATE_JUMP_2_BLOCKS;

					creature->LOT.IsJumping = true;
				}
				else if (los)
					item->Animation.TargetState = GUARD_STATE_HUNT_TO_IDLE;
				else if (creature->Mood != MoodType::Bored)
				{
					if (AI.distance < GUARD_WALK_RANGE || item->AIBits & FOLLOW)
						item->Animation.TargetState = GUARD_STATE_WALK_FORWARD;
					else
						item->Animation.TargetState = GUARD_STATE_RUN_FORWARD;
				}
				else
					item->Animation.TargetState = GUARD_STATE_IDLE;
			
				if (item->TriggerFlags == (int)GuardOcb::Run)
					item->TriggerFlags = (int)GuardOcb::None;

				break;

			case GUARD_STATE_TURN_180_1:
				creature->Flags = 0;

				if (AI.angle >= 0)
					item->Pose.Orientation.y -= ANGLE(2.0f);
				else
					item->Pose.Orientation.y += ANGLE(2.0f);

				if (TestLastFrame(*item))
					item->Pose.Orientation.y += -ANGLE(180.0f);

				break;

			case GUARD_STATE_SINGLE_FIRE_ATTACK:
			case GUARD_STATE_RAPID_FIRE_ATTACK:
				creature->MaxTurn = 0;
				torsoX = laraAI.angle / 2;
				headY = laraAI.angle / 2;

				if (AI.ahead)
					torsoY = AI.xAngle;

				if (abs(AI.angle) >= ANGLE(2.0f))
				{
					if (AI.angle >= 0)
						item->Pose.Orientation.y += ANGLE(2.0f);
					else
						item->Pose.Orientation.y -= ANGLE(2.0f);
				}
				else
					item->Pose.Orientation.y += AI.angle;

				if (item->Animation.ActiveState == GUARD_STATE_RAPID_FIRE_ATTACK)
				{
					if (creature->Flags)
					{
						if (item->Animation.FrameNumber < 10 &&
							(item->Animation.FrameNumber & 1))
						{
							creature->Flags = 0;
						}
					}
				}

				if (creature->Flags == 0)
				{
					if (item->ObjectNumber == ID_MAFIA)
					{
						if (item->Animation.ActiveState == GUARD_STATE_SINGLE_FIRE_ATTACK)
						{
							ShotLara(item, &AI, MafiaGunBite, torsoX, 30);
						}
						else
						{
							ShotLara(item, &AI, MafiaGunBite, torsoX, 10);
						}

						creature->MuzzleFlash[0].Bite = MafiaGunBite;
						creature->MuzzleFlash[0].Delay = 2;
					}
					else
					{
						if (item->Animation.ActiveState == GUARD_STATE_SINGLE_FIRE_ATTACK)
						{
							ShotLara(item, &AI, SwatGunBite, torsoX, 30);
						}
						else
						{
							ShotLara(item, &AI, SwatGunBite, torsoX, 10);
						}

						creature->MuzzleFlash[0].Bite = SwatGunBite;
						creature->MuzzleFlash[0].Delay = 2;
					}
					
					creature->Flags = 1;
				}

				break;

			case GUARD_STATE_AIM:
				creature->MaxTurn = 0;
				creature->Flags = 0;
				torsoX = laraAI.angle / 2;
				headY = laraAI.angle / 2;

				if (AI.ahead)
					torsoY = AI.xAngle;

				if (abs(AI.angle) >= ANGLE(2.0f))
				{
					if (AI.angle >= 0)
						item->Pose.Orientation.y += ANGLE(2.0f);
					else
						item->Pose.Orientation.y -= ANGLE(2.0f);
				}
				else
				{
					item->Pose.Orientation.y += AI.angle;
				}

				if (!Targetable(item, &AI))
					item->Animation.TargetState = GUARD_STATE_IDLE;
				else if (item->ObjectNumber == ID_GUARD1 || item->ObjectNumber == ID_GUARD2)
					item->Animation.TargetState = GUARD_STATE_SINGLE_FIRE_ATTACK;
				else
					item->Animation.TargetState = GUARD_STATE_RAPID_FIRE_ATTACK;

				break;

			case GUARD_STATE_WALK_FORWARD:
				creature->MaxTurn = GUARD_WALK_TURN_RATE_MAX;
				creature->LOT.IsJumping = false;

				if (!Targetable(item, &AI) ||
					AI.distance >= GUARD_ATTACK_RANGE && AI.zoneNumber == AI.enemyZone ||
					item->ObjectNumber == ID_SCIENTIST ||
					item->AIBits & AMBUSH || item->AIBits & PATROL1) // TODO: CHECK
				{
					if (canJump1block || canJump2blocks)
					{
						creature->MaxTurn = 0;
						SetAnimation(*item, GUARD_ANIM_JUMP_START);

						if (canJump1block)
							item->Animation.TargetState = GUARD_STATE_JUMP_1_BLOCK;
						else
							item->Animation.TargetState = GUARD_STATE_JUMP_2_BLOCKS;

						creature->LOT.IsJumping = true;
					}
					else if (AI.distance >= SQUARE(BLOCK(1)))
					{
						if (!los || item->AIBits)
						{
							if (AI.distance > GUARD_WALK_RANGE)
							{
								if (!(item->InDrawRoom))
									item->Animation.TargetState = GUARD_STATE_RUN_FORWARD;
							}
						}
						else
							item->Animation.TargetState = GUARD_STATE_IDLE;
					}
					else
						item->Animation.TargetState = GUARD_STATE_IDLE;
				}
				else
					item->Animation.TargetState = GUARD_STATE_AIM;
			
				break;

			case GUARD_STATE_RUN_FORWARD:
				creature->MaxTurn = GUARD_RUN_TURN_RATE_MAX;
				creature->LOT.IsJumping = false;

				if (Targetable(item, &AI) && (AI.distance < GUARD_ATTACK_RANGE || AI.enemyZone == AI.zoneNumber) && item->ObjectNumber != ID_SCIENTIST)
				{
					item->Animation.TargetState = GUARD_STATE_AIM;
				}
				else if (canJump1block || canJump2blocks)
				{
					creature->MaxTurn = 0;
					SetAnimation(*item, GUARD_ANIM_JUMP_START);

					if (canJump1block)
						item->Animation.TargetState = GUARD_STATE_JUMP_1_BLOCK;
					else
						item->Animation.TargetState = GUARD_STATE_JUMP_2_BLOCKS;

					creature->LOT.IsJumping = true;
				}
				else if (los)
					item->Animation.TargetState = GUARD_STATE_IDLE;
				else if (AI.distance < GUARD_WALK_RANGE)
					item->Animation.TargetState = GUARD_STATE_WALK_FORWARD;

				if (item->TriggerFlags == (int)GuardOcb::Run) // TODO: why is this set for run ? he is not jumping this time ! - TokyoSU: 24/12/2022
				{
					creature->MaxTurn = 0;
					creature->LOT.IsJumping = true;
				}
			
				break;

			case GUARD_STATE_ROPE_DOWN:
				creature->MaxTurn = 0;
				headY = laraAI.angle;

				if (item->Pose.Position.y <= (item->Floor - BLOCK(2)) || item->TriggerFlags != (int)GuardOcb::RopeDownFast)
				{
					if (item->Pose.Position.y >= (item->Floor - CLICK(2)))
						item->Animation.TargetState = GUARD_STATE_AIM;
				}
				else
				{
					item->TriggerFlags = (int)GuardOcb::None;
					TestTriggers(item, true);
					SoundEffect(SFX_TR4_LARA_POLE_SLIDE_LOOP, &item->Pose);
				}
				if (abs(AI.angle) >= ANGLE(2.0f))
				{
					if ((AI.angle & 0x8000) == 0)
						item->Pose.Orientation.y += ANGLE(2.0f);
					else
						item->Pose.Orientation.y -= ANGLE(2.0f);
				}
				else
					item->Pose.Orientation.y += AI.angle;
			
				break;

			case GUARD_STATE_SIT:
				creature->MaxTurn = 0;
				headY = AIGuard(creature);

				if (creature->Alerted)
					item->Animation.TargetState = GUARD_STATE_STAND_UP;

				break;

			case GUARD_STATE_STAND_UP:
			case GUARD_STATE_AWAKE_FROM_SLEEP:
				creature->MaxTurn = 0;
				if (item->Animation.FrameNumber == 0)
				{
					TestTriggers(item, true);
					break;
				}

				if (item->Animation.FrameNumber == 44)
				{
					item->SetMeshSwapFlags(NO_JOINT_BITS);

					short currentItemNumber = g_Level.Rooms[item->RoomNumber].itemNumber;
					if (currentItemNumber == NO_VALUE)
						break;

					while (true)
					{
						currentItem = &g_Level.Items[currentItemNumber];

						if (currentItem->ObjectNumber >= ID_ANIMATING1 &&
							currentItem->ObjectNumber <= ID_ANIMATING15 &&
							currentItem->RoomNumber == item->RoomNumber)
						{
							if (currentItem->TriggerFlags > (int)GuardOcb::DoorKick && currentItem->TriggerFlags < (int)GuardOcb::RopeDownFast)
								break;
						}

						currentItemNumber = currentItem->NextItem;
						if (currentItemNumber == NO_VALUE)
							break;
					}

					if (currentItemNumber == NO_VALUE)
						break;

					currentItem->MeshBits = -3;
				}
				else if (TestLastFrame(*item))
				{
					item->Pose.Orientation.y -= ANGLE(90.0f);
				}
			
				break;

			case GUARD_STATE_SLEEP_ON_CHAIR_LOOP:
				creature->MaxTurn = 0;
				headY = 0;

				if (!item->HitStatus &&
					LaraItem->Animation.Velocity.z < 40 &&
					!Lara.Control.Weapon.HasFired)
				{
					creature->Alerted = false;
				}

				if (creature->Alerted)
					item->Animation.TargetState = GUARD_STATE_AWAKE_FROM_SLEEP;

				break;

			case GUARD_STATE_WAITING_ON_WALL:
				creature->MaxTurn = 0;
				headY = AIGuard(creature);

				if (creature->Alerted)
					item->Animation.TargetState = GUARD_STATE_IDLE;

				break;

			case GUARD_STATE_HUNT:
			case GUARD_STATE_HUNT_TO_IDLE:
				creature->MaxTurn = GUARD_WALK_TURN_RATE_MAX;
				creature->LOT.IsJumping = false;

				if (item->Animation.ActiveState == GUARD_STATE_HUNT_TO_IDLE)
				{
					if (item->TriggerFlags != (int)GuardOcb::StartHuntStop || !los || item->HitStatus)
						item->Animation.TargetState = GUARD_STATE_HUNT;
				}

				if (canJump1block || canJump2blocks || AI.distance < SQUARE(BLOCK(1)) || !los || item->HitStatus)
					item->Animation.TargetState = GUARD_STATE_IDLE;

				break;

			case GUARD_STATE_INSERT_CODE:
				creature->MaxTurn = 0;
				if (item->Animation.FrameNumber == 39)
					TestTriggers(item, true);
			
				break;

			case GUARD_STATE_USE_KEYPAD:
				creature->MaxTurn = 0;
				currentItem = nullptr;

				for (currentItemNumber = g_Level.Rooms[item->RoomNumber].itemNumber; currentItemNumber != NO_VALUE; currentItemNumber = currentItem->NextItem)
				{
					currentItem = &g_Level.Items[currentItemNumber];
					if (item->ObjectNumber == ID_PUZZLE_HOLE8) // TODO: Avoid hardcoded object number. -- TokyoSU 24/12/2022
						break;
				}

				if (item->Animation.FrameNumber == 0)
				{
					currentItem->MeshBits = 0x1FFF;
					item->Pose.Position.x = currentItem->Pose.Position.x - CLICK(1);
					item->Pose.Orientation.y = currentItem->Pose.Orientation.y;
					item->Pose.Position.z = currentItem->Pose.Position.z + CLICK(0.5f);
					item->SetMeshSwapFlags(1024);
				}
				else
				{
					if (item->Animation.FrameNumber == 32)
					{
						currentItem->MeshBits = 16381;
					}
					else if (item->Animation.FrameNumber == 74)
					{
						currentItem->MeshBits = 278461;
					}
					else if (item->Animation.FrameNumber == 120)
					{
						currentItem->MeshBits = 802621;
					}
					else if (item->Animation.FrameNumber ==  157)
					{
						currentItem->MeshBits = 819001;
					}
					else if (item->Animation.FrameNumber == 190)
					{
						currentItem->MeshBits = 17592121;
					}
					else if (TestLastFrame(*item))
					{
						currentItem->MeshBits = 0x1FFF;
						TestTriggers(item, true);
						item->Animation.RequiredState = GUARD_STATE_WALK_FORWARD;
						item->SetMeshSwapFlags(NO_JOINT_BITS);
					}
				}

				break;

			case GUARD_STATE_USE_COMPUTER:
				creature->MaxTurn = 0;

				if ((item->ObjectNumber != ID_SCIENTIST || item != Lara.TargetEntity) &&
					(Random::TestProbability(0.992f) || item->TriggerFlags >= (int)GuardOcb::Idle || item->TriggerFlags == (int)GuardOcb::UseComputerScientist))
				{
					if (item->AIBits & GUARD)
					{
						headY = AIGuard(creature);

						if (item->AIBits & PATROL1)
						{
							item->TriggerFlags--;
							if (item->TriggerFlags <= (int)GuardOcb::None)
								item->AIBits = PATROL1 | MODIFY;
						}
					}
				}
				else
					item->Animation.TargetState = GUARD_STATE_IDLE;
			
				break;

			case GUARD_STATE_SURRENDER:
				creature->MaxTurn = 0;

				if (item != Lara.TargetEntity && Random::TestProbability(1 / 64.0f))
				{
					if (item->TriggerFlags == (int)GuardOcb::UseComputer || item->TriggerFlags == (int)GuardOcb::UseComputerScientist)
						item->Animation.RequiredState = GUARD_STATE_USE_COMPUTER;

					item->Animation.TargetState = GUARD_STATE_IDLE;
				}

				if (item->Animation.FrameNumber == 39)
					TestTriggers(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z, enemy->RoomNumber, true);

				break;
			}
		}

		CreatureJoint(item, 0, torsoX);
		CreatureJoint(item, 1, torsoY);
		CreatureJoint(item, 2, headY);

		if (creature->ReachedGoal && creature->Enemy)
		{
			auto enemy = creature->Enemy;
		
			if (enemy->Flags != 4)
			{
				if (enemy->Flags & 0x10)
				{
					item->Animation.TargetState = GUARD_STATE_IDLE;
					item->Animation.RequiredState = GUARD_STATE_USE_COMPUTER;
					item->TriggerFlags = 300;
					item->AIBits = GUARD | PATROL1;
				}
				else
				{
					if (enemy->Flags & 0x20)
					{
						item->Animation.TargetState = GUARD_STATE_IDLE;
						item->Animation.RequiredState = GUARD_STATE_INSERT_CODE;
						item->AIBits = PATROL1 | MODIFY;
					}
					else
					{
						TestTriggers(enemy, true);
						item->Animation.RequiredState = GUARD_STATE_WALK_FORWARD;

						if (creature->Enemy->Flags & 2)
							item->ItemFlags[3] = (creature->Tosspad & 0xFF) - 1;

						if (creature->Enemy->Flags & 8)
						{
							item->Animation.RequiredState = GUARD_STATE_IDLE;
							item->TriggerFlags = 300;
							item->AIBits |= GUARD | PATROL1;
						}
					}
				}
			}
			else
			{
				item->Animation.TargetState = GUARD_STATE_IDLE;
				item->Animation.RequiredState = GUARD_STATE_USE_KEYPAD;
			}
		}
		if ((item->Animation.ActiveState >= GUARD_STATE_VAULT_2_STEPS_UP ||
			item->Animation.ActiveState == GUARD_STATE_DEATH_1 ||
			item->Animation.ActiveState == GUARD_STATE_DEATH_2) &&
			item->Animation.ActiveState != GUARD_STATE_HUNT)
		{
			CreatureAnimation(itemNumber, angle, 0);
		}
		else
		{
			switch (CreatureVault(itemNumber, angle, 2, BLOCK(0.25f)))
			{
			case 2:
				SetAnimation(*item, GUARD_ANIM_VAULT_2_STEPS_UP);
				creature->MaxTurn = 0;
				break;

			case 3:
				SetAnimation(*item, GUARD_ANIM_VAULT_3_STEPS_UP);
				creature->MaxTurn = 0;
				break;

			case 4:
				SetAnimation(*item, GUARD_ANIM_VAULT_4_STEPS_UP);
				creature->MaxTurn = 0;
				break;

			case -2:
				SetAnimation(*item, GUARD_ANIM_VAULT_2_STEPS_DOWN);
				creature->MaxTurn = 0;
				break;

			case -3:
				SetAnimation(*item, GUARD_ANIM_VAULT_3_STEPS_DOWN);
				creature->MaxTurn = 0;
				break;

			case -4:
				SetAnimation(*item, GUARD_ANIM_VAULT_4_STEPS_DOWN);
				creature->MaxTurn = 0;
				break;
			}
		}
	}

	void SniperControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short joint0 = 0;
		short joint1 = 0;
		short joint2 = 0;

		if (creature->MuzzleFlash[0].Delay != 0)
			creature->MuzzleFlash[0].Delay--;

		if (item->HitPoints > 0)
		{
			if (item->AIBits)
				GetAITarget(creature);
			else if (creature->HurtByLara)
				creature->Enemy = LaraItem;
		
			AI_INFO AI;
			CreatureAIInfo(item, &AI);

			GetCreatureMood(item, &AI, true);
			CreatureMood(item, &AI, true);

			angle = CreatureTurn(item, creature->MaxTurn);
			creature->MaxTurn = 0;

			if (AI.ahead)
			{
				joint0 = AI.angle / 2;
				joint1 = AI.xAngle;
				joint2 = AI.angle / 2;
			}

			switch (item->Animation.ActiveState)
			{
			case SNIPER_STATE_IDLE:
				item->MeshBits = 0;

				if (TargetVisible(item, &AI))
					item->Animation.TargetState = SNIPER_STATE_UNCOVER;

				break;

			case SNIPER_STATE_UNCOVER:
				item->MeshBits = ALL_JOINT_BITS;
				break;

			case 3:
				creature->Flags = 0;
				if (!TargetVisible(item, &AI) ||
					item->HitStatus &&
					Random::TestProbability(1 / 2.0f))
				{
					item->Animation.TargetState = SNIPER_STATE_COVER;
				}
				else if (Random::TestProbability(1 / 30.0f))
					item->Animation.TargetState = SNIPER_STATE_FIRE;
			
				break;

			case SNIPER_STATE_FIRE:
				if (!creature->Flags)
				{
					ShotLara(item, &AI, SniperGunBite, joint0, 100);
					creature->MuzzleFlash[0].Bite = SniperGunBite;
					creature->MuzzleFlash[0].Delay = 2;
					creature->Flags = 1;
				}

				break;
			}
		}
		else
		{
			item->HitPoints = 0;

			if (item->Animation.ActiveState != SNIPER_STATE_DEATH)
			{
				item->Animation.AnimNumber = 5;
				item->Animation.FrameNumber = 0;
				item->Animation.ActiveState = SNIPER_STATE_DEATH;
			}
		}

		CreatureTilt(item, 0);
		CreatureJoint(item, 0, joint0);
		CreatureJoint(item, 1, joint1);
		CreatureJoint(item, 2, joint2);
		CreatureAnimation(itemNumber, angle, 0);
	}

	void InitializeMafia2(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(*item, 0);
		item->SetMeshSwapFlags(9216);
	}

	void Mafia2Control(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short joint0 = 0;
		short joint1 = 0;
		short joint2 = 0;

		// Can mafia jump? Check for a distances of 1 and 2 sectors.
		int x = item->Pose.Position.x;
		int y = item->Pose.Position.y;
		int z = item->Pose.Position.z;

		int dx = 870 * phd_sin(item->Pose.Orientation.y);
		int dz = 870 * phd_cos(item->Pose.Orientation.y);

		x += dx;
		z += dz;
		int height1 = GetPointCollision(Vector3i(x, y, z), item->RoomNumber).GetFloorHeight();

		x += dx;
		z += dz;
		int height2 = GetPointCollision(Vector3i(x, y, z), item->RoomNumber).GetFloorHeight();
	
		x += dx;
		z += dz;
		int height3 = GetPointCollision(Vector3i(x, y, z), item->RoomNumber).GetFloorHeight();
	
		int height = 0;
		bool canJump1Sector = true;
		if (item->BoxNumber == LaraItem->BoxNumber ||
			y >= (height1 - CLICK(1.5f)) ||
			y >= (height2 + CLICK(1)) ||
			y <= (height2 - CLICK(1)))
		{
			height = height2;
			canJump1Sector = false;
		}

		bool canJump2Sectors = true;
		if (item->BoxNumber == LaraItem->BoxNumber ||
			y >= (height1 - CLICK(1.5f)) ||
			y >= (height - CLICK(1.5f)) ||
			y >= (height3 + CLICK(1)) ||
			y <= (height3 - CLICK(1)))
		{
			canJump2Sectors = false;
		}

		if (creature->MuzzleFlash[0].Delay != 0)
			creature->MuzzleFlash[0].Delay--;

		if (creature->MuzzleFlash[1].Delay != 0)
			creature->MuzzleFlash[1].Delay--;

		if (item->AIBits)
			GetAITarget(creature);
		else
			creature->Enemy = LaraItem;

		AI_INFO ai;
		CreatureAIInfo(item, &ai);

		if (item->HitPoints > 0)
		{
			AI_INFO laraAI;
			if (creature->Enemy == LaraItem)
			{
				laraAI.angle = ai.angle;
				laraAI.distance = ai.distance;
			}
			else
			{
				dx = LaraItem->Pose.Position.x - item->Pose.Position.x;
				dz = LaraItem->Pose.Position.z - item->Pose.Position.z;
				laraAI.angle = phd_atan(dz, dx) - item->Pose.Orientation.y;
				laraAI.distance = pow(dx, 2) + pow(dz, 2);
			}

			GetCreatureMood(item, &ai, creature->Enemy != LaraItem);
			CreatureMood(item, &ai, creature->Enemy != LaraItem);
			creature->Enemy = LaraItem;
			angle = CreatureTurn(item, creature->MaxTurn);

			if ((laraAI.distance < pow(BLOCK(2), 2) && LaraItem->Animation.Velocity.z > 20) ||
				item->HitStatus ||
				TargetVisible(item, &laraAI))
			{
				if (!(item->AIBits & FOLLOW))
				{
					creature->Enemy = LaraItem;
					AlertAllGuards(itemNumber);
				}
			}
			switch (item->Animation.ActiveState)
			{
			case MAFIA2_STATE_IDLE:
				creature->MaxTurn = 0;
				creature->Flags = 0;
				creature->LOT.IsJumping = false;
				joint2 = laraAI.angle;

				if (ai.ahead && !(item->AIBits & GUARD))
				{
					joint0 = ai.angle / 2;
					joint1 = ai.xAngle;
				}
				if (item->AIBits & GUARD)
				{
					joint2 = AIGuard(creature);
					break;
				}
				if (laraAI.angle <= ANGLE(112.5f) && laraAI.angle >= -ANGLE(112.5f))
				{
					if (item->TestMeshSwapFlags(9216))
					{
						item->Animation.TargetState = MAFIA2_STATE_UNDRAW_GUNS;
						break;
					}
				}
				else if (item->TestMeshSwapFlags(9216))
				{
					item->Animation.TargetState = MAFIA2_STATE_TURN_180;
					break;
				}
				if (Targetable(item, &ai))
				{
					if (ai.distance < pow(BLOCK(1), 2) || ai.zoneNumber != ai.enemyZone)
						item->Animation.TargetState = MAFIA2_STATE_AIM;
					else if (!(item->AIBits & MODIFY))
						item->Animation.TargetState = MAFIA2_STATE_WALK;
				}
				else
				{
					if (item->AIBits & PATROL1)
						item->Animation.TargetState = MAFIA2_STATE_WALK;
					else
					{
						if (canJump1Sector || canJump2Sectors)
						{
							item->Animation.AnimNumber = 41;
							item->Animation.FrameNumber = 0;
							item->Animation.ActiveState = MAFIA2_STATE_IDLE_START_JUMP;
							creature->MaxTurn = 0;

							if (canJump2Sectors)
								item->Animation.TargetState = MAFIA2_STATE_JUMP_2_BLOCKS;
							else
								item->Animation.TargetState = MAFIA2_STATE_JUMP_1_BLOCK;

							creature->LOT.IsJumping = true;
							break;
						}

						if (creature->Mood != MoodType::Bored)
						{
							if (ai.distance >= pow(BLOCK(3), 2))
								item->Animation.TargetState = MAFIA2_STATE_WALK;
						}
						else
							item->Animation.TargetState = MAFIA2_STATE_IDLE;
					}
				}

				break;

			case MAFIA2_STATE_TURN_180_UNDRAW_WEAPON:
			case MAFIA2_STATE_TURN_180:
				creature->MaxTurn = 0;

				if (ai.angle >= 0)
					item->Pose.Orientation.y -= ANGLE(2.0f);
				else
					item->Pose.Orientation.y += ANGLE(2.0f);

				if (item->Animation.FrameNumber != 16 ||
					!item->TestMeshSwapFlags(9216))
				{
					if (TestLastFrame(*item))
						item->Pose.Orientation.y += -ANGLE(180.0f);
				}
				else
				{
					item->SetMeshSwapFlags(128);
				}
			
				break;

			case MAFIA2_STATE_FIRE:
				creature->MaxTurn = 0;
				joint0 = laraAI.angle / 2;
				joint2 = laraAI.angle / 2;

				if (ai.ahead)
					joint1 = ai.xAngle;

				if (abs(ai.angle) >= ANGLE(2.0f))
				{
					if (ai.angle >= 0)
						item->Pose.Orientation.y += ANGLE(2.0f);
					else
						item->Pose.Orientation.y -= ANGLE(2.0f);
				}
				else
				{
					item->Pose.Orientation.y += ai.angle;
				}
			
				if (!(creature->Flags & 1) && item->Animation.FrameNumber == 2)
				{
					ShotLara(item, &ai, ArmedMafia2GunRightBite, laraAI.angle / 2, 25);
					creature->MuzzleFlash[1].Bite = ArmedMafia2GunRightBite;
					creature->MuzzleFlash[1].Delay = 2;
					creature->Flags |= 1;
				}

				if (!(creature->Flags & 2) && item->Animation.FrameNumber == 6)
				{
					ShotLara(item, &ai, ArmedMafia2GunLeftBite, laraAI.angle / 2, 25);
					creature->MuzzleFlash[0].Bite = ArmedMafia2GunLeftBite;
					creature->MuzzleFlash[0].Delay = 2;
					creature->Flags |= 2;
				}
			
				break;

			case MAFIA2_STATE_AIM:
				creature->MaxTurn = 0;
				creature->Flags = 0;
				joint0 = laraAI.angle / 2;
				joint2 = laraAI.angle / 2;

				if (ai.ahead)
					joint1 = ai.xAngle;

				if (abs(ai.angle) >= ANGLE(2.0f))
				{
					if (ai.angle >= 0)
						item->Pose.Orientation.y += ANGLE(2.0f);
					else
						item->Pose.Orientation.y -= ANGLE(2.0f);
				}
				else
					item->Pose.Orientation.y += ai.angle;

				if (Targetable(item, &ai))
					item->Animation.TargetState = MAFIA2_STATE_FIRE;
				else if (laraAI.angle > 20480 || laraAI.angle < -20480)
					item->Animation.TargetState = 32;
				else
					item->Animation.TargetState = MAFIA2_STATE_IDLE;
			
				break;

			case MAFIA2_STATE_WALK:
				creature->MaxTurn = ANGLE(5.0f);
				creature->LOT.IsJumping = false;

				if (Targetable(item, &ai) &&
					(ai.distance < pow(BLOCK(1), 2) || ai.zoneNumber != ai.enemyZone))
				{
					item->Animation.TargetState = MAFIA2_STATE_AIM;
				}
				else
				{
					if (canJump1Sector || canJump2Sectors)
					{
						item->Animation.AnimNumber = 41;
						item->Animation.FrameNumber = 0;
						item->Animation.ActiveState = MAFIA2_STATE_IDLE_START_JUMP;
						creature->MaxTurn = 0;

						if (canJump2Sectors)
							item->Animation.TargetState = MAFIA2_STATE_JUMP_2_BLOCKS;
						else
							item->Animation.TargetState = MAFIA2_STATE_JUMP_1_BLOCK;

						creature->LOT.IsJumping = true;
						break;
					}

					if (ai.distance >= pow(BLOCK(1), 2))
					{
						if (ai.distance > pow(BLOCK(3), 2))
							item->Animation.TargetState = MAFIA2_STATE_RUN;
					}
					else
						item->Animation.TargetState = MAFIA2_STATE_IDLE;
				}

				break;

			case MAFIA2_STATE_RUN:
				creature->MaxTurn = ANGLE(10.0f);
				creature->LOT.IsJumping = false;

				if (Targetable(item, &ai) &&
					(ai.distance < pow(BLOCK(1), 2) || ai.zoneNumber != ai.enemyZone))
				{
					item->Animation.TargetState = MAFIA2_STATE_AIM;
				}
				else if (canJump1Sector || canJump2Sectors)
				{
					item->Animation.AnimNumber = 50;
					item->Animation.FrameNumber = 0;
					item->Animation.ActiveState = MAFIA2_STATE_IDLE_START_JUMP;
					creature->MaxTurn = 0;

					if (canJump2Sectors)
						item->Animation.TargetState = MAFIA2_STATE_JUMP_2_BLOCKS;
					else
						item->Animation.TargetState = MAFIA2_STATE_JUMP_1_BLOCK;

					creature->LOT.IsJumping = true;
				}
				else if (ai.distance < pow(BLOCK(3), 2))
					item->Animation.TargetState = MAFIA2_STATE_WALK;
			
				break;

			case MAFIA2_STATE_UNDRAW_GUNS:
				creature->MaxTurn = 0;

				if (ai.angle >= 0)
					item->Pose.Orientation.y += ANGLE(2.0f);
				else
					item->Pose.Orientation.y -= ANGLE(2.0f);

				if (item->Animation.FrameNumber == 16 &&
					item->TestMeshSwapFlags(9216))
				{
					item->SetMeshSwapFlags(128);
				}

				break;

			default:
				break;
			}
		}
		else
		{
			if (item->Animation.ActiveState != MAFIA2_STATE_DEATH_1 &&
				item->Animation.ActiveState != MAFIA2_STATE_DEATH_2)
			{
				if (ai.angle >= ANGLE(67.5f) || ai.angle <= -ANGLE(67.5f))
				{
					item->Animation.AnimNumber = 16;
					item->Animation.ActiveState = MAFIA2_STATE_DEATH_2;
					item->Pose.Orientation.y += ai.angle - ANGLE(18.0f);
				}
				else
				{
					item->Animation.AnimNumber = 11;
					item->Animation.ActiveState = MAFIA2_STATE_DEATH_1;
					item->Pose.Orientation.y += ai.angle;
				}

				item->Animation.FrameNumber = 0;
			}
		}

		CreatureJoint(item, 0, joint0);
		CreatureJoint(item, 1, joint1);
		CreatureJoint(item, 2, joint2);

		if (item->Animation.ActiveState >= 20 ||
			item->Animation.ActiveState == MAFIA2_STATE_DEATH_1 ||
			item->Animation.ActiveState == MAFIA2_STATE_DEATH_2)
		{
			CreatureAnimation(itemNumber, angle, 0);
		}
		else
		{
			switch (CreatureVault(itemNumber, angle, 2, CLICK(2)) + 4)
			{
			case 0:
				item->Animation.AnimNumber = 38;
				item->Animation.FrameNumber = 0;
				item->Animation.ActiveState = 23;
				break;

			case 1:
				item->Animation.AnimNumber = 39;
				item->Animation.FrameNumber = 0;
				item->Animation.ActiveState = 24;
				creature->MaxTurn = 0;
				break;

			case 2:
				item->Animation.AnimNumber = 40;
				item->Animation.FrameNumber = 0;
				item->Animation.ActiveState = 25;
				creature->MaxTurn = 0;
				break;

			case 6:
				item->Animation.AnimNumber = 35;
				item->Animation.FrameNumber = 0;
				item->Animation.ActiveState = 20;
				creature->MaxTurn = 0;
				break;

			case 7:
				item->Animation.AnimNumber = 36;
				item->Animation.FrameNumber = 0;
				item->Animation.ActiveState = 21;
				creature->MaxTurn = 0;
				break;

			case 8:
				item->Animation.AnimNumber = 37;
				item->Animation.FrameNumber = 0;
				item->Animation.ActiveState = 22;
				creature->MaxTurn = 0;
				break;
			}
		}
	}

	void GuardHit(ItemInfo& target, ItemInfo& source, std::optional<GameVector> pos, int damage, bool isExplosive, int jointIndex)
	{
		if (pos.has_value())
		{
			if (jointIndex == GUARD_HEAD_MESH)
			{
				DoBloodSplat(pos->x, pos->y, pos->z, 10, source.Pose.Orientation.y, pos->RoomNumber);
				DoDamage(&target, INT_MAX);

				return;
			}
			else
				DoBloodSplat(pos->x, pos->y, pos->z, 10, source.Pose.Orientation.y, pos->RoomNumber);
		}

		DoItemHit(&target, damage, isExplosive);
	}
}
