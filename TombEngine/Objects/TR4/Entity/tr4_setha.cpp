#include "framework.h"
#include "Objects/TR4/Entity/tr4_setha.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/effects/spark.h"
#include "Game/effects/item_fx.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Specific/clock.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Effects::Items;
using namespace TEN::Effects::Spark;
using namespace TEN::Math;

namespace TEN::Entities::TR4
{
	constexpr auto SETH_POUNCE_ATTACK_DAMAGE = 200;
	constexpr auto SETH_KILL_ATTACK_DAMAGE	 = 250;

	constexpr auto SETH_IDLE_RANGE					   = SQUARE(BLOCK(4));
	constexpr auto SETH_WALK_RANGE					   = SQUARE(BLOCK(3));
	constexpr auto SETH_RECOIL_RANGE				   = SQUARE(BLOCK(2));
	constexpr auto SETH_POUNCE_ATTACK_RANGE			   = SQUARE(BLOCK(3));
	constexpr auto SETH_KILL_ATTACK_RANGE			   = SQUARE(BLOCK(1));
	constexpr auto SETH_SINGLE_PROJECTILE_ATTACK_RANGE = SQUARE(BLOCK(4));
	constexpr auto SETH_DUAL_PROJECTILE_ATTACK_RANGE   = SQUARE(BLOCK(2.5f));

	constexpr auto SETH_HARD_RECOIL_RECOVER_CHANCE	  = 1 / 2.0f;
	constexpr auto SETH_DUAL_PROJECTILE_ATTACK_CHANCE = 1 / 2.0f;
	constexpr auto SETH_JUMP_PROJECTILE_ATTACK_CHANCE = 1 / 2.0f;

	constexpr auto SETH_WALK_TURN_RATE_MAX = ANGLE(7.0f);
	constexpr auto SETH_RUN_TURN_RATE_MAX  = ANGLE(11.0f);

	const auto SethBite1   = CreatureBiteInfo(Vector3(0, 220, 50), 17);
	const auto SethBite2   = CreatureBiteInfo(Vector3(0, 220, 50), 13);
	const auto SethAttack1 = CreatureBiteInfo(Vector3(-16, 200, 32), 13);
	const auto SethAttack2 = CreatureBiteInfo(Vector3(16, 200, 32), 17);

	const auto SethPounceAttackJoints1 = std::vector<unsigned int>{ 13, 14, 15 };
	const auto SethPounceAttackJoints2 = std::vector<unsigned int>{ 16, 17, 18 };

	enum SethState
	{
		// No state 0.
		SETH_STATE_IDLE = 1,
		SETH_STATE_WALK_FORWARD = 2,
		SETH_STATE_RUN_FORWARD = 3,
		SETH_STATE_POUNCE_ATTACK = 4,
		SETH_STATE_JUMP = 5,
		SETH_STATE_SOFT_RECOIL = 6,
		SETH_STATE_HARD_RECOIL = 7,
		SETH_STATE_KILL_ATTACK_START = 8,
		SETH_STATE_KILL_ATTACK_END = 9,
		SETH_STATE_HARD_RECOIL_RECOVER = 10,
		SETH_STATE_DUAL_PROJECTILE_ATTACK = 11,
		SETH_STATE_JUMP_PROJECTILE_ATTACK = 12,
		SETH_STATE_SINGLE_PROJECTILE_ATTACK = 13,
		SETH_STATE_HOVER = 14,
		SETH_STATE_HOVER_PROJECTILE_ATTACK = 15,
		SETH_STATE_HOVER_RECOIL = 16
	};

