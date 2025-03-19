#include "framework.h"
#include "Objects/TR4/Entity/tr4_baddy.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/control/lot.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_fire.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Math;

/*
ID_BADDY1
1 - Rolls to the right 1 pow
2 - Jumps to the left 1 pow
3 - ducks when triggered
4 - Climbs up 4 clicks when triggered
101-104 – Slides to the left while crouching when triggered (eg. train level – just doesn’t work in trainmode)
1004 - Climbs up 6 clicks when triggered
1000 – N x 1000 – Is activated once the baddy with the previous thousand is dead and needs no trigger (have tested up to 20.000). Must be placed in room 2 of a level.
This means that:
2000 - Attacks Lara after she kills 1st baddy triggered
3000 - Same as above but after she kills 2nd baddy triggered
4000 - Same as above but after she kills 3rd baddy triggered
6000 - Same as above but after she kills 5th baddy triggered
etc.

ID_BADDY2
1 - Jumps to right when triggered
2 - Rolls to left when triggered
3 - Crouches when triggered
4 - Climbs up 4 clicks when triggered
10 - Draws uzi when triggered
11 - Jumps to the right when triggered and draws uzi
12 - Rolls to the left when triggered and draws uzi
13 - Crouches when triggered and draws uzi
14 - Climbs up 4 clicks when triggered and draws uzi
101 - Slides to the left while crouching when triggered (eg. Train level)
101-104 - Slides to the left while crouching when triggered. The setup requires an enemy jeep and an AI_X1 nullmesh with the same OCB as the jeep and the baddy. It works only in trainmode. When triggered, the baddy will ride the roof of the enemy jeep parallel to the railtracks, until they reach the AI_X1 nullmesh. The baddy will jump off in the direction he’s placed in the map, while the jeep will fall back.
*/

namespace TEN::Entities::TR4
{
	constexpr auto BADDY_UZI_AMMO = 24;

	const auto BaddyGunBite	  = CreatureBiteInfo(Vector3(-5, 200, 50), 11);
	const auto BaddySwordBite = CreatureBiteInfo(Vector3::Zero, 15);
	const auto BaddySwordAttackJoints = std::vector<unsigned int>{ 14, 15, 16 };

	enum BaddyState
	{
		BADDY_STATE_IDLE = 0,
		BADDY_STATE_WALK = 1,
		BADDY_STATE_RUN = 2,
		// 3
		BADDY_STATE_DODGE_START = 4,
		// 5
		// 6
		// 7
		BADDY_STATE_DODGE = 8,
		BADDY_STATE_DODGE_END = 9,
		BADDY_STATE_DRAW_GUN = 10,
		BADDY_STATE_HOLSTER_GUN = 11,
		BADDY_STATE_DRAW_SWORD = 12,
		BADDY_STATE_HOLSTER_SWORD = 13,
		BADDY_STATE_FIRE = 14,
		BADDY_STATE_SWORD_HIT_FRONT = 15,
		BADDY_STATE_SWORD_HIT_RIGHT = 16,
		BADDY_STATE_SWORD_HIT_LEFT = 17,
		BADDY_STATE_MONKEY_GRAB = 18,
		BADDY_STATE_MONKEY_IDLE = 19,
		BADDY_STATE_MONKEY_FORWARD = 20,
		BADDY_STATE_MONKEY_PUSH_OFF = 21,
		BADDY_STATE_MONKEY_FALL_LAND = 22,
		BADDY_STATE_ROLL_LEFT = 23,
		BADDY_STATE_JUMP_RIGHT = 24,
		BADDY_STATE_STAND_TO_CROUCH = 25,
		BADDY_STATE_CROUCH = 26,
		BADDY_STATE_CROUCH_PICKUP = 27,
		BADDY_STATE_CROUCH_TO_STAND = 28,
		BADDY_STATE_WALK_SWORD_HIT_RIGHT = 29,
		BADDY_STATE_SOMERSAULT = 30,
		BADDY_STATE_AIM = 31,
		BADDY_STATE_DEATH = 32,
		BADDY_STATE_JUMP_FORWARD_1_BLOCK = 33,
		BADDY_STATE_JUMP_FORWARD_FALL = 34,
		BADDY_STATE_MONKEY_TO_FREEFALL = 35,
		BADDY_STATE_FREEFALL = 36,
		BADDY_STATE_FREEFALL_LAND_DEATH = 37,
		BADDY_STATE_JUMP_FORWARD_2_BLOCKS = 38,
		BADDY_STATE_CLIMB_4_STEPS = 39,
		BADDY_STATE_CLIMB_3_STEPS = 40,
		BADDY_STATE_CLIMB_2_STEPS = 41,
		BADDY_STATE_JUMP_OFF_4_STEPS = 42,
		BADDY_STATE_JUMP_OFF_3_STEPS = 43,
		BADDY_STATE_BLIND = 44
	};

