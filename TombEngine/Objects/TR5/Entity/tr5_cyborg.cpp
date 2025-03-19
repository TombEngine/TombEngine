#include "framework.h"
#include "Objects/TR5/Entity/tr5_cyborg.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/box.h"
#include "Game/effects/effects.h"
#include "Game/effects/Electricity.h"
#include "Game/effects/item_fx.h"
#include "Game/effects/spark.h"
#include "Game/effects/tomb4fx.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/pickup/pickup.h"
#include "Game/Setup.h"
#include "Scripting/Internal/TEN/Objects/Moveable/MoveableObject.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Effects::Electricity;
using namespace TEN::Effects::Items;
using namespace TEN::Effects::Spark;

namespace TEN::Entities::Creatures::TR5
{
	constexpr auto CYBORG_HEALTH_MAX = 50;
	constexpr auto CYBORG_GUN_ATTACK_DAMAGE = 12;

	constexpr auto CYBORG_AWARE_RANGE	   = SQUARE(BLOCK(2));
	constexpr auto CYBORG_IDLE_RANGE	   = SQUARE(BLOCK(1));
	constexpr auto CYBORG_WALK_RANGE	   = SQUARE(BLOCK(3));
	constexpr auto CYBORG_GUN_ATTACK_RANGE = SQUARE(BLOCK(4));

	constexpr auto CYBORG_DISTURBANCE_VELOCITY = 20.0f;

	const auto CyborgGunBite = CreatureBiteInfo(Vector3(-32, 240, 50), 18);
	const auto CyborgJoints = std::vector<unsigned int>{ 15, 14, 13, 6, 5, 12, 7, 4, 10, 11, 19 };

	enum CyborgState
	{
		// No state 0.
		CYBORG_STATE_IDLE = 1,
		CYBORG_STATE_WALK = 2,
		CYBORG_STATE_RUN = 3,
		CYBORG_STATE_START_END_MONKEY = 4,
		CYBORG_STATE_MONKEY = 5,
		// No states 6-14.
		CYBORG_STATE_JUMP = 15,
		CYBORG_STATE_JUMP_2_BLOCKS = 16,
		CYBORG_STATE_CLIMBUP_6CLICK = 17,
		CYBORG_STATE_CLIMBUP_4CLICK = 18,
		CYBORG_STATE_CLIMBUP_3CLICK = 19,
		// No states 20-22.
		CYBORG_STATE_3CLICK_JUMPDOWN = 23,
		CYBORG_STATE_4CLICK_JUMPDOWN = 24,
		CYBORG_STATE_6CLICK_JUMPDOWN = 25,
		// No states 25-37.
		CYBORG_STATE_AIM = 38,
		CYBORG_STATE_FIRE = 39,
		// No states 40-41.
		CYBORG_STATE_GASSED = 42,
		CYBORG_STATE_DEATH = 43
	};