	enum SethAnim
	{
		SETH_ANIM_DUAL_PROJECTILE_ATTACK = 0,
		SETH_ANIM_JUMP_START = 1,
		SETH_ANIM_JUMP_CONTINUE = 2,
		SETH_ANIM_JUMP_END = 3,
		SETH_ANIM_JUMP_PROJECTILE_ATTACK = 4,
		SETH_ANIM_SINGLE_PROJECTILE_ATTACK_START = 5,
		SETH_ANIM_SINGLE_PROJECTILE_ATTACK_CONTINUE = 6,
		SETH_ANIM_SINGLE_PROJECTILE_ATTACK_END = 7,
		SETH_ANIM_WALK_FORWARD = 8,
		SETH_ANIM_WALK_FORWARD_TO_RUN = 9,
		SETH_ANIM_RUN_FORWARD = 10,
		SETH_ANIM_SOFT_RECOIL = 11,
		SETH_ANIM_IDLE = 12,
		SETH_ANIM_KILL_ATTACK_START = 13,
		SETH_ANIM_KILL_ATTACK_END = 14,
		SETH_ANIM_POUNCE_ATTACK_START = 15,
		SETH_ANIM_POUNCE_ATTACK_END = 16,
		SETH_ANIM_HARD_RECOIL_START = 17,
		SETH_ANIM_HARD_RECOIL_END = 18,
		SETH_ANIM_HARD_RECOIL_RECOVER = 19,
		SETH_ANIM_WALK_FORWARD_TO_IDLE = 20,
		SETH_ANIM_IDLE_TO_WALK_FORWARD = 21,
		SETH_ANIM_RUN_FORWARD_TO_IDLE = 22,
		SETH_ANIM_RUN_FORWARD_TO_WALK = 23,
		SETH_ANIM_HOVER_PROJECTILE_ATTACK = 24,
		SETH_ANIM_HOVER_RECOIL = 25,
		SETH_ANIM_IDLE_TO_HOVER = 26,
		SETH_ANIM_HOVER_IDLE_TO_LAND = 27,
		SETH_ANIM_HOVER_IDLE = 28
	};

	void InitializeSeth(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(item, SETH_ANIM_IDLE);
	}

	void SethControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto& creature = *GetCreatureInfo(item);

		int x = item->Pose.Position.x;
		int y = item->Pose.Position.y;
		int z = item->Pose.Position.z;

		int dx = 870 * phd_sin(item->Pose.Orientation.y);
		int dz = 870 * phd_cos(item->Pose.Orientation.y);

		int ceiling = GetPointCollision(Vector3i(x, y, z), item->RoomNumber).GetCeilingHeight();

		x += dx;
		z += dz;
		int height1 = GetPointCollision(Vector3i(x, y, z), item->RoomNumber).GetFloorHeight();

		x += dx;
		z += dz;
		int height2 = GetPointCollision(Vector3i(x, y, z), item->RoomNumber).GetFloorHeight();

		x += dx;
		z += dz;
		int height3 = GetPointCollision(Vector3i(x, y, z), item->RoomNumber).GetFloorHeight();

		bool canJump = false;
		if ((y < (height1 - CLICK(1.5f)) || y < (height2 - CLICK(1.5f))) &&
			(y < (height3 + CLICK(1)) && y >(height3 - CLICK(1)) || height3 == NO_HEIGHT))
		{
			canJump = true;
		}

		x = item->Pose.Position.x - dx;
		z = item->Pose.Position.z - dz;
		int height4 = GetPointCollision(Vector3i(x, y, z), item->RoomNumber).GetFloorHeight();

		AI_INFO AI;
		short angle = 0;