	enum BaddyAnim
	{
		BADDY_ANIM_RUN = 0,
		BADDY_ANIM_RUN_STOP_START = 1,
		BADDY_ANIM_RUN_STOP_END = 2,
		BADDY_ANIM_SOMERSAULT_START = 3,
		BADDY_ANIM_SOMERSAULT_END = 4,
		BADDY_ANIM_DODGE_START = 5,
		// 6
		// 7
		// 8
		BADDY_ANIM_MONKEY_GRAB = 9,
		BADDY_ANIM_MONKEY_IDLE = 10,
		BADDY_ANIM_MONKEY_FORWARD = 11,
		BADDY_ANIM_MONKEY_IDLE_TO_FORWARD = 12,
		BADDY_ANIM_MONKEY_STOP_LEFT = 13,
		BADDY_ANIM_MONKEY_STOP_RIGHT = 14,
		BADDY_ANIM_MONKEY_FALL_LAND = 15,
		BADDY_ANIM_MONKEY_PUSH_OFF = 16,
		BADDY_ANIM_DODGE_END = 17,
		BADDY_ANIM_STAND_IDLE = 18,
		BADDY_ANIM_DODGE_END_TO_STAND = 19,
		BADDY_ANIM_DRAW_GUN = 20,
		BADDY_ANIM_HOLSTER_GUN = 21,
		BADDY_ANIM_DRAW_SWORD = 22,
		BADDY_ANIM_HOLSTER_SWORD = 23,
		BADDY_ANIM_STAND_TO_ROLL_LEFT = 24,
		BADDY_ANIM_ROLL_LEFT_START = 25,
		BADDY_ANIM_ROLL_LEFT_CONTINUE = 26,
		BADDY_ANIM_ROLL_LEFT_END = 27,
		BADDY_ANIM_ROLL_LEFT_TO_CROUCH = 28,
		BADDY_ANIM_CROUCH = 29,
		BADDY_ANIM_CROUCH_TO_STAND = 30,
		BADDY_ANIM_STAND_TO_WALK = 31,
		BADDY_ANIM_WALK = 32,
		BADDY_ANIM_WALK_TO_RUN = 33,
		BADDY_ANIM_STAND_TO_AIM = 34,
		BADDY_ANIM_AIM = 35,
		BADDY_ANIM_FIRE = 36,
		BADDY_ANIM_AIM_TO_STAND = 37,
		BADDY_ANIM_SWORD_HIT_FRONT = 38,
		BADDY_ANIM_CROUCH_PICKUP = 39,
		BADDY_ANIM_STAND_TO_CROUCH = 40,
		BADDY_ANIM_SWORD_HIT_RIGHT = 41,
		BADDY_ANIM_SWORD_HIT_RIGHT_TO_LEFT = 42,
		BADDY_ANIM_SWORD_HIT_RIGHT_TO_STAND = 43,
		BADDY_ANIM_SWORD_HIT_LEFT = 44,
		BADDY_ANIM_STAND_DEATH = 45,
		BADDY_ANIM_WALK_SWORD_HIT_RIGHT = 46,
		BADDY_ANIM_STAND_TO_JUMP_RIGHT = 47,
		BADDY_ANIM_JUMP_RIGHT_START = 48,
		BADDY_ANIM_JUMP_RIGHT_CONTINUE = 49,
		BADDY_ANIM_JUMP_RIGHT_END = 50,
		BADDY_ANIM_RUN_TO_WALK = 51,
		// 52
		// 53
		BADDY_ANIM_WALK_STOP_RIGHT = 54,
		BADDY_ANIM_STAND_TO_JUMP_FORWARD = 55,
		BADDY_ANIM_JUMP_FORWARD_1_BLOCK = 56,
		BADDY_ANIM_JUMP_FORWARD_FALL = 57,
		BADDY_ANIM_JUMP_FORWARD_LAND = 58,
		BADDY_ANIM_MONKEY_TO_FREEFALL = 59,
		BADDY_ANIM_FREEFALL = 60,
		BADDY_ANIM_FREEFALL_LAND_DEATH = 61,
		BADDY_ANIM_CLIMB_4_STEPS = 62,
		BADDY_ANIM_CLIMB_3_STEPS = 63,
		BADDY_ANIM_CLIMB_2_STEPS = 64,
		BADDY_ANIM_JUMP_OFF_4_STEPS = 65,
		BADDY_ANIM_JUMP_OFF_3_STEPS = 66,
		BADDY_ANIM_JUMP_FORWARD_2_BLOCKS = 67,
		BADDY_ANIM_BLIND = 68,
		BADDY_ANIM_BLIND_TO_STAND = 69,
		BADDY_ANIM_DEAD = 70,
	};

	enum BaddyFrame
	{
		FRAME_BADDY_HOLSTER_GUN = 20,
		FRAME_BADDY_DRAW_GUN = 21,
		FRAME_BADDY_HOLSTER_SWORD = 22,
		FRAME_BADDY_DRAW_SWORD = 12,
		FRAME_BADDY_RUN_TO_SOMERSAULT = 11,
		FRAME_BADDY_SWORD_HIT_NO_DAMAGE_MAX = 12,
		FRAME_BADDY_SWORD_HIT_DAMAGE_MIN = 13,
		FRAME_BADDY_SWORD_HIT_DAMAGE_MAX = 21,
		FRAME_BADDY_CROUCH_PICKUP = 9,
		FRAME_BADDY_FIRE_MIN = 1,
		FRAME_BADDY_FIRE_MAX = 13,
		FRAME_BADDY_SOMERSAULT_START_TAKE_OFF = 18,
	};

	enum BaddyMeshSwapFlags
	{
		MESHSWAPFLAGS_BADDY_EMPTY = 0x7FC800,
		MESHSWAPFLAGS_BADDY_SWORD_SIMPLE = 0x7E0880,
		MESHSWAPFLAGS_BADDY_SWORD_NINJA = 0x000880,
		MESHSWAPFLAGS_BADDY_GUN = 0x7FC010,
	};

	void InitializeBaddy(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);

		short objectNumber = (Objects[ID_BADDY2].loaded ? ID_BADDY2 : ID_BADDY1);

		if (item->ObjectNumber == ID_BADDY1)
		{
			item->SetMeshSwapFlags(MESHSWAPFLAGS_BADDY_GUN);
			item->MeshBits = 0xFF81FFFF;
			item->ItemFlags[2] = BADDY_UZI_AMMO;
		}
		else
		{
			item->SetMeshSwapFlags(MESHSWAPFLAGS_BADDY_SWORD_NINJA);
			item->MeshBits = ALL_JOINT_BITS;
			item->ItemFlags[2] = 0;
		}
	
		item->ItemFlags[1] = -1;

		short ocb = item->TriggerFlags % 1000;

		// To the same things of OCB 1, 2, 3, 4 but also drawing uzis
		if (ocb > 9 && ocb < 20)
		{
			item->ItemFlags[2] += BADDY_UZI_AMMO;
			item->TriggerFlags -= 10;
			ocb -= 10;
		}
	
		if (!ocb || ocb > 4 && ocb < 7)
		{
			item->Animation.AnimNumber = BADDY_ANIM_STAND_IDLE;
			item->Animation.FrameNumber = 0;
			item->Animation.TargetState = BADDY_STATE_IDLE;
			item->Animation.ActiveState = BADDY_STATE_IDLE;
			return;
		}

		// OCB: jump right
		if (ocb == 1)
		{
			item->Animation.AnimNumber = BADDY_ANIM_STAND_TO_JUMP_RIGHT;
			item->Animation.FrameNumber = 0;
			item->Animation.TargetState = BADDY_STATE_JUMP_RIGHT;
			item->Animation.ActiveState = BADDY_STATE_JUMP_RIGHT;
			return;
		}