	enum CyborgAnim
	{
		CYBORG_ANIM_WALK = 0,
		CYBORG_ANIM_RUN = 1,
		// No anims 2-3.
		CYBORG_ANIM_IDLE = 4,
		// No anims 5 - 11.
		CYBORG_ANIM_WALK_START = 13, //
		CYBORG_ANIM_WALK_FINISH = 14,//
		CYBORG_ANIM_RUN_START = 15,
		CYBORG_ANIM_RUN_FINISH = 16,
		CYBORG_ANIM_DODGE_END = 17,
		CYBORG_ANIM_STAND_IDLE = 18,
		// No anims 19-21.
		CYBORG_ANIM_LEAP_FORWARD_START = 22,
		CYBORG_ANIM_1BLOCK_LEAP_MIDAIR = 23,
		CYBORG_ANIM_LEAP_LAND = 24,
		CYBORG_ANIM_2BLOCK_LEAP_MIDAIR = 25,
		CYBORG_ANIM_TURN_LEFT = 26,
		CYBORG_ANIM_CLIMBUP_6CLICK = 27, //Maybe 5 click? 
		CYBORG_ANIM_CLIMBUP_4CLICK = 28,
		CYBORG_ANIM_CLIMBUP_3CLICK = 29,
		// No anims 30-31.
		CYBORG_ANIM_SUMMON = 32,// (Not sure)
		// No anims 33-34.
		CYBORG_ANIM_6CLICK_JUMPDOWN = 35,
		// No anims 35-40.
		CYBORG_ANIM_4CLICK_JUMPDOWN = 41,
		CYBORG_ANIM_3CLICK_JUMPDOWN = 42,
		// No anims 43-44.
		CYBORG_ANIM_JUMP_FORWARD_LEFT_FOOT_FIRST = 45,
		CYBORG_ANIM_JUMP_FORWARD_RIGHT_FOOT_FIRST = 46,
		// No anims 47-49.
		CYBORG_ANIM_HUGE_LEAP = 50,
		// No anims 51-55.
		CYBORG_CYBORG_ANIM_TURN_RIGHT = 56,
		CYBORG_ANIM_SHUFFLE_RIGHT = 57,
		CYBORG_ANIM_SHUFFLE_LEFT = 58,
		CYBORG_ANIM_MONKEY_TO_FREEFALL = 59,
		CYBORG_ANIM_HOLSTERGUN = 60,
		CYBORG_ANIM_AIM = 61,
		CYBORG_ANIM_FIRE = 62,
		CYBORG_ANIM_LOWERGUN = 63,
		// No anims 64-67.
		CYBORG_ANIM_CHOKE_DEATH = 68,
		CYBORG_ANIM_ELECTROCUTION_DEATH = 69,
		CYBORG_ANIM_FALLBACK_DEATH = 70,
		CYBORG_ANIM_DEATH_END = 71
	};

	void InitializeCyborg(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(item, CYBORG_ANIM_IDLE);
	}

	void CyborgControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto& item = g_Level.Items[itemNumber];
		auto& creature = *GetCreatureInfo(&item);

		short headingAngle = 0;
		short joint0 = 0;
		short joint1 = 0;
		short joint2 = 0;

		int x = item.Pose.Position.x;
		int z = item.Pose.Position.z;

		int dx = 808 * phd_sin(item.Pose.Orientation.y);
		int dz = 808 * phd_cos(item.Pose.Orientation.y);

		x += dx;
		z += dz;
		int height1 = GetPointCollision(Vector3i(x, item.Pose.Position.y, z), item.RoomNumber).GetFloorHeight();

		x += dx;
		z += dz;
		int height2 = GetPointCollision(Vector3i(x, item.Pose.Position.y, z), item.RoomNumber).GetFloorHeight();

		x += dx;
		z += dz;
		auto pointColl = GetPointCollision(Vector3i(x, item.Pose.Position.y, z), item.RoomNumber);
		int roomNumber = pointColl.GetRoomNumber();
		int height3 = pointColl.GetFloorHeight();

		bool canJump1block = true;
		if (item.BoxNumber == LaraItem->BoxNumber ||
			item.Pose.Position.y >= (height1 - CLICK(1.5f)) ||
			item.Pose.Position.y >= (height2 + CLICK(2)) ||
			item.Pose.Position.y <= (height2 - CLICK(2)))
		{
			canJump1block = false;
		}

		bool canJump2blocks = true;
		if (item.BoxNumber == LaraItem->BoxNumber ||
			item.Pose.Position.y >= (height1 - CLICK(1.5f)) ||
			item.Pose.Position.y >= (height2 - CLICK(1.5f)) ||
			item.Pose.Position.y >= (height3 + CLICK(2)) ||
			item.Pose.Position.y <= (height3 - CLICK(2)))
		{
			canJump2blocks = false;
		}

		if (creature.MuzzleFlash[0].Delay != 0)
			creature.MuzzleFlash[0].Delay--;

		if (item.AIBits)
			GetAITarget(&creature);
		else
			creature.Enemy = LaraItem;

		AI_INFO AI;
		CreatureAIInfo(&item, &AI);

