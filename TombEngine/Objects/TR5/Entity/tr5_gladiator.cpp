#include "framework.h"
#include "Objects/TR5/Entity/tr5_gladiator.h"

#include "Game/Animation/Animation.h"
#include "Game/control/box.h"
#include "Game/effects/debris.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Room;
using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR5
{
	constexpr auto GLADIATOR_ATTACK_DAMAGE = 120;

	// TODO: Ranges.

	const auto GladiatorBite = CreatureBiteInfo(Vector3::Zero, 16);
	const auto GladiatorAttackJoints = std::vector<unsigned int>{ 13, 14 };

	enum GladiatorState
	{
		// No state 0.
		GLADIATOR_STATE_IDLE = 1,
		GLADIATOR_STATE_WALK_FORWARD = 2,
		GLADIATOR_STATE_RUN_FORWARD = 3,
		GLADIATOR_STATE_GUARD_START = 4,
		GLADIATOR_STATE_GUARD_CONTINUE = 5,
		GLADIATOR_STATE_DEATH = 6,
		GLADIATOR_STATE_SWORD_ATTACK_1 = 8,
		GLADIATOR_STATE_SWORD_ATTACK_2 = 9,
		GLADIATOR_STATE_RUN_SWORD_ATTACK = 10,
		GLADIATOR_STATE_WALK_SWORD_ATTACK = 11
	};

	enum GladiatorAnim
	{
		GLADIATOR_ANIM_IDLE = 0,
		GLADIATOR_ANIM_IDLE_TO_WALK_FORWARD = 1,
		GLADIATOR_ANIM_WAK_FORWARD = 2,
		GLADIATOR_ANIM_WALK_FORWARD_TO_RUN_FORWARD = 3,
		GLADIATOR_ANIM_RUN_FORWARD = 4,
		GLADIATOR_ANIM_IDLE_TO_RUN_FORWARD = 5,
		GLADIATOR_ANIM_WALK_FORWARD_TO_IDLE_LEFT = 6,
		GLADIATOR_ANIM_WALK_FORWARD_TO_IDLE_RIGHT = 7,
		GLADIATOR_ANIM_RUN_FORWARD_TO_WALK_FORWARD_LEFT = 8,
		GLADIATOR_ANIM_RUN_FORWARD_TO_WALK_FORWARD_RIGHT = 9,
		GLADIATOR_ANIM_RUN_FORWARD_TO_IDLE_LEFT = 10,
		GLADIATOR_ANIM_RUN_FORWARD_TO_IDLE_RIGHT = 11,
		GLADIATOR_ANIM_GUARD_START = 12,
		GLADIATOR_ANIM_GUARD_START_QUICK = 12, // Unused.
		GLADIATOR_ANIM_GUARD_CONTINUE = 14,
		GLADIATOR_ANIM_GUAD_END = 15,
		GLADIATOR_ANIM_DEATH = 16,
		GLADIATOR_ANIM_SWORD_ATTACK_1 = 17,
		GLADIATOR_ANIM_SWORD_ATTACK_2_START = 18,
		GLADIATOR_ANIM_SWORD_ATTACK_2_END = 19,
		GLADIATOR_ANIM_SWORD_ATTACK_2_END_EARLY = 20, // Unused.
		GLADIATOR_ANIM_RUN_SWORD_ATTACK = 21,
		GLADIATOR_ANIM_WALK_SWORD_ATTACK = 22
	};

	void InitializeGladiator(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(*item, GLADIATOR_ANIM_IDLE);

		if (item->TriggerFlags == 1)
			item->SetMeshSwapFlags(ALL_JOINT_BITS);
	}

	void ControlGladiator(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short tilt = 0;
		short joint0 = 0;
		short joint1 = 0;
		short joint2 = 0;

		if (item->HitPoints <= 0)
		{
			item->HitPoints = 0;

			if (item->Animation.ActiveState != GLADIATOR_STATE_DEATH)
				SetAnimation(*item, GLADIATOR_ANIM_DEATH);
		}
		else
		{
			if (item->AIBits)
				GetAITarget(creature);
			else if (creature->HurtByLara)
				creature->Enemy = LaraItem;

			AI_INFO AI;
			CreatureAIInfo(item, &AI);

			int unknown = true;
			short deltaAngle;
			int distance;

			if (creature->Enemy == LaraItem)
			{
				distance = AI.distance;
				deltaAngle = AI.angle;
			}
			else
			{
				int dx = LaraItem->Pose.Position.x - item->Pose.Position.x;
				int dz = LaraItem->Pose.Position.z - item->Pose.Position.z;

				deltaAngle = phd_atan(dz, dx) - item->Pose.Orientation.y;
				if (deltaAngle <= -ANGLE(90.0f) || deltaAngle >= ANGLE(90.0f))
					unknown = false;

				distance = pow(dx, 2) + pow(dz, 2);
			}

			GetCreatureMood(item, &AI, true);
			CreatureMood(item, &AI, true);

			angle = CreatureTurn(item, creature->MaxTurn);

			if (AI.ahead)
			{
				joint0 = AI.angle / 2;
				joint2 = AI.angle / 2;
				joint1 = AI.xAngle;
			}

			switch (item->Animation.ActiveState)
			{
			case GLADIATOR_STATE_IDLE:
				joint2 = deltaAngle;
				creature->MaxTurn = (-(int)(creature->Mood != MoodType::Bored)) & 0x16C;
				creature->Flags = 0;

				if (item->AIBits & GUARD ||
					Random::TestProbability(1.0f / 32) &&
					(AI.distance > pow(BLOCK(1), 2) || creature->Mood != MoodType::Attack))
				{
					joint2 = AIGuard(creature);
					break;
				}

				if (item->AIBits & PATROL1)
					item->Animation.TargetState = GLADIATOR_STATE_WALK_FORWARD;
				else
				{
					if (creature->Mood == MoodType::Escape)
					{
						if (Lara.TargetEntity != item && AI.ahead && !item->HitStatus)
						{
							item->Animation.TargetState = GLADIATOR_STATE_IDLE;
							break;
						}
					}
					else
					{
						if (creature->Mood == MoodType::Bored ||
							(item->AIBits & FOLLOW && (creature->ReachedGoal || distance > pow(BLOCK(2), 2))))
						{
							if (item->Animation.RequiredState != NO_VALUE)
								item->Animation.TargetState = item->Animation.RequiredState;
							else if (Random::TestProbability(1 / 64.0f))
								item->Animation.TargetState = GLADIATOR_STATE_IDLE;

							break;
						}

						if (Lara.TargetEntity == item &&
							unknown && distance < pow(BLOCK(1.5f), 2) &&
							Random::TestProbability(1 / 2.0f) &&
							(Lara.Control.Weapon.GunType == LaraWeaponType::Shotgun || Random::TestProbability(0.06f)) &&
							item->MeshBits == -1)
						{
							item->Animation.TargetState = GLADIATOR_STATE_GUARD_START;
							break;
						}

						if (AI.bite && AI.distance < pow(819, 2))
						{
							if (Random::TestProbability(1 / 2.0f))
								item->Animation.TargetState = GLADIATOR_STATE_SWORD_ATTACK_1;
							else
								item->Animation.TargetState = GLADIATOR_STATE_SWORD_ATTACK_2;

							break;
						}
					}

					item->Animation.TargetState = GLADIATOR_STATE_WALK_FORWARD;
				}

				break;

			case GLADIATOR_STATE_WALK_FORWARD:
				joint2 = deltaAngle;
				creature->MaxTurn = creature->Mood != MoodType::Bored ? ANGLE(7.0f) : ANGLE(2.0f);
				creature->Flags = 0;

				if (item->AIBits & PATROL1)
				{
					joint2 = 0;
					item->Animation.TargetState = GLADIATOR_STATE_WALK_FORWARD;
				}
				else if (creature->Mood == MoodType::Escape)
					item->Animation.TargetState = GLADIATOR_STATE_RUN_FORWARD;
				else if (creature->Mood != MoodType::Bored)
				{
					if (AI.distance < pow(BLOCK(1), 2))
					{
						item->Animation.TargetState = GLADIATOR_STATE_IDLE;
						break;
					}

					if (AI.bite && AI.distance < pow(BLOCK(2), 2))
						item->Animation.TargetState = GLADIATOR_STATE_WALK_SWORD_ATTACK;
					else if (!AI.ahead || AI.distance > pow(BLOCK(1.5f), 2))
						item->Animation.TargetState = GLADIATOR_STATE_RUN_FORWARD;
				}
				else if (Random::TestProbability(1 / 64.0f))
				{
					item->Animation.TargetState = GLADIATOR_STATE_IDLE;
					break;
				}

				break;

			case GLADIATOR_STATE_RUN_FORWARD:
				creature->MaxTurn = ANGLE(11.0f);
				creature->LOT.IsJumping = false;
				tilt = angle / 2;

				if (AI.ahead)
					joint2 = AI.angle;

				if (item->AIBits & GUARD)
				{
					item->Animation.TargetState = GLADIATOR_STATE_IDLE;
					creature->MaxTurn = 0;
					break;
				}

				if (creature->Mood == MoodType::Escape)
				{
					if (Lara.TargetEntity != item && AI.ahead)
					{
						item->Animation.TargetState = GLADIATOR_STATE_IDLE;
						break;
					}

					break;
				}

				if (item->AIBits & FOLLOW &&
					(creature->ReachedGoal || distance > pow(BLOCK(2), 2)))
				{
					item->Animation.TargetState = GLADIATOR_STATE_IDLE;
					break;
				}

				if (creature->Mood == MoodType::Bored)
				{
					item->Animation.TargetState = GLADIATOR_STATE_WALK_FORWARD;
					break;
				}

				if (AI.distance < pow(BLOCK(1.5f), 2))
				{
					if (AI.bite)
						item->Animation.TargetState = GLADIATOR_STATE_RUN_SWORD_ATTACK;
					else
						item->Animation.TargetState = GLADIATOR_STATE_WALK_FORWARD;
				}

				break;

			case GLADIATOR_STATE_GUARD_START:
				if (item->HitStatus)
				{
					if (!unknown)
					{
						item->Animation.TargetState = GLADIATOR_STATE_IDLE;
						break;
					}
				}
				else if (Lara.TargetEntity != item || Random::TestProbability(1 / 128.0f))
				{
					item->Animation.TargetState = GLADIATOR_STATE_IDLE;
					break;
				}

				break;

			case 5:
				if (Lara.TargetEntity != item)
					item->Animation.TargetState = GLADIATOR_STATE_IDLE;

				break;

			case GLADIATOR_STATE_SWORD_ATTACK_1:
			case GLADIATOR_STATE_SWORD_ATTACK_2:
			case GLADIATOR_STATE_RUN_SWORD_ATTACK:
			case GLADIATOR_STATE_WALK_SWORD_ATTACK:
				creature->MaxTurn = 0;

				if (abs(AI.angle) >= ANGLE(7.0f))
				{
					if (AI.angle >= 0)
						item->Pose.Orientation.y += ANGLE(7.0f);
					else
						item->Pose.Orientation.y -= ANGLE(7.0f);
				}
				else
				{
					item->Pose.Orientation.y += AI.angle;
				}

				if (item->Animation.FrameNumber > 10)
				{
					auto* room = &g_Level.Rooms[item->RoomNumber];

					auto pos = GetJointPosition(item, 16);

					auto* floor = GetSector(room, pos.x - room->Position.x, pos.z - room->Position.z);
					if (floor->Stopper)
					{
						for (int i = 0; i < room->mesh.size(); i++)
						{
							auto* mesh = &room->mesh[i];

							if (!((pos.z ^ mesh->pos.Position.z) & 0xFFFFFC00))
							{
								if (!((pos.x ^ mesh->pos.Position.x) & 0xFFFFFC00))
								{
									if (Statics[mesh->staticNumber].shatterType != ShatterType::None)
									{
										ShatterObject(0, mesh, -64, LaraItem->RoomNumber, 0);
										//SoundEffect(ShatterSounds[gfCurrentLevel - 5][*(v28 + 18)], v28);

										TestTriggers(pos.x, pos.y, pos.z, item->RoomNumber, true);
									}
								}
							}
						}
					}

					if (!creature->Flags)
					{
						if (item->TouchBits.Test(GladiatorAttackJoints))
						{
							DoDamage(creature->Enemy, GLADIATOR_ATTACK_DAMAGE);
							CreatureEffect2(item, GladiatorBite, 10, item->Pose.Orientation.y, DoBloodSplat);
							SoundEffect(SFX_TR4_LARA_THUD, &item->Pose);
							creature->Flags = 1;
						}
					}
				}

				break;

			default:
				break;
			}
		}

		CreatureTilt(item, tilt);
		CreatureJoint(item, 0, joint0);
		CreatureJoint(item, 1, joint1);
		CreatureJoint(item, 2, joint2);
		CreatureAnimation(itemNumber, angle, 0);
	}
}