		// OCB: jump left
		if (ocb == 2)
		{
			item->Animation.AnimNumber = BADDY_ANIM_STAND_TO_ROLL_LEFT;
			item->Animation.FrameNumber = 0;
			item->Animation.TargetState = BADDY_STATE_ROLL_LEFT;
			item->Animation.ActiveState = BADDY_STATE_ROLL_LEFT;
			return;
		}
	
		// OCB: crouch
		if (ocb == 3)
		{
			item->Animation.AnimNumber = BADDY_ANIM_CROUCH;
			item->Animation.FrameNumber = 0;
			item->Animation.TargetState = BADDY_STATE_CROUCH;
			item->Animation.ActiveState = BADDY_STATE_CROUCH;
			return;
		}

		// OCB: climb up 4 or 6 clicks 
		if (ocb == 4)
		{
			item->Animation.AnimNumber = BADDY_ANIM_CLIMB_4_STEPS;
			item->Animation.FrameNumber = 0;
			item->Animation.TargetState = BADDY_STATE_CLIMB_4_STEPS;
			item->Animation.ActiveState = BADDY_STATE_CLIMB_4_STEPS;
			item->Pose.Position.x += phd_sin(item->Pose.Orientation.y) * CLICK(4);
			item->Pose.Position.z += phd_cos(item->Pose.Orientation.y) * CLICK(4);
			return;
		}

		// OCB: crouch and jump in train levels?
		if (ocb > 100)
		{
			item->Animation.AnimNumber = BADDY_ANIM_CROUCH;
			item->Animation.FrameNumber = 0;
			item->Animation.TargetState = BADDY_STATE_CROUCH;
			item->Animation.ActiveState = BADDY_STATE_CROUCH;
			item->Pose.Position.x += phd_sin(item->Pose.Orientation.y) * CLICK(4);
			item->Pose.Position.z += phd_cos(item->Pose.Orientation.y) * CLICK(4);
			item->ItemFlags[3] = ocb;
			return;
		}
	