		// Swap joint meshes as damage is taken.
		if (item.HitStatus)
		{
			if (Random::TestProbability(1 / 8.0f))
			{
				if (item.ItemFlags[0] < CyborgJoints.size())
				{
					unsigned int jointBit = 1 << CyborgJoints[item.ItemFlags[0]];
					
					item.ItemFlags[1] |= jointBit >> 4;

					item.SetMeshSwapFlags(item.ItemFlags[1] << 4);
					item.ItemFlags[0]++;
				}
			}
		}

		int randomIndex = TestEnvironment(ENV_FLAG_WATER, item.RoomNumber) ?
			Random::GenerateInt(0, 4) : Random::GenerateInt(0, 70);

		if (randomIndex < item.ItemFlags[0])
		{
			auto pos = GetJointPosition(&item, CyborgJoints[randomIndex], Vector3i(0, 0, 50)).ToVector3();

			SpawnElectricityGlow(pos, 48, 32, 32, 64);
	
			SpawnCyborgSpark(pos);
			SpawnDynamicLight(pos.x, pos.y, pos.z, Random::GenerateInt(4, 20), 31, 63, 127);

			SoundEffect(SFX_TR5_HITMAN_SPARKS_SHORT, &item.Pose);

			if (randomIndex == 5 || randomIndex == 7 || randomIndex == 10)
			{
				auto pos2 = Vector3::Zero;
				auto pointColl2 = GetPointCollision(pos2, item.RoomNumber);
				
				switch (randomIndex)
				{
				case 5:
					pos2 = GetJointPosition(&item, 15, Vector3i(0, 0, 50)).ToVector3();
					break;

				case 7:
					pos2 = GetJointPosition(&item, 6, Vector3i(0, 0, 50)).ToVector3();
					pointColl2 = GetPointCollision(pos2, item.RoomNumber);

					if (TestEnvironment(ENV_FLAG_WATER, pointColl2.GetRoomNumber()) && item.HitPoints > 0)
					{
						DropPickups(&item);
						DoDamage(&item, INT_MAX);
						SetAnimation(item, CYBORG_ANIM_ELECTROCUTION_DEATH);
					}
				
					break;

				case 10:
					pos2 = GetJointPosition(&item, 12, Vector3i(0, 0, 50)).ToVector3();
					break;
				}

				SpawnElectricity(pos, pos2, Random::GenerateInt(8, 16), 32, 64, 128, 24, (int)ElectricityFlags::Spline | (int)ElectricityFlags::ThinOut | (int)ElectricityFlags::ThinIn, 6, 3);
			}
		}