		if (item->HitPoints <= 0)
		{
			item->HitPoints = 0;
		}
		else
		{
			if (item->AIBits & AMBUSH)
				GetAITarget(&creature);
			else
				creature.Enemy = LaraItem;

			CreatureAIInfo(item, &AI);

			GetCreatureMood(item, &AI, true);
			CreatureMood(item, &AI, true);

			angle = CreatureTurn(item, creature.MaxTurn);

			switch (item->Animation.ActiveState)
			{
			case SETH_STATE_IDLE:
				creature.Flags = 0;
				creature.LOT.IsJumping = false;

				if (item->Animation.RequiredState != NO_VALUE)
				{
					item->Animation.TargetState = item->Animation.RequiredState;
					break;
				}
				else if (AI.distance < SETH_KILL_ATTACK_RANGE && AI.bite)
				{
					item->Animation.TargetState = SETH_STATE_KILL_ATTACK_START;
					break;
				}
				else if (LaraItem->Pose.Position.y >= (item->Pose.Position.y - BLOCK(1)))
				{
					if (AI.distance < SETH_DUAL_PROJECTILE_ATTACK_RANGE && AI.ahead &&
						 Targetable(item, &AI) && Random::TestProbability(SETH_DUAL_PROJECTILE_ATTACK_CHANCE))
					{
						item->Animation.TargetState = SETH_STATE_DUAL_PROJECTILE_ATTACK;
						item->ItemFlags[0] = 0;
						break;
					}
					else if (ceiling != NO_HEIGHT &&
						ceiling < (item->Pose.Position.y - BLOCK(7 / 8.0f)) &&
						height4 != NO_HEIGHT &&
						height4 > (item->Pose.Position.y - BLOCK(1)) &&
						Random::TestProbability(SETH_JUMP_PROJECTILE_ATTACK_CHANCE))
					{
						item->Pose.Position.y -= BLOCK(1.5f);
						if (Targetable(item, &AI))
						{
							item->Pose.Position.y += BLOCK(1.5f);
							item->Animation.TargetState = SETH_STATE_JUMP_PROJECTILE_ATTACK;
							item->ItemFlags[0] = 0;
						}
						else
						{
							item->Pose.Position.y += BLOCK(1.5f);
							item->Animation.TargetState = SETH_STATE_WALK_FORWARD;
						}

						break;
					}
					else
					{
						if (AI.distance < SETH_POUNCE_ATTACK_RANGE && AI.ahead &&
							AI.angle < ANGLE(33.75f) && AI.angle > ANGLE(-33.75f))
						{
							if (Targetable(item, &AI))
							{
								item->Animation.TargetState = SETH_STATE_POUNCE_ATTACK;
								break;
							}
						}
						else if (AI.distance < SETH_SINGLE_PROJECTILE_ATTACK_RANGE &&
							AI.angle < ANGLE(45.0f) && AI.angle > ANGLE(-45.0f) &&
							height4 != NO_HEIGHT &&
							height4 >= (item->Pose.Position.y - CLICK(1)) &&
							Targetable(item, &AI))
						{
							item->Animation.TargetState = SETH_STATE_SINGLE_PROJECTILE_ATTACK;
							item->ItemFlags[0] = 0;
							break;
						}
						else if (canJump)
						{
							item->Animation.TargetState = SETH_STATE_JUMP;
							break;
						}
					}
				}
				else
				{
					if (creature.ReachedGoal)
					{
						item->Animation.TargetState = SETH_STATE_HOVER;
						break;
					}
					else
					{
						item->AIBits = AMBUSH;
						creature.HurtByLara = true;
					}
				}

				item->Animation.TargetState = SETH_STATE_WALK_FORWARD;
				break;

			case SETH_STATE_WALK_FORWARD:
				creature.MaxTurn = SETH_WALK_TURN_RATE_MAX;

				if ((AI.bite && AI.distance < SETH_IDLE_RANGE) ||
					canJump || creature.ReachedGoal)
				{
					item->Animation.TargetState = SETH_STATE_IDLE;
				}
				else if (AI.distance > SETH_WALK_RANGE)
				{
					item->Animation.TargetState = SETH_STATE_RUN_FORWARD;
				}
				
				break;

			case SETH_STATE_RUN_FORWARD:
				creature.MaxTurn = SETH_RUN_TURN_RATE_MAX;

				if ((AI.bite && AI.distance < SETH_IDLE_RANGE) ||
					canJump || creature.ReachedGoal)
				{
					item->Animation.TargetState = SETH_STATE_IDLE;
				}
				else if (AI.distance < SETH_WALK_RANGE)
				{
					item->Animation.TargetState = SETH_STATE_WALK_FORWARD;
				}
				
				break;

			case SETH_STATE_POUNCE_ATTACK:
				if (canJump)
				{
					if (item->Animation.AnimNumber == SETH_ANIM_POUNCE_ATTACK_START &&
						item->Animation.FrameNumber == 0)
					{
						creature.MaxTurn = 0;
						creature.ReachedGoal = true;
					}
				}

				if (!creature.Flags)
				{
					if (item->TouchBits.TestAny())
					{
						if (item->Animation.AnimNumber == SETH_ANIM_POUNCE_ATTACK_END)
						{
							if (item->TouchBits.Test(SethPounceAttackJoints1))
							{
								DoDamage(creature.Enemy, SETH_POUNCE_ATTACK_DAMAGE);
								CreatureEffect2(item, SethBite1, 25, -1, DoBloodSplat);
								creature.Flags = 1; // Flag 1 = is attacking.
							}

							if (item->TouchBits.Test(SethPounceAttackJoints2))
							{
								DoDamage(creature.Enemy, SETH_POUNCE_ATTACK_DAMAGE);
								CreatureEffect2(item, SethBite2, 25, -1, DoBloodSplat);
								creature.Flags = 1; // Flag 1 = is attacking.
							}
						}
					}
				}

				break;

			case SETH_STATE_JUMP:
				creature.MaxTurn = 0;
				creature.ReachedGoal = false;
				creature.LOT.IsJumping = true;
				break;

			case SETH_STATE_HARD_RECOIL:
				if (item->Animation.AnimNumber == SETH_ANIM_HARD_RECOIL_START &&
					TestLastFrame(*item))
				{
					if (Random::TestProbability(SETH_HARD_RECOIL_RECOVER_CHANCE))
						item->Animation.RequiredState = SETH_STATE_HARD_RECOIL_RECOVER;
				}

				break;

			case SETH_STATE_KILL_ATTACK_START:
				creature.MaxTurn = 0;

				if (abs(AI.angle) >= ANGLE(3.0f))
				{
					if (AI.angle >= 0)
						item->Pose.Orientation.y += ANGLE(3.0f);
					else
						item->Pose.Orientation.y -= ANGLE(3.0f);
				}
				else
				{
					item->Pose.Orientation.y += AI.angle;
				}

				if (!creature.Flags)
				{
					if (item->TouchBits.TestAny())
					{
						if (item->Animation.FrameNumber > SETH_ANIM_POUNCE_ATTACK_START &&
							item->Animation.FrameNumber < SETH_ANIM_IDLE_TO_HOVER)
						{
							DoDamage(creature.Enemy, SETH_KILL_ATTACK_DAMAGE);
							CreatureEffect2(item, SethBite1, 25, -1, DoBloodSplat);
							creature.Flags = 1; // Flag 1 = is attacking.
						}
					}
				}

				if (LaraItem->HitPoints < 0)
				{
					CreatureKill(item, SETH_ANIM_KILL_ATTACK_END, LEA_SETH_DEATH, SETH_STATE_KILL_ATTACK_END, LS_DEATH);
					ItemCustomBurn(LaraItem, Vector3(0.0f, 0.8f, 0.1f), Vector3(0.0f, 0.9f, 0.8f), 6 * FPS);
					creature.MaxTurn = 0;
					return;
				}

				break;

			case SETH_STATE_DUAL_PROJECTILE_ATTACK:
			case SETH_STATE_JUMP_PROJECTILE_ATTACK:
			case SETH_STATE_SINGLE_PROJECTILE_ATTACK:
			case SETH_STATE_HOVER_PROJECTILE_ATTACK:
				creature.MaxTurn = 0;

				if (item->Animation.ActiveState == SETH_STATE_HOVER_PROJECTILE_ATTACK)
					creature.Target.y = LaraItem->Pose.Position.y;

				if (abs(AI.angle) >= ANGLE(3.0f))
				{
					if (AI.angle >= 0)
						item->Pose.Orientation.y += ANGLE(3.0f);
					else
						item->Pose.Orientation.y -= ANGLE(3.0f);
					
					SethAttack(itemNumber);
				}
				else
				{
					item->Pose.Orientation.y += AI.angle;
					SethAttack(itemNumber);
				}

				break;

			case SETH_STATE_HOVER:
				if (item->Animation.AnimNumber != SETH_ANIM_IDLE_TO_HOVER)
				{
					item->Animation.IsAirborne = false;
					creature.MaxTurn = 0;
					creature.Target.y = LaraItem->Pose.Position.y;
					creature.LOT.Fly = 16;

					if (abs(AI.angle) >= ANGLE(3.0f))
					{
						if (AI.angle >= 0)
							item->Pose.Orientation.y += ANGLE(3.0f);
						else
							item->Pose.Orientation.y -= ANGLE(3.0f);
					}
					else
					{
						item->Pose.Orientation.y += AI.angle;
					}
				}

				if (LaraItem->Pose.Position.y <= (item->Floor - CLICK(2)))
				{
					if (Targetable(item, &AI))
					{
						item->Animation.TargetState = SETH_STATE_HOVER_PROJECTILE_ATTACK;
						item->ItemFlags[0] = 0;
					}
				}
				else
				{
					item->Animation.IsAirborne = true;
					creature.LOT.Fly = 0;

					if (item->Pose.Position.y >= item->Floor)
					{
						item->Animation.IsAirborne = false;
						item->Animation.TargetState = SETH_STATE_IDLE;
						creature.ReachedGoal = false;
					}
				}

				break;

			default:
				break;
			}
		}