		item->Animation.FrameNumber = 0;
	}

	void BaddyControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		// Don't focus on disabled items
		if (creature->Enemy && (creature->Enemy->Flags & IFLAG_KILLED))
			creature->Enemy = nullptr;

		auto* enemyItem = creature->Enemy;

		short angle = 0;
		short tilt = 0;
		short joint1 = 0;
		short joint2 = 0;
		short joint3 = 0;

		// TODO: better add a second control routine for baddy 2 instead of mixing them?
		short objectNumber = (Objects[ID_BADDY2].loaded ? ID_BADDY2 : ID_BADDY1);

		if (creature->MuzzleFlash[0].Delay != 0)
			creature->MuzzleFlash[0].Delay--;

		bool roll = false;
		bool jump = false;

		if (item->TriggerFlags % 1000)
		{
			creature->MaxTurn = 0;
			creature->LOT.IsJumping = true;

			if (item->TriggerFlags % 1000 > 100)
			{
				item->ItemFlags[0] = -80;
				FindAITargetObject(creature, ID_AI_X1);
			}

			item->TriggerFlags = 1000 * (item->TriggerFlags / 1000);
		}

		// Can baddy jump? Check for a distance of 1 and 2 sectors
		int x = item->Pose.Position.x;
		int y = item->Pose.Position.y;
		int z = item->Pose.Position.z;

		int dx = 942 * phd_sin(item->Pose.Orientation.y);
		int dz = 942 * phd_cos(item->Pose.Orientation.y);

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
		if (enemyItem && item->BoxNumber == enemyItem->BoxNumber ||
			y >= (height1 - CLICK(1.5f)) ||
			y >= (height2 + CLICK(1)) ||
			y <= (height2 - CLICK(1)))
		{
			height = height2;
			canJump1Sector = false;
		}

		bool canJump2Sectors = true;
		if (enemyItem && item->BoxNumber == enemyItem->BoxNumber ||
			y >= (height1 - CLICK(1.5f)) ||
			y >= (height - CLICK(1.5f)) ||
			y >= (height3 + CLICK(1)) ||
			y <= (height3 - CLICK(1)))
		{
			canJump2Sectors = false;
		}

		auto* currentCreature = creature;

		if (item->ItemFlags[1] == item->RoomNumber ||
			g_Level.Rooms[item->RoomNumber].itemNumber == NO_VALUE)
		{
			currentCreature = creature;
		}
		else
		{
			currentCreature = creature;
			creature->Enemy = LaraItem;
			ItemInfo* currentItem = nullptr;
			for (short itemNum = g_Level.Rooms[item->RoomNumber].itemNumber; itemNum != NO_VALUE; itemNum = currentItem->NextItem)
			{
				currentItem = &g_Level.Items[itemNum];
				if ((currentItem->ObjectNumber == ID_SMALLMEDI_ITEM ||
					 currentItem->ObjectNumber == ID_BIGMEDI_ITEM ||
					 currentItem->ObjectNumber == ID_UZI_AMMO_ITEM) &&
					SameZone(creature, currentItem))
				{
					if (currentItem->Status != ITEM_INVISIBLE)
					{
						creature->Enemy = currentItem;
						break;
					}
				}
			}
		}

		item->ItemFlags[1] = item->RoomNumber;

		auto probe = GetPointCollision(*item);

		if (item->HitPoints <= 0)
		{
			item->Floor = GetPointCollision(*item).GetFloorHeight();
			currentCreature->LOT.IsMonkeying = false;

			switch (item->Animation.ActiveState)
			{
			case BADDY_STATE_DEATH:
				item->Animation.IsAirborne = true;
				currentCreature->LOT.IsMonkeying = false;

				if (item->Pose.Position.y >= item->Floor)
				{
					item->Pose.Position.y = item->Floor;
					item->Animation.Velocity.y = 0;
					item->Animation.IsAirborne = false;
				}

				break;

			case BADDY_STATE_MONKEY_TO_FREEFALL:
				item->Animation.TargetState = BADDY_STATE_FREEFALL;
				item->Animation.IsAirborne = false;
				break;

			case BADDY_STATE_FREEFALL:
				item->Animation.IsAirborne = true;

				if (item->Pose.Position.y >= item->Floor)
				{
					item->Pose.Position.y = item->Floor;
					item->Animation.Velocity.y = 0;
					item->Animation.IsAirborne = false;
					item->Animation.TargetState = BADDY_STATE_FREEFALL_LAND_DEATH;
				}

				break;

			case BADDY_STATE_FREEFALL_LAND_DEATH:
				item->Pose.Position.y = item->Floor;
				break;

			case BADDY_STATE_MONKEY_GRAB:
			case BADDY_STATE_MONKEY_IDLE:
			case BADDY_STATE_MONKEY_FORWARD:
				item->Animation.AnimNumber = BADDY_ANIM_MONKEY_TO_FREEFALL;
				item->Animation.FrameNumber = 0;
				item->Animation.ActiveState = BADDY_STATE_MONKEY_TO_FREEFALL;
				item->Animation.Velocity.z = 0;
				break;

			default:
				item->Animation.AnimNumber = BADDY_ANIM_STAND_DEATH;
				item->Animation.FrameNumber = 0;
				item->Animation.ActiveState = BADDY_STATE_DEATH;
				currentCreature->LOT.IsJumping = true;

				// OCB: respawn code for BADDY_1
				if (item->TriggerFlags > 999)
				{
					for (int i = 0; i < g_Level.NumItems; i++)
					{
						auto* possibleEnemy = &g_Level.Items[i];

						if (possibleEnemy->ObjectNumber == ID_BADDY1 || possibleEnemy->ObjectNumber == ID_BADDY2 &&
							(item->TriggerFlags / 1000) == (possibleEnemy->TriggerFlags / 1000) - 1 &&
							!(possibleEnemy->Flags & IFLAG_KILLED))
						{
							if (EnableEntityAI(i, false))
								possibleEnemy->Status = ITEM_ACTIVE;
							else
								possibleEnemy->Status = ITEM_INVISIBLE;
							
							AddActiveItem(i);
							break;
						}
					}
				}

				break;
			}
		}
		else
		{
			if (item->AIBits)
				GetAITarget(currentCreature);
			else if (!currentCreature->Enemy)
				currentCreature->Enemy = LaraItem;

			AI_INFO AI;
			CreatureAIInfo(item, &AI);

			AI_INFO laraAI;
			if (currentCreature->Enemy->IsLara())
			{
				laraAI.angle = AI.angle;
				laraAI.ahead = AI.ahead;
				laraAI.distance = AI.distance;
			}
			else
			{
				dx = LaraItem->Pose.Position.x - item->Pose.Position.x;
				dz = LaraItem->Pose.Position.z - item->Pose.Position.z;
				laraAI.angle = phd_atan(dz, dx) - item->Pose.Orientation.y;
				laraAI.ahead = true;

				if (laraAI.angle <= -ANGLE(90.0f) || laraAI.angle >= ANGLE(90.0f))
					laraAI.ahead = false;

				laraAI.distance = dx * dx + dz * dz;
			}

			GetCreatureMood(item, &AI, true);

			// Vehicle handling
			if (Lara.Context.Vehicle != NO_VALUE && AI.bite)
				currentCreature->Mood = MoodType::Escape;

			CreatureMood(item, &AI, true);

			angle = CreatureTurn(item, currentCreature->MaxTurn);

			//currentCreature->enemy = LaraItem;

			//ItemInfo* oldEnemy = creature->enemy;
			//creature->enemy = LaraItem;

			// Is baddy alerted?
			if (item->HitStatus ||
				laraAI.distance < pow(BLOCK(1), 2) ||
				TargetVisible(item, &laraAI) &&
				abs(LaraItem->Pose.Position.y - item->Pose.Position.y) < CLICK(4))
			{
				currentCreature->Alerted = true;
			}

			if (item != Lara.TargetEntity ||
				laraAI.distance <= pow(942, 2) ||
				laraAI.angle <= -ANGLE(56.25f) ||
				laraAI.angle >= ANGLE(56.25f))
			{
				roll = false;
				jump = false;
			}
			else
			{
				dx = 942 * phd_sin(item->Pose.Orientation.y + ANGLE(45.0f));
				dz = 942 * phd_cos(item->Pose.Orientation.y + ANGLE(45.0f));

				x = item->Pose.Position.x + dx;
				y = item->Pose.Position.y;
				z = item->Pose.Position.z + dz;
				int height4 = GetPointCollision(Vector3i(x, y, z), item->RoomNumber).GetFloorHeight();

				dx = 942 * phd_sin(item->Pose.Orientation.y + ANGLE(78.75f));
				dz = 942 * phd_cos(item->Pose.Orientation.y + ANGLE(78.75f));

				x = item->Pose.Position.x + dx;
				y = item->Pose.Position.y;
				z = item->Pose.Position.z + dz;
				int height5 = GetPointCollision(Vector3i(x, y, z), item->RoomNumber).GetFloorHeight();

				if (abs(height5 - item->Pose.Position.y) > CLICK(1))
					jump = false;
				else
				{
					jump = true;
					if ((height4 + CLICK(2)) >= item->Pose.Position.y)
						jump = false;
				}

				dx = 942 * phd_sin(item->Pose.Orientation.y - ANGLE(45.0f));
				dz = 942 * phd_cos(item->Pose.Orientation.y - ANGLE(45.0f));

				x = item->Pose.Position.x + dx;
				y = item->Pose.Position.y;
				z = item->Pose.Position.z + dz;
				int height6 = GetPointCollision(Vector3i(x, y, z), item->RoomNumber).GetFloorHeight();

				dx = 942 * phd_sin(item->Pose.Orientation.y - ANGLE(78.75f));
				dz = 942 * phd_cos(item->Pose.Orientation.y - ANGLE(78.75f));

				x = item->Pose.Position.x + dx;
				y = item->Pose.Position.y;
				z = item->Pose.Position.z + dz;
				int height7 = GetPointCollision(Vector3i(x, y, z), item->RoomNumber).GetFloorHeight();

				if (abs(height7 - item->Pose.Position.y) > CLICK(1) ||
					(height6 + CLICK(2)) >= item->Pose.Position.y)
					roll = false;
				else
					roll = true;
			}

			switch (item->Animation.ActiveState)
			{
			case BADDY_STATE_IDLE:
				currentCreature->MaxTurn = 0;
				currentCreature->LOT.IsMonkeying = false;
				currentCreature->LOT.IsJumping = false;
				currentCreature->Flags = 0;
				joint3 = AI.angle / 2;

				if (AI.ahead && item->AIBits != GUARD)
				{
					joint1 = AI.angle / 2;
					joint2 = AI.xAngle;
				}

				if (item->AIBits & GUARD)
				{
					joint3 = AIGuard(currentCreature);
					item->Animation.TargetState = 0;
					break;
				}

				if (item->ObjectNumber == ID_BADDY2 &&
					item->TestMeshSwapFlags(MESHSWAPFLAGS_BADDY_SWORD_NINJA) &&
					item == Lara.TargetEntity &&
					laraAI.ahead &&
					laraAI.distance > pow(682, 2))
				{
					item->Animation.TargetState = BADDY_STATE_DODGE_START;
					break;
				}

				if (Targetable(item, &AI) && item->ItemFlags[2] > 0)
				{
					if (item->TestMeshSwapFlags(MESHSWAPFLAGS_BADDY_GUN))
					{
						item->Animation.TargetState = BADDY_STATE_AIM;
						break;
					}

					if (!item->TestMeshSwapFlags(MESHSWAPFLAGS_BADDY_SWORD_SIMPLE) && 
						!item->TestMeshSwapFlags(MESHSWAPFLAGS_BADDY_SWORD_NINJA))
					{
						item->Animation.TargetState = BADDY_STATE_DRAW_GUN;
						break;
					}

					item->Animation.TargetState = BADDY_STATE_HOLSTER_SWORD;
					break;
				}

				if (item->AIBits & MODIFY)
				{
					item->Animation.TargetState = BADDY_STATE_IDLE;

					if (item->Floor > item->Pose.Position.y + CLICK(3))
						item->AIBits &= ~MODIFY;

					break;
				}

				if (canJump1Sector || canJump2Sectors)
				{
					currentCreature->MaxTurn = 0;
					currentCreature->LOT.IsJumping = true;

					item->Animation.AnimNumber = BADDY_ANIM_STAND_TO_JUMP_FORWARD;
					item->Animation.FrameNumber = 0;
					item->Animation.ActiveState = BADDY_STATE_JUMP_FORWARD_1_BLOCK;

					if (!canJump2Sectors)
						item->Animation.TargetState = BADDY_STATE_JUMP_FORWARD_1_BLOCK;
					else
						item->Animation.TargetState = BADDY_STATE_JUMP_FORWARD_2_BLOCKS;

					break;
				}

				if (currentCreature->Enemy)
				{
					short objectNumber = currentCreature->Enemy->ObjectNumber;
					if ((objectNumber == ID_SMALLMEDI_ITEM || objectNumber == ID_UZI_AMMO_ITEM || objectNumber == ID_BIGMEDI_ITEM) &&
						AI.distance < pow(BLOCK(0.5f), 2))
					{
						item->Animation.TargetState = BADDY_STATE_STAND_TO_CROUCH;
						item->Animation.RequiredState = BADDY_STATE_CROUCH_PICKUP;
						break;
					}
				}

				if (item->TestMeshSwapFlags(MESHSWAPFLAGS_BADDY_GUN) && item->ItemFlags[2] < 1)
				{
					item->Animation.TargetState = BADDY_STATE_HOLSTER_GUN;
					break;
				}

				if (currentCreature->MonkeySwingAhead)
				{
					probe = GetPointCollision(*item);
					if (probe.GetCeilingHeight() == probe.GetFloorHeight() - CLICK(6))
					{
						if (item->TestMeshSwapFlags(MESHSWAPFLAGS_BADDY_EMPTY))
						{
							item->Animation.TargetState = BADDY_STATE_MONKEY_GRAB;
							break;
						}

						if (item->TestMeshSwapFlags(MESHSWAPFLAGS_BADDY_GUN))
						{
							item->Animation.TargetState = BADDY_STATE_HOLSTER_GUN;
							break;
						}

						item->Animation.TargetState = BADDY_STATE_HOLSTER_SWORD;
						break;
					}
				}
				else
				{
					if (roll)
					{
						currentCreature->MaxTurn = 0;
						item->Animation.TargetState = BADDY_STATE_ROLL_LEFT;
						break;
					}

					if (jump)
					{
						currentCreature->MaxTurn = 0;
						item->Animation.TargetState = BADDY_STATE_JUMP_RIGHT;
						break;
					}

					if (item->TestMeshSwapFlags(MESHSWAPFLAGS_BADDY_EMPTY))
					{
						item->Animation.TargetState = BADDY_STATE_DRAW_SWORD;
						break;
					}

					if (currentCreature->Enemy && 
						currentCreature->Enemy->HitPoints > 0 && 
						AI.distance < pow(BLOCK(0.5f), 2) &&
						abs(AI.verticalDistance) < BLOCK(1))
					{
						if (item->TestMeshSwapFlags(MESHSWAPFLAGS_BADDY_GUN))
							item->Animation.TargetState = BADDY_STATE_HOLSTER_GUN;
						else if (AI.distance >= pow(BLOCK(0.5f), 2))
							item->Animation.TargetState = BADDY_STATE_SWORD_HIT_FRONT;
						else if (Random::TestProbability(1 / 2.0f))
							item->Animation.TargetState = BADDY_STATE_SWORD_HIT_LEFT;
						else
							item->Animation.TargetState = BADDY_STATE_SWORD_HIT_RIGHT;
						
						break;
					}
				}

				item->Animation.TargetState = BADDY_STATE_WALK;
				break;

			case BADDY_STATE_WALK:
				currentCreature->MaxTurn = ANGLE(7.0f);
				currentCreature->LOT.IsMonkeying = false;
				currentCreature->LOT.IsJumping = false;
				currentCreature->Flags = 0;

				if (laraAI.ahead)
					joint3 = laraAI.angle;
				else if (laraAI.ahead)
					joint3 = laraAI.angle;

				if (Targetable(item, &AI) && item->ItemFlags[2] > 0)
				{
					item->Animation.TargetState = BADDY_STATE_IDLE;
					break;
				}

				if (canJump1Sector || canJump2Sectors)
				{
					currentCreature->MaxTurn = 0;
					item->Animation.TargetState = BADDY_STATE_IDLE;
					break;
				}

				if (currentCreature->ReachedGoal && currentCreature->MonkeySwingAhead)
				{
					item->Animation.TargetState = BADDY_STATE_IDLE;
					break;
				}

				if (item->ItemFlags[2] < 1)
				{
					if (!item->TestMeshSwapFlags(MESHSWAPFLAGS_BADDY_SWORD_SIMPLE) && 
						!item->TestMeshSwapFlags(MESHSWAPFLAGS_BADDY_SWORD_NINJA))
					{
						item->Animation.TargetState = BADDY_STATE_IDLE;
						break;
					}
				}

				if (AI.ahead && AI.distance < pow(BLOCK(0.5f), 2))
				{
					item->Animation.TargetState = BADDY_STATE_IDLE;
					break;
				}

				if (AI.bite)
				{
					if (AI.distance < pow(482, 2))
					{
						item->Animation.TargetState = BADDY_STATE_IDLE;
						break;
					}

					if (AI.distance < pow(BLOCK(1), 2))
					{
						item->Animation.TargetState = BADDY_STATE_WALK_SWORD_HIT_RIGHT;
						break;
					}
				}

				if (roll || jump)
				{
					item->Animation.ActiveState = BADDY_STATE_IDLE;
					break;
				}

				if (currentCreature->Mood == MoodType::Attack &&
					!(currentCreature->JumpAhead) &&
					AI.distance > pow(BLOCK(1), 2))
				{
					item->Animation.TargetState = BADDY_STATE_RUN;
				}

				break;

			case BADDY_STATE_RUN:
				currentCreature->MaxTurn = ANGLE(11.0f);
				tilt = abs(angle) / 2;

				if (AI.ahead)
					joint3 = AI.angle;
				
				if (Random::GenerateInt(0, 30) > 20 &&
					objectNumber == ID_BADDY2 &&
					item->Animation.FrameNumber == FRAME_BADDY_RUN_TO_SOMERSAULT &&
					height3 == height1 &&
					abs(height1 - item->Pose.Position.y) < CLICK(1.5f) &&
					(AI.angle > -ANGLE(22.5f) && AI.angle < ANGLE(22.5f) && AI.distance < pow(BLOCK(3), 2) || height2 >= (height1 + CLICK(2))))
				{
					item->Animation.TargetState = BADDY_STATE_SOMERSAULT;
					currentCreature->MaxTurn = 0;
					break;
				}

				if (Targetable(item, &AI) &&
					item->ItemFlags[2] > 0 ||
					canJump1Sector ||
					canJump2Sectors ||
					currentCreature->MonkeySwingAhead ||
					item->AIBits & FOLLOW ||
					AI.distance < pow(614, 2) ||
					currentCreature->JumpAhead)
				{
					item->Animation.TargetState = BADDY_STATE_IDLE;
					break;
				}

				if (AI.distance < pow(BLOCK(1), 2))
				{
					item->Animation.TargetState = BADDY_STATE_WALK;
					break;
				}

				break;

			case BADDY_STATE_SWORD_HIT_RIGHT:
			case BADDY_STATE_SWORD_HIT_FRONT:
			case BADDY_STATE_SWORD_HIT_LEFT:
			case BADDY_STATE_WALK_SWORD_HIT_RIGHT:
				currentCreature->MaxTurn = 0;

				if (item->Animation.ActiveState == BADDY_STATE_SWORD_HIT_RIGHT &&
					AI.distance < pow(BLOCK(0.5f), 2))
				{
					item->Animation.TargetState = BADDY_STATE_SWORD_HIT_LEFT;
				}

				if (AI.ahead)
				{
					joint1 = AI.angle;
					joint2 = AI.xAngle;
				}

				if (item->Animation.ActiveState != BADDY_STATE_SWORD_HIT_FRONT ||
					item->Animation.FrameNumber < FRAME_BADDY_SWORD_HIT_NO_DAMAGE_MAX)
				{
					if (abs(AI.angle) >= ANGLE(7.0f))
					{
						if (AI.angle >= 0)
							item->Pose.Orientation.y += ANGLE(7.0f);
						else
							item->Pose.Orientation.y -= ANGLE(7.0f);
					}
					else
						item->Pose.Orientation.y += AI.angle;
				}

				if (!currentCreature->Flags)
				{
					if (item->TouchBits.Test(BaddySwordAttackJoints))
					{
						if (item->Animation.FrameNumber > FRAME_BADDY_SWORD_HIT_DAMAGE_MIN &&
							item->Animation.FrameNumber < FRAME_BADDY_SWORD_HIT_DAMAGE_MAX)
						{
							DoDamage(creature->Enemy, 120);
							CreatureEffect2(item, BaddySwordBite, 10, item->Pose.Orientation.y, DoBloodSplat);
							currentCreature->Flags = 1;
						}
					}
				}

				if (item->Animation.FrameNumber == GetAnimData(*item).EndFrameNumber)
					currentCreature->Flags = 0;

				break;

			case BADDY_STATE_MONKEY_IDLE:
				currentCreature->MaxTurn = 0;
				currentCreature->Flags = 0;
				joint1 = 0;
				joint2 = 0;

				probe = GetPointCollision(*item);

				if (laraAI.ahead && laraAI.distance < pow(682, 2) &&
					(LaraItem->Animation.ActiveState == LS_MONKEY_IDLE ||
						LaraItem->Animation.ActiveState == LS_MONKEY_FORWARD ||
						LaraItem->Animation.ActiveState == LS_MONKEY_SHIMMY_LEFT ||
						LaraItem->Animation.ActiveState == LS_MONKEY_SHIMMY_RIGHT ||
						LaraItem->Animation.ActiveState == LS_MONKEY_TURN_180 ||
						LaraItem->Animation.ActiveState == LS_MONKEY_TURN_LEFT ||
						LaraItem->Animation.ActiveState == LS_MONKEY_TURN_RIGHT))
				{
					item->Animation.TargetState = BADDY_STATE_MONKEY_PUSH_OFF;
				}
				else if (item->BoxNumber != currentCreature->LOT.TargetBox &&
					currentCreature->MonkeySwingAhead ||
					probe.GetCeilingHeight() != (probe.GetFloorHeight() - CLICK(6)))
				{
					item->Animation.TargetState = BADDY_STATE_MONKEY_FORWARD;
				}
				else
				{
					item->Animation.TargetState = BADDY_STATE_MONKEY_FALL_LAND;
					currentCreature->LOT.IsMonkeying = false;
					currentCreature->LOT.IsJumping = false;
				}

				break;

			case BADDY_STATE_MONKEY_FORWARD:
				currentCreature->MaxTurn = ANGLE(7.0f);
				currentCreature->LOT.IsJumping = true;
				currentCreature->LOT.IsMonkeying = true;
				currentCreature->Flags = 0;
				joint1 = 0;
				joint2 = 0;

				if (item->BoxNumber == currentCreature->LOT.TargetBox ||
					!currentCreature->MonkeySwingAhead)
				{
					probe = GetPointCollision(*item);

					if (probe.GetCeilingHeight() == probe.GetFloorHeight() - CLICK(6))
						item->Animation.TargetState = BADDY_STATE_MONKEY_IDLE;
				}

				if (laraAI.ahead)
				{
					if (laraAI.distance < pow(682, 2))
					{
						if (LaraItem->Animation.ActiveState == LS_MONKEY_IDLE ||
							LaraItem->Animation.ActiveState == LS_MONKEY_FORWARD ||
							LaraItem->Animation.ActiveState == LS_MONKEY_SHIMMY_LEFT ||
							LaraItem->Animation.ActiveState == LS_MONKEY_SHIMMY_RIGHT ||
							LaraItem->Animation.ActiveState == LS_MONKEY_TURN_180 ||
							LaraItem->Animation.ActiveState == LS_MONKEY_TURN_LEFT ||
							LaraItem->Animation.ActiveState == LS_MONKEY_TURN_RIGHT)
						{
							item->Animation.TargetState = BADDY_STATE_MONKEY_IDLE;
						}
					}
				}

				break;

			case BADDY_STATE_MONKEY_PUSH_OFF:
				currentCreature->MaxTurn = ANGLE(7.0f);

				if (!currentCreature->Flags)
				{
					if (item->TouchBits.TestAny())
					{
						SetAnimation(*LaraItem, LA_JUMP_UP);
						LaraItem->Animation.IsAirborne = true;
						LaraItem->Animation.Velocity.y = 2;
						LaraItem->Animation.Velocity.y = 1;
						LaraItem->Pose.Position.y += CLICK(0.75f);
						Lara.Control.HandStatus = HandStatus::Free;
						currentCreature->Flags = 1;
					}
				}

				break;

			case BADDY_STATE_ROLL_LEFT:
			case BADDY_STATE_JUMP_RIGHT:
				item->Status = ITEM_ACTIVE;
				currentCreature->MaxTurn = 0;
				currentCreature->Alerted = false;
				break;

			case BADDY_STATE_CROUCH:
				if (!item->ItemFlags[0])
				{
					if (currentCreature->Enemy)
					{
						if ((currentCreature->Enemy->ObjectNumber == ID_SMALLMEDI_ITEM ||
							currentCreature->Enemy->ObjectNumber == ID_BIGMEDI_ITEM ||
							currentCreature->Enemy->ObjectNumber == ID_UZI_AMMO_ITEM) &&
							AI.distance < pow(BLOCK(0.5f), 2))
						{
							item->Animation.TargetState = BADDY_STATE_CROUCH_PICKUP;
							break;
						}
					}

					if (currentCreature->Alerted)
						item->Animation.TargetState = BADDY_STATE_CROUCH_TO_STAND;
				}
				else
				{
					if (AI.distance >= pow(682, 2))
						break;
					
					item->Animation.TargetState = BADDY_STATE_CROUCH_TO_STAND;
					currentCreature->Enemy = nullptr;
				}

				break;

			case BADDY_STATE_CROUCH_PICKUP:
				ClampRotation(item->Pose, AI.angle, ANGLE(11.0f));

				if (item->Animation.FrameNumber != FRAME_BADDY_CROUCH_PICKUP)
					break;

				if (!currentCreature->Enemy)
					break;

				if (currentCreature->Enemy->ObjectNumber != ID_SMALLMEDI_ITEM &&
					currentCreature->Enemy->ObjectNumber != ID_BIGMEDI_ITEM &&
					currentCreature->Enemy->ObjectNumber != ID_UZI_AMMO_ITEM)
				{
					break;
				}

				if (currentCreature->Enemy->RoomNumber == NO_VALUE ||
					currentCreature->Enemy->Status == ITEM_INVISIBLE ||
					currentCreature->Enemy->InDrawRoom)
				{
					currentCreature->Enemy = nullptr;
					break;
				}

				if (currentCreature->Enemy->ObjectNumber == ID_SMALLMEDI_ITEM)
					item->HitPoints += Objects[item->ObjectNumber].HitPoints / 2;
				else if (currentCreature->Enemy->ObjectNumber == ID_BIGMEDI_ITEM)
					item->HitPoints = Objects[item->ObjectNumber].HitPoints;
				else if (currentCreature->Enemy->ObjectNumber == ID_UZI_AMMO_ITEM)
					item->ItemFlags[2] += BADDY_UZI_AMMO;
				else
				{
					currentCreature->Enemy = nullptr;
					break;
				}
			
				KillItem(currentCreature->Enemy->Index);

				// Cancel enemy pointer for other active baddys
				for (int i = 0; i < ActiveCreatures.size(); i++)
				{
					if (ActiveCreatures[i]->ItemNumber != NO_VALUE && ActiveCreatures[i]->ItemNumber != itemNumber && ActiveCreatures[i]->Enemy == creature->Enemy)
						ActiveCreatures[i]->Enemy = nullptr;
				}

				creature->Enemy = nullptr;
				break;

			case BADDY_STATE_AIM:
				currentCreature->MaxTurn = 0;

				if (AI.ahead)
				{
					joint1 = AI.angle;
					joint2 = AI.xAngle;
				}
				ClampRotation(item->Pose, AI.angle, ANGLE(7));

				if (!Targetable(item, &AI) ||
					item->ItemFlags[2] < 1)
				{
					item->Animation.TargetState = BADDY_STATE_IDLE;
					break;
				}

				item->Animation.TargetState = BADDY_STATE_FIRE;
				break;

			case BADDY_STATE_FIRE:

				if (AI.ahead)
				{
					joint1 = AI.angle;
					joint2 = AI.xAngle;
				}
				ClampRotation(item->Pose, AI.angle, ANGLE(7.0f));

				if (item->Animation.FrameNumber >= FRAME_BADDY_FIRE_MAX ||
					item->Animation.FrameNumber == FRAME_BADDY_FIRE_MIN)
				{
					break;
				}

				if (!item->HitStatus)
					item->ItemFlags[2]--;

				if (!ShotLara(item, &AI, BaddyGunBite, joint1, 15))
					item->Animation.TargetState = BADDY_STATE_IDLE;

				creature->MuzzleFlash[0].Bite = BaddyGunBite;
				creature->MuzzleFlash[0].Delay = 2;
				break;

			default:
				break;

			case BADDY_STATE_HOLSTER_GUN:
				if (item->Animation.FrameNumber == FRAME_BADDY_HOLSTER_GUN)
					item->SetMeshSwapFlags(MESHSWAPFLAGS_BADDY_EMPTY);

				break;

			case BADDY_STATE_DRAW_GUN:
				if (item->Animation.FrameNumber == FRAME_BADDY_DRAW_GUN)
					item->SetMeshSwapFlags(MESHSWAPFLAGS_BADDY_GUN);

				break;

			case BADDY_STATE_HOLSTER_SWORD:
				if (item->Animation.FrameNumber == FRAME_BADDY_HOLSTER_SWORD)
					item->SetMeshSwapFlags(MESHSWAPFLAGS_BADDY_EMPTY);
				
				break;

			case BADDY_STATE_DRAW_SWORD:
				if (item->Animation.FrameNumber != FRAME_BADDY_DRAW_SWORD)
					break;

				if (item->ObjectNumber == ID_BADDY1)
					item->SetMeshSwapFlags(MESHSWAPFLAGS_BADDY_SWORD_SIMPLE);
				else
					item->SetMeshSwapFlags(MESHSWAPFLAGS_BADDY_SWORD_NINJA);

				break;

			case BADDY_STATE_DODGE:
				currentCreature->MaxTurn = 0;

				ClampRotation(item->Pose, AI.angle, ANGLE(11.0f));

				if (laraAI.distance < pow(682, 2) ||
					item != Lara.TargetEntity)
				{
					item->Animation.TargetState = BADDY_STATE_DODGE_END;
				}

				break;

			case BADDY_STATE_BLIND:
				if (!FlashGrenadeAftershockTimer)
				{
					if (Random::TestProbability(1 / 128.0f))
						item->Animation.TargetState = BADDY_STATE_IDLE;
				}

				break;

			case BADDY_STATE_SOMERSAULT:
				if (item->Animation.AnimNumber == BADDY_ANIM_SOMERSAULT_END)
				{
					ClampRotation(item->Pose, AI.angle, ANGLE(7.0f));
					break;
				}

				if (item->Animation.FrameNumber != FRAME_BADDY_SOMERSAULT_START_TAKE_OFF)
					break;

				currentCreature->LOT.IsJumping = true;
				break;

			case BADDY_STATE_JUMP_FORWARD_1_BLOCK:
			case BADDY_STATE_JUMP_FORWARD_2_BLOCKS:
				if (item->ItemFlags[0] >= 0)
					break;

				if (item->Animation.AnimNumber != BADDY_ANIM_STAND_TO_JUMP_FORWARD)
					item->ItemFlags[0] += 2;
				
				break;
			}
		}

		CreatureTilt(item, tilt);
		CreatureJoint(item, 0, joint1);
		CreatureJoint(item, 1, joint2);
		CreatureJoint(item, 2, joint3);

		if (item->Animation.ActiveState >= BADDY_STATE_JUMP_FORWARD_2_BLOCKS ||
			item->Animation.ActiveState == BADDY_STATE_JUMP_FORWARD_1_BLOCK ||
			item->Animation.ActiveState == BADDY_STATE_MONKEY_FORWARD ||
			item->Animation.ActiveState == BADDY_STATE_DEATH ||
			item->Animation.ActiveState == BADDY_STATE_SOMERSAULT ||
			item->Animation.ActiveState == BADDY_STATE_BLIND)
		{
			CreatureAnimation(itemNumber, angle, 0);
		}
		else if (FlashGrenadeAftershockTimer <= 100)
		{
			int vault = CreatureVault(itemNumber, angle, 2, 260);

			switch (vault)
			{
			case 2:
				SetAnimation(*item, BADDY_ANIM_CLIMB_2_STEPS);
				creature->MaxTurn = 0;
				break;

			case 3:
				SetAnimation(*item, BADDY_ANIM_CLIMB_3_STEPS);
				creature->MaxTurn = 0;
				break;

			case 4:
				SetAnimation(*item, BADDY_ANIM_CLIMB_4_STEPS);
				creature->MaxTurn = 0;
				break;

			case -3:
				SetAnimation(*item, BADDY_ANIM_JUMP_OFF_3_STEPS);
				creature->MaxTurn = 0;
				break;

			case -4:
				SetAnimation(*item, BADDY_ANIM_JUMP_OFF_4_STEPS);
				creature->MaxTurn = 0;
				break;

			default:
				return;
			}
		}
		else
		{
			SetAnimation(*item, BADDY_ANIM_BLIND, Random::GenerateInt(0, 8));
			creature->MaxTurn = 0;
		}
	}

	void Baddy2Hit(ItemInfo& target, ItemInfo& source, std::optional<GameVector> pos, int damage, bool isExplosive, int jointIndex)
	{
		const auto& player = *GetLaraInfo(&source);
		const auto& object = Objects[target.ObjectNumber];

		if (pos.has_value())
		{
			if (target.Animation.ActiveState == BADDY_STATE_DODGE &&
				(player.Control.Weapon.GunType == LaraWeaponType::Pistol ||
				 player.Control.Weapon.GunType == LaraWeaponType::Shotgun ||
				 player.Control.Weapon.GunType == LaraWeaponType::Uzi ||
				 player.Control.Weapon.GunType == LaraWeaponType::HK ||
				 player.Control.Weapon.GunType == LaraWeaponType::Revolver))
			{
				// Baddy2 bullet deflection with sword.
				SoundEffect(SFX_TR4_BADDY_SWORD_RICOCHET, &target.Pose);
				TriggerRicochetSpark(*pos, source.Pose.Orientation.y, false);
				return;
			}
			else if (object.hitEffect == HitEffect::Blood)
			{
				DoBloodSplat(pos->x, pos->y, pos->z, Random::GenerateInt(4, 8), source.Pose.Orientation.y, pos->RoomNumber);
			}
		}

		DoItemHit(&target, damage, isExplosive);
	}
}