		if (item.HitPoints > 0)
		{
			AI_INFO laraAI;
			if (creature.Enemy->IsLara())
			{
				laraAI.angle = AI.angle;
				laraAI.distance = AI.distance;
			}
			else
			{
				int dx = LaraItem->Pose.Position.x - item.Pose.Position.x;
				int dz = LaraItem->Pose.Position.z - item.Pose.Position.z;
				laraAI.angle = phd_atan(dz, dx) - item.Pose.Orientation.y;
				laraAI.distance = SQUARE(dx) + SQUARE(dz);
			}
			
			GetCreatureMood(&item, &AI, !creature.Enemy->IsLara());

			if (TestEnvironment(ENV_FLAG_NO_LENSFLARE, &item))
			{
				if (!(GlobalCounter & 7))
					item.HitPoints--;

				creature.Mood = MoodType::Escape;
				item.Animation.ActiveState = CYBORG_STATE_RUN;

				if (item.HitPoints <= 0)
					SetAnimation(item, CYBORG_ANIM_CHOKE_DEATH);
			}
			// Keep cyborg invincible if not in gassed room or shocked in water.
			else
			{
				item.HitPoints = CYBORG_HEALTH_MAX;
			}

			CreatureMood(&item, &AI, !creature.Enemy->IsLara());

			headingAngle = CreatureTurn(&item, creature.MaxTurn);

			if (laraAI.distance < CYBORG_AWARE_RANGE &&
				LaraItem->Animation.Velocity.z > CYBORG_DISTURBANCE_VELOCITY ||
				item.HitStatus ||
				TargetVisible(&item, &laraAI))
			{
				if (!(item.AIBits & FOLLOW))
				{
					creature.Enemy = LaraItem;
					AlertAllGuards(itemNumber);
				}
			}

			int height = 0;

			switch (item.Animation.ActiveState)
			{
			case CYBORG_STATE_IDLE:
				creature.MaxTurn = 0;
				creature.Flags = 0;
				creature.LOT.IsJumping = false;
				joint2 = laraAI.angle;

				if (AI.ahead && item.AIBits != GUARD)
				{
					joint0 = AI.angle / 2;
					joint1 = AI.xAngle;
				}

				if (item.Animation.RequiredState != NO_VALUE)
				{
					item.Animation.TargetState = item.Animation.RequiredState;
				}
				else
				{
					if (item.AIBits & GUARD)
					{
						joint2 = AIGuard(&creature);

						if (item.AIBits & PATROL1)
						{
							item.TriggerFlags--;
							if (item.TriggerFlags < 1)
								item.AIBits |= PATROL1;
						}
					}
					else if (Targetable(&item, &AI))
					{
						if (AI.distance < CYBORG_GUN_ATTACK_RANGE || AI.zoneNumber != AI.enemyZone)
						{
							item.Animation.TargetState = CYBORG_STATE_AIM;
						}
						else if (item.AIBits != MODIFY)
						{
							item.Animation.TargetState = CYBORG_STATE_WALK;
						}
					}
					else
					{
						if (item.AIBits & PATROL1)
						{
							item.Animation.TargetState = CYBORG_STATE_WALK;
						}
						else
						{
							if (canJump1block || canJump2blocks)
							{
								SetAnimation(item, CYBORG_ANIM_LEAP_FORWARD_START);
								creature.MaxTurn = 0;

								if (canJump2blocks)
									item.Animation.TargetState = CYBORG_STATE_JUMP_2_BLOCKS;

								creature.LOT.IsJumping = true;
							}
							else if (!creature.MonkeySwingAhead)
							{
								if (creature.Mood != MoodType::Bored)
								{
									if (AI.distance < CYBORG_WALK_RANGE || item.AIBits & FOLLOW)
										item.Animation.TargetState = CYBORG_STATE_WALK;
									else
										item.Animation.TargetState = CYBORG_STATE_RUN;
								}
								else
								{
									item.Animation.TargetState = CYBORG_STATE_IDLE;
								}
							}
							else
							{
								pointColl = GetPointCollision(item.Pose.Position, roomNumber);
								roomNumber = pointColl.GetRoomNumber();
								height = pointColl.GetFloorHeight();

								if (pointColl.GetCeilingHeight() == (height - BLOCK(1.5f)))
									item.Animation.TargetState = CYBORG_STATE_START_END_MONKEY;
								else
									item.Animation.TargetState = CYBORG_STATE_WALK;
							}
						}
					}
				}

				break;

			case CYBORG_STATE_WALK:
				creature.MaxTurn = ANGLE(5.0f);
				creature.LOT.IsJumping = false;

				if (Targetable(&item, &AI) &&
					(AI.distance < CYBORG_GUN_ATTACK_RANGE ||
						AI.zoneNumber != AI.enemyZone))
				{
					item.Animation.TargetState = CYBORG_STATE_IDLE;
					item.Animation.RequiredState = CYBORG_STATE_AIM;
				}
				else
				{
					if (canJump1block || canJump2blocks)
					{
						SetAnimation(item, CYBORG_ANIM_LEAP_FORWARD_START);
						creature.MaxTurn = 0;

						if (canJump2blocks)
							item.Animation.TargetState = CYBORG_STATE_JUMP_2_BLOCKS;

						creature.LOT.IsJumping = true;
					}
					else if (!creature.MonkeySwingAhead)
					{
						if (AI.distance < CYBORG_IDLE_RANGE)
						{
							item.Animation.TargetState = CYBORG_STATE_IDLE;
						}
						else
						{
							if (AI.distance > CYBORG_WALK_RANGE)
							{
								if (!item.AIBits)
									item.Animation.TargetState = CYBORG_STATE_RUN;
							}
						}
					}
					else
					{
						item.Animation.TargetState = CYBORG_STATE_IDLE;
					}
				}

				break;

			case CYBORG_STATE_RUN:
				creature.MaxTurn = ANGLE(10.0f);
				creature.LOT.IsJumping = false;

				if (Targetable(&item, &AI) &&
					(AI.distance < CYBORG_GUN_ATTACK_RANGE ||
						AI.zoneNumber != AI.enemyZone))
				{
					item.Animation.TargetState = CYBORG_STATE_IDLE;
					item.Animation.RequiredState = CYBORG_STATE_AIM;
				}
				else if (canJump1block || canJump2blocks)
				{
					SetAnimation(item, CYBORG_ANIM_LEAP_FORWARD_START);
					creature.MaxTurn = 0;

					if (canJump2blocks)
						item.Animation.TargetState = CYBORG_STATE_JUMP_2_BLOCKS;

					creature.LOT.IsJumping = true;
				}
				else
				{
					if (creature.MonkeySwingAhead)
					{
						item.Animation.TargetState = CYBORG_STATE_IDLE;
					}
					else if (AI.distance < CYBORG_WALK_RANGE)
					{
						item.Animation.TargetState = CYBORG_STATE_WALK;
					}
				}

				break;

			case CYBORG_STATE_START_END_MONKEY:
				creature.MaxTurn = 0;

				if (item.BoxNumber == creature.LOT.TargetBox ||
					!creature.MonkeySwingAhead)
				{
					pointColl = GetPointCollision(item.Pose.Position, roomNumber);
					roomNumber = pointColl.GetRoomNumber();
					height = pointColl.GetFloorHeight();

					if (pointColl.GetCeilingHeight() == height - BLOCK(1.5f), 2)
						item.Animation.TargetState = CYBORG_STATE_IDLE;
				}
				else
				{
					item.Animation.TargetState = CYBORG_STATE_MONKEY;
				}

				break;

			case CYBORG_STATE_MONKEY:
				creature.MaxTurn = ANGLE(5.0f);
				creature.LOT.IsMonkeying = true;
				creature.LOT.IsJumping = true;

				if (item.BoxNumber == creature.LOT.TargetBox ||
					!creature.MonkeySwingAhead)
				{
					pointColl = GetPointCollision(item.Pose.Position, roomNumber);
					roomNumber = pointColl.GetRoomNumber();
					height = pointColl.GetFloorHeight();

					if (pointColl.GetCeilingHeight() == height - BLOCK(1.5f), 2)
						item.Animation.TargetState = CYBORG_STATE_START_END_MONKEY;
				}

				break;

			case CYBORG_STATE_AIM:
				creature.MaxTurn = 0;
				creature.Flags = 0;
				joint0 = laraAI.angle / 2;
				joint2 = laraAI.angle / 2;

				if (AI.ahead)
					joint1 = AI.xAngle;

				if (abs(AI.angle) >= ANGLE(2.0f))
				{
					if (AI.angle >= 0)
						item.Pose.Orientation.y += ANGLE(2.0f);
					else
						item.Pose.Orientation.y -= ANGLE(2.0f);
				}
				else
				{
					item.Pose.Orientation.y += AI.angle;
				}

				if (Targetable(&item, &AI) &&
					(AI.distance < CYBORG_GUN_ATTACK_RANGE ||
						AI.zoneNumber != AI.enemyZone))
				{
					item.Animation.TargetState = CYBORG_STATE_FIRE;
				}
				else
				{
					item.Animation.TargetState = CYBORG_STATE_IDLE;
				}

				break;

			case CYBORG_STATE_FIRE:
				joint0 = laraAI.angle / 2;
				joint2 = laraAI.angle / 2;

				if (AI.ahead)
					joint1 = AI.xAngle;

				creature.MaxTurn = 0;

				if (abs(AI.angle) >= ANGLE(2.0f))
				{
					if (AI.angle >= 0)
						item.Pose.Orientation.y += ANGLE(2.0f);
					else
						item.Pose.Orientation.y -= ANGLE(2.0f);
				}
				else
				{
					item.Pose.Orientation.y += AI.angle;
				}

				if (item.Animation.FrameNumber > 6 &&
					item.Animation.FrameNumber < 16 &&
					((byte)item.Animation.FrameNumber & 1))
				{
					ShotLara(&item, &AI, CyborgGunBite, joint0, CYBORG_GUN_ATTACK_DAMAGE);
					creature.MuzzleFlash[0].Bite = CyborgGunBite;
					creature.MuzzleFlash[0].Delay = 1;
				}

				break;
			}
		}
		else if (item.Animation.ActiveState == CYBORG_STATE_DEATH && LaraItem->Effect.Type == EffectType::None)
		{
			auto pos = GetJointPosition(LaraItem, LM_RFOOT);
			auto footProbeRight = GetPointCollision(pos, LaraItem->RoomNumber);
			pos = GetJointPosition(LaraItem, LM_LFOOT);
			auto footProbeLeft = GetPointCollision(pos, LaraItem->RoomNumber);

			short roomNumberLeft = LaraItem->RoomNumber;
			GetFloor(pos.x, pos.y, pos.z, &roomNumberLeft);

			pos = GetJointPosition(LaraItem, LM_RFOOT);
			short roomNumberRight = LaraItem->RoomNumber;
			GetFloor(pos.x, pos.y, pos.z, &roomNumberRight);

			auto* roomRight = &g_Level.Rooms[roomNumberRight];
			auto* roomLeft = &g_Level.Rooms[roomNumberLeft];

			short flipNumber = g_Level.Rooms[item.RoomNumber].flipNumber;

			if (TestEnvironment(ENV_FLAG_WATER, footProbeLeft.GetRoomNumber()) ||
				TestEnvironment(ENV_FLAG_WATER, footProbeRight.GetRoomNumber()))
			{
				if (roomLeft->flipNumber == flipNumber || roomRight->flipNumber == flipNumber)
				{
					ItemElectricBurn(creature.Enemy);
					DoDamage(creature.Enemy, INT_MAX);
				}
			}
		}