		if (item->HitStatus)
		{
			if ((Lara.Control.Weapon.GunType == LaraWeaponType::Shotgun || Lara.Control.Weapon.GunType == LaraWeaponType::Revolver) &&
				AI.distance < SETH_RECOIL_RANGE && !(creature.LOT.IsJumping))
			{
				if (item->Animation.ActiveState != SETH_STATE_JUMP_PROJECTILE_ATTACK)
				{
					if (item->Animation.ActiveState <= SETH_STATE_SINGLE_PROJECTILE_ATTACK)
					{
						if (abs(height4 - item->Pose.Position.y) >= BLOCK(0.5f))
							SetAnimation(*item, SETH_ANIM_SOFT_RECOIL);
						else
							SetAnimation(*item, SETH_ANIM_HARD_RECOIL_START);
					}
					else
					{
						SetAnimation(*item, SETH_ANIM_HOVER_RECOIL);
					}
				}
			}
		}

		CreatureAnimation(itemNumber, angle, 0);
	}

	void SethAttack(int itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		item->ItemFlags[0]++;

		auto pos1 = GetJointPosition(item, SethAttack1);
		auto pos2 = GetJointPosition(item, SethAttack2);

		int sparkR = 64;
		int sparkG = Random::GenerateInt(64, 192);
		int sparkB = sparkG;
		auto sparkColor = Vector3(sparkR, sparkG, sparkB);

		int flameR = 0;
		int flameG = Random::GenerateInt(64, 192);
		int flameB = flameG - 32;
		auto flameColor = Vector3(flameR, flameG, flameB);

		int scale = 0;

		switch (item->Animation.ActiveState)
		{
		case SETH_STATE_DUAL_PROJECTILE_ATTACK:
		case SETH_STATE_HOVER_PROJECTILE_ATTACK:
			if (item->ItemFlags[0] < 78 && Random::GenerateInt(0, 32) < item->ItemFlags[0])
			{
				// Spawn spark particles.
				for (int i = 0; i < 2; i++)
				{
					TriggerAttackSpark(pos1.ToVector3(), sparkColor);
					TriggerAttackSpark(pos2.ToVector3(), sparkColor);
				}
			}

			scale = item->ItemFlags[0] * 2;
			if (scale > 128)
				scale = 128;

			if ((Wibble & 0xF) == 8)
			{
				if (item->ItemFlags[0] < 127)
				{
					TriggerAttackFlame(pos1, flameColor, scale);
					TriggerAttackFlame(pos2, flameColor, scale);
				}
			}
			else if (!(Wibble & 0xF) && item->ItemFlags[0] < 103)
			{
				TriggerAttackFlame(pos1, flameColor, scale);
				TriggerAttackFlame(pos2, flameColor, scale);
			}

			if (item->ItemFlags[0] >= 96 && item->ItemFlags[0] <= 99)
			{
				auto pos = GetJointPosition(item, SethAttack1.BoneID, Vector3i(SethAttack1.Position.x, SethAttack1.Position.y * 2, SethAttack1.Position.z));
				auto orient = Geometry::GetOrientToPoint(pos1.ToVector3(), pos.ToVector3());
				SethProjectileAttack(Pose(pos1, orient), item->RoomNumber, 0);
			}
			else if (item->ItemFlags[0] >= 122 && item->ItemFlags[0] <= 125)
			{
				auto pos = GetJointPosition(item, SethAttack2.BoneID, Vector3i(SethAttack2.Position.x, SethAttack2.Position.y * 2, SethAttack2.Position.z));
				auto orient = Geometry::GetOrientToPoint(pos2.ToVector3(), pos.ToVector3());
				SethProjectileAttack(Pose(pos2, orient), item->RoomNumber, 0);
			}

			break;

		case SETH_STATE_JUMP_PROJECTILE_ATTACK:
			scale = item->ItemFlags[0] * 4;
			if (scale > 160)
				scale = 160;

			if ((Wibble & 0xF) == 8)
			{
				if (item->ItemFlags[0] < 132)
				{
					TriggerAttackFlame(pos1, flameColor, scale);
					TriggerAttackFlame(pos2, flameColor, scale);
				}
			}
			else if (!(Wibble & 0xF) && item->ItemFlags[0] < 132)
			{
				TriggerAttackFlame(pos1, flameColor, scale);
				TriggerAttackFlame(pos2, flameColor, scale);
			}

			if (item->ItemFlags[0] >= 60 && item->ItemFlags[0] <= 74 ||
				item->ItemFlags[0] >= 112 && item->ItemFlags[0] <= 124)
			{
				if (Wibble & 4)
				{
					auto pos = GetJointPosition(item, SethAttack1.BoneID, Vector3i(SethAttack1.Position.x, SethAttack1.Position.y * 2, SethAttack1.Position.z));
					auto orient = Geometry::GetOrientToPoint(pos1.ToVector3(), pos.ToVector3());
					SethProjectileAttack(Pose(pos1, orient), item->RoomNumber, 0);

					pos = GetJointPosition(item, SethAttack2.BoneID, Vector3i(SethAttack2.Position.x, SethAttack2.Position.y * 2, SethAttack2.Position.z));
					orient = Geometry::GetOrientToPoint(pos2.ToVector3(), pos.ToVector3());
					SethProjectileAttack(Pose(pos2, orient), item->RoomNumber, 0);
				}
			}

			break;

		case SETH_STATE_SINGLE_PROJECTILE_ATTACK:
			if (item->ItemFlags[0] > 40 &&
				item->ItemFlags[0] < 100 &&
				Random::GenerateInt(0, 8) < (item->ItemFlags[0] - 40))
			{
				// Spawn spark particles.
				for (int i = 0; i < 2; i++)
				{
					TriggerAttackSpark(pos1.ToVector3(), sparkColor);
					TriggerAttackSpark(pos2.ToVector3(), sparkColor);
				}
			}

			scale = item->ItemFlags[0] * 2;
			if (scale > 128)
				scale = 128;

			if ((Wibble & 0xF) == 8)
			{
				if (item->ItemFlags[0] < 103)
				{
					TriggerAttackFlame(pos1, flameColor, scale);
					TriggerAttackFlame(pos2, flameColor, scale);
				}
			}
			else if (!(Wibble & 0xF) && item->ItemFlags[0] < 103)
			{
				TriggerAttackFlame(pos1, flameColor, scale);
				TriggerAttackFlame(pos2, flameColor, scale);
			}

			if (item->ItemFlags[0] == 102)
			{
				auto pos = GetJointPosition(item, SethAttack1.BoneID, Vector3i(SethAttack2.Position.x, SethAttack2.Position.y * 2, SethAttack2.Position.z));
				auto orient = Geometry::GetOrientToPoint(pos1.ToVector3(), pos.ToVector3());
				SethProjectileAttack(Pose(pos1, orient), item->RoomNumber, 1);
			}

			break;
		}
	}

	void SethProjectileAttack(const Pose& pose, int roomNumber, int flags)
	{
		short fxNumber = CreateNewEffect(roomNumber);
		if (fxNumber == -1)
			return;

		auto& fx = EffectList[fxNumber];

		fx.pos.Position.x = pose.Position.x;
		fx.pos.Position.y = pose.Position.y - Random::GenerateInt(-32, 32);
		fx.pos.Position.z = pose.Position.z;

		fx.pos.Orientation.x = pose.Orientation.x;
		fx.pos.Orientation.y = pose.Orientation.y;
		fx.pos.Orientation.z = 0;
		fx.roomNumber = roomNumber;
		fx.counter = (GetRandomControl() * 2) - ANGLE(180.0f); // TODO: This isn't an angle. Run tests on what it actually does.
		fx.flag1 = flags;
		fx.objectNumber = ID_ENERGY_BUBBLES;
		fx.speed = Random::GenerateInt(0, 32) - ((flags == 1) ? 64 : 0) + 96;
		fx.frameNumber = Objects[ID_ENERGY_BUBBLES].meshIndex + flags;
	}
}