		CreatureJoint(&item, 0, joint0);
		CreatureJoint(&item, 1, joint1);
		CreatureJoint(&item, 2, joint2);

		if (creature.ReachedGoal)
		{
			if (creature.Enemy)
			{
				TestTriggers(
					creature.Enemy->Pose.Position.x,
					creature.Enemy->Pose.Position.y,
					creature.Enemy->Pose.Position.z, roomNumber, true);

				item.Animation.RequiredState = CYBORG_STATE_WALK;

				if (creature.Enemy->Flags & 2)
					item.ItemFlags[3] = (creature.Tosspad & 0xFF) - 1;

				if (creature.Enemy->Flags & 8)
				{
					item.Animation.RequiredState = CYBORG_STATE_IDLE;
					item.TriggerFlags = 300;
					item.AIBits = GUARD | PATROL1;
				}

				item.ItemFlags[3]++;
				creature.ReachedGoal = false;
				creature.Enemy = nullptr;
			}
		}

		if (item.Animation.ActiveState >= CYBORG_STATE_JUMP || item.Animation.ActiveState == CYBORG_STATE_MONKEY)
		{
			CreatureAnimation(itemNumber, headingAngle, 0);
		}
		else
		{
			switch (CreatureVault(itemNumber, headingAngle, 2, 260) + 4)
			{
			case 0:
				SetAnimation(item, CYBORG_ANIM_6CLICK_JUMPDOWN);
				creature.MaxTurn = 0;
				break;

			case 1:
				SetAnimation(item, CYBORG_ANIM_4CLICK_JUMPDOWN);
				creature.MaxTurn = 0;
				break;

			case 2:
				SetAnimation(item, CYBORG_ANIM_3CLICK_JUMPDOWN);
				creature.MaxTurn = 0;
				break;

			case 6:
				SetAnimation(item, CYBORG_ANIM_CLIMBUP_3CLICK);
				creature.MaxTurn = 0;
				break;

			case 7:
				SetAnimation(item, CYBORG_ANIM_CLIMBUP_4CLICK);
				creature.MaxTurn = 0;
				break;

			case 8:
				SetAnimation(item, CYBORG_ANIM_CLIMBUP_6CLICK);
				creature.MaxTurn = 0;
				break;

			default:
				return;
			}
		}
	}
}
