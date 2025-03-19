#include "framework.h"
#include "Objects/TR3/Entity/tr3_mp_stick.h"

#include "Game/control/box.h"
#include "Game/control/lot.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/itemdata/creature_info.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR3
{
	const auto MPStickBite1 = CreatureBiteInfo(Vector3(247, 10, 11), 13);
	const auto MPStickBite2 = CreatureBiteInfo(Vector3(0, 0, 100), 6);
	const auto MPStickPunchAttackJoints = std::vector<unsigned int>{ 10, 13 };
	const auto MPStickKickAttackJoints  = std::vector<unsigned int>{ 5, 6 };

	constexpr auto MPSTICK_VAULT_SHIFT = 260;

	enum MPStickState
	{
		// No state 0.
		MPSTICK_STATE_STOP = 1,
		MPSTICK_STATE_WALK = 2,
		MPSTICK_STATE_PUNCH2 = 3,
		MPSTICK_STATE_AIM2 = 4,
		MPSTICK_STATE_WAIT = 5,
		MPSTICK_STATE_AIM1 = 6,
		MPSTICK_STATE_AIM0 = 7,
		MPSTICK_STATE_PUNCH1 = 8,
		MPSTICK_STATE_PUNCH0 = 9,
		MPSTICK_STATE_RUN = 10,
		MPSTICK_STATE_DEATH = 11,
		MPSTICK_STATE_KICK = 12,
		MPSTICK_STATE_CLIMB3 = 13,
		MPSTICK_STATE_CLIMB1 = 14,
		MPSTICK_STATE_CLIMB2 = 15,
		MPSTICK_STATE_FALL3 = 16
	};

	enum MPStickAnim
	{
		MPSTICK_ANIM_IDLE = 6,
		MPSTICK_ANIM_DEATH = 26,
		MPSTICK_ANIM_VAULT_3_STEPS_UP = 27,
		MPSTICK_ANIM_VAULT_1_STEPS_UP = 28,
		MPSTICK_ANIM_VAULT_2_STEPS_UP = 29,
		MPSTICK_ANIM_VAULT_4_STEPS_DOWN = 30
	};

	void InitializeMPStick(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(*item, MPSTICK_ANIM_IDLE);
	}

	void MPStickControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short tilt = 0;
		short head = 0;
		auto extraTorsoRot = EulerAngles::Identity;

		if (item->BoxNumber != NO_VALUE && (g_Level.PathfindingBoxes[item->BoxNumber].flags & BLOCKED))
		{
			DoDamage(item, 20);
			DoLotsOfBlood(item->Pose.Position.x, item->Pose.Position.y - (GetRandomControl() & 255) - 32, item->Pose.Position.z, (GetRandomControl() & 127) + 128, GetRandomControl() * 2, item->RoomNumber, 3);
		}

		AI_INFO laraAI;
		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != MPSTICK_STATE_DEATH)
			{
				SetAnimation(*item, 26);
				creature->LOT.Step = 256;
			}
		}
		else
		{
			if (item->AIBits)
				GetAITarget(creature);
			else
			{
				creature->Enemy = LaraItem;

				int dx = LaraItem->Pose.Position.x - item->Pose.Position.x;
				int dz = LaraItem->Pose.Position.z - item->Pose.Position.z;
				laraAI.distance = pow(dx, 2) + pow(dx, 2);

				int bestDistance = INT_MAX;
				for (auto& currentCreature : ActiveCreatures)
				{
					if (currentCreature->ItemNumber == NO_VALUE || currentCreature->ItemNumber == itemNumber)
						continue;

					auto* target = &g_Level.Items[currentCreature->ItemNumber];
					if (target->ObjectNumber != ID_LARA)
						continue;

					dx = target->Pose.Position.x - item->Pose.Position.x;
					dz = target->Pose.Position.z - item->Pose.Position.z;

					if (dz > BLOCK(31.25f) || dz < -BLOCK(31.25f) ||
						dx > BLOCK(31.25f) || dx < -BLOCK(31.25f))
					{
						continue;
					}

					int distance = pow(dx, 2) + pow(dz, 2);
					if (distance < bestDistance && distance < laraAI.distance)
					{
						bestDistance = distance;
						creature->Enemy = target;
					}
				}
			}

			AI_INFO AI;
			CreatureAIInfo(item, &AI);

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
				laraAI.distance = SQUARE(dx) + SQUARE(dz);
			}

			GetCreatureMood(item, &AI, true);
			CreatureMood(item, &AI, true);

			angle = CreatureTurn(item, creature->MaxTurn);

			auto* enemy = creature->Enemy;
			creature->Enemy = LaraItem;

			if (item->HitStatus || ((laraAI.distance < pow(BLOCK(1), 2) ||
				TargetVisible(item, &laraAI)) && abs(LaraItem->Pose.Position.y - item->Pose.Position.y) < BLOCK(1)))
			{
				if (!creature->Alerted)
					SoundEffect(SFX_TR3_AMERCAN_HOY, &item->Pose);

				AlertAllGuards(itemNumber);
			}

			creature->Enemy = enemy;

			switch (item->Animation.ActiveState)
			{
			case MPSTICK_STATE_WAIT:
				if (creature->Alerted || item->Animation.TargetState == MPSTICK_STATE_RUN)
				{
					item->Animation.TargetState = MPSTICK_STATE_STOP;
					break;
				}

			case MPSTICK_STATE_STOP:
				creature->MaxTurn = 0;
				creature->Flags = 0;
				head = laraAI.angle;

				if (item->AIBits & GUARD)
				{
					head = AIGuard(creature);
					if (Random::TestProbability(1 / 256.0f))
					{
						if (item->Animation.ActiveState == MPSTICK_STATE_STOP)
							item->Animation.TargetState = MPSTICK_STATE_WAIT;
						else
							item->Animation.TargetState = MPSTICK_STATE_STOP;
					}

					break;
				}
				else if (item->AIBits & PATROL1)
					item->Animation.TargetState = MPSTICK_STATE_WALK;

				else if (creature->Mood == MoodType::Escape)
				{
					if (Lara.TargetEntity != item && AI.ahead && !item->HitStatus)
						item->Animation.TargetState = MPSTICK_STATE_STOP;
					else
						item->Animation.TargetState = MPSTICK_STATE_RUN;
				}
				else if (creature->Mood == MoodType::Bored ||
					(item->AIBits & FOLLOW && (creature->ReachedGoal || laraAI.distance > pow(BLOCK(2), 2))))
				{
					if (item->Animation.RequiredState != NO_VALUE)
						item->Animation.TargetState = item->Animation.RequiredState;
					else if (AI.ahead)
						item->Animation.TargetState = MPSTICK_STATE_STOP;
					else
						item->Animation.TargetState = MPSTICK_STATE_RUN;
				}
				else if (AI.bite && AI.distance < pow(BLOCK(0.5f), 2))
					item->Animation.TargetState = MPSTICK_STATE_AIM0;
				else if (AI.bite && AI.distance < pow(BLOCK(1), 2))
					item->Animation.TargetState = MPSTICK_STATE_AIM1;
				else if (AI.bite && AI.distance < pow(BLOCK(1), 2))
					item->Animation.TargetState = MPSTICK_STATE_WALK;
				else
					item->Animation.TargetState = MPSTICK_STATE_RUN;

				break;

			case MPSTICK_STATE_WALK:
				creature->MaxTurn = ANGLE(6.0f);
				creature->Flags = 0;
				head = laraAI.angle;

				if (item->AIBits & PATROL1)
				{
					item->Animation.TargetState = MPSTICK_STATE_WALK;
					head = 0;
				}
				else if (creature->Mood == MoodType::Escape)
					item->Animation.TargetState = MPSTICK_STATE_RUN;
				else if (creature->Mood == MoodType::Bored)
				{
					if (Random::TestProbability(1 / 128.0f))
					{
						item->Animation.RequiredState = MPSTICK_STATE_WAIT;
						item->Animation.TargetState = MPSTICK_STATE_STOP;
					}
				}
				else if (AI.bite && AI.distance < pow(BLOCK(1.5f), 2) && AI.xAngle < 0)
					item->Animation.TargetState = MPSTICK_STATE_KICK;
				else if (AI.bite && AI.distance < pow(BLOCK(0.5f), 2))
					item->Animation.TargetState = MPSTICK_STATE_STOP;
				else if (AI.bite && AI.distance < pow(BLOCK(1.25f), 2))
					item->Animation.TargetState = MPSTICK_STATE_AIM2;
				else
					item->Animation.TargetState = MPSTICK_STATE_RUN;

				break;

			case MPSTICK_STATE_RUN:
				creature->MaxTurn = ANGLE(7.0f);
				tilt = angle / 2;

				if (AI.ahead)
					head = AI.angle;

				if (item->AIBits & GUARD)
					item->Animation.TargetState = MPSTICK_STATE_WAIT;
				else if (creature->Mood == MoodType::Escape)
				{
					if (Lara.TargetEntity != item && AI.ahead)
						item->Animation.TargetState = MPSTICK_STATE_STOP;

					break;
				}
				else if (item->AIBits & FOLLOW && (creature->ReachedGoal || laraAI.distance > pow(BLOCK(2), 2)))
					item->Animation.TargetState = MPSTICK_STATE_STOP;
				else if (creature->Mood == MoodType::Bored)
					item->Animation.TargetState = MPSTICK_STATE_WALK;
				else if (AI.ahead && AI.distance < pow(BLOCK(1), 2))
					item->Animation.TargetState = MPSTICK_STATE_WALK;

				break;

			case MPSTICK_STATE_AIM0:
				creature->MaxTurn = ANGLE(6.0f);
				creature->Flags = 0;

				if (AI.ahead)
				{
					extraTorsoRot.x = AI.xAngle;
					extraTorsoRot.y = AI.angle;
				}

				if (AI.bite && AI.distance < pow(BLOCK(0.5f), 2))
					item->Animation.TargetState = MPSTICK_STATE_PUNCH0;
				else
					item->Animation.TargetState = MPSTICK_STATE_STOP;

				break;

			case MPSTICK_STATE_AIM1:
				creature->MaxTurn = ANGLE(6.0f);
				creature->Flags = 0;

				if (AI.ahead)
				{
					extraTorsoRot.x = AI.xAngle;
					extraTorsoRot.y = AI.angle;
				}

				if (AI.ahead && AI.distance < pow(BLOCK(1), 2))
					item->Animation.TargetState = MPSTICK_STATE_PUNCH1;
				else
					item->Animation.TargetState = MPSTICK_STATE_STOP;

				break;

			case MPSTICK_STATE_AIM2:
				creature->MaxTurn = ANGLE(6.0f);
				creature->Flags = 0;

				if (AI.ahead)
				{
					extraTorsoRot.x = AI.xAngle;
					extraTorsoRot.y = AI.angle;
				}

				if (AI.bite && AI.distance < pow(BLOCK(1.25f), 2))
					item->Animation.TargetState = MPSTICK_STATE_PUNCH2;
				else
					item->Animation.TargetState = MPSTICK_STATE_WALK;

				break;

			case MPSTICK_STATE_PUNCH0:
				creature->MaxTurn = ANGLE(6.0f);

				if (AI.ahead)
				{
					extraTorsoRot.x = AI.xAngle;
					extraTorsoRot.y = AI.angle;
				}

				if (creature->Enemy->IsLara())
				{
					if (!creature->Flags && item->TouchBits.Test(MPStickPunchAttackJoints))
					{
						DoDamage(enemy, 80);
						CreatureEffect(item, MPStickBite1, DoBloodSplat);
						SoundEffect(SFX_TR4_LARA_THUD, &item->Pose);
						creature->Flags = 1;
					}
				}
				else
				{
					if (!creature->Flags && enemy != nullptr)
					{
						if (Vector3i::Distance(item->Pose.Position, creature->Enemy->Pose.Position) <= BLOCK(0.25f))
						{
							DoDamage(enemy, 5);
							CreatureEffect(item, MPStickBite1, DoBloodSplat);
							SoundEffect(SFX_TR4_LARA_THUD, &item->Pose);
							creature->Flags = 1;
						}
					}
				}

				break;

			case MPSTICK_STATE_PUNCH1:
				creature->MaxTurn = ANGLE(6.0f);

				if (AI.ahead)
				{
					extraTorsoRot.x = AI.xAngle;
					extraTorsoRot.y = AI.angle;
				}

				if (creature->Enemy->IsLara())
				{
					if (!creature->Flags && item->TouchBits.Test(MPStickPunchAttackJoints))
					{
						DoDamage(creature->Enemy, 80);
						CreatureEffect(item, MPStickBite1, DoBloodSplat);
						SoundEffect(SFX_TR4_LARA_THUD, &item->Pose);
						creature->Flags = 1;
					}
				}
				else
				{
					if (!creature->Flags && creature->Enemy != nullptr)
					{
						if (Vector3i::Distance(item->Pose.Position, creature->Enemy->Pose.Position) <= BLOCK(0.25f))
						{
							DoDamage(creature->Enemy, 5);
							CreatureEffect(item, MPStickBite1, DoBloodSplat);
							SoundEffect(SFX_TR4_LARA_THUD, &item->Pose);
							creature->Flags = 1;
						}
					}
				}

				if (AI.ahead && AI.distance > pow(BLOCK(1), 2) && AI.distance < pow(BLOCK(1.25f), 2))
					item->Animation.TargetState = MPSTICK_STATE_PUNCH2;

				break;

			case MPSTICK_STATE_PUNCH2:
				creature->MaxTurn = ANGLE(6.0f);

				if (AI.ahead)
				{
					extraTorsoRot.x = AI.xAngle;
					extraTorsoRot.y = AI.angle;
				}

				if (creature->Enemy->IsLara())
				{
					if (creature->Flags != 2 && item->TouchBits.Test(MPStickPunchAttackJoints))
					{
						DoDamage(creature->Enemy, 100);
						CreatureEffect(item, MPStickBite1, DoBloodSplat);
						SoundEffect(70, &item->Pose);
						creature->Flags = 2;
					}
				}
				else
				{
					if (creature->Flags != 2 && creature->Enemy)
					{
						if (Vector3i::Distance(item->Pose.Position, creature->Enemy->Pose.Position) <= BLOCK(0.25f))
						{
							DoDamage(creature->Enemy, 6);
							CreatureEffect(item, MPStickBite1, DoBloodSplat);
							SoundEffect(SFX_TR4_LARA_THUD, &item->Pose);
							creature->Flags = 2;
						}
					}
				}

				break;

			case MPSTICK_STATE_KICK:
				creature->MaxTurn = ANGLE(6.0f);

				if (AI.ahead)
					extraTorsoRot.y = AI.angle;

				if (creature->Enemy->IsLara())
				{
					if (creature->Flags != 1 && item->TouchBits.Test(MPStickKickAttackJoints) &&
						item->Animation.FrameNumber > 8)
					{
						DoDamage(creature->Enemy, 150);
						CreatureEffect(item, MPStickBite2, DoBloodSplat);
						SoundEffect(SFX_TR4_LARA_THUD, &item->Pose);
						creature->Flags = 1;
					}
				}
				else
				{
					if (!creature->Flags != 1 && creature->Enemy &&
						item->Animation.FrameNumber > 8)
					{
						if (Vector3i::Distance(item->Pose.Position, creature->Enemy->Pose.Position) <= BLOCK(0.25f))
						{
							DoDamage(creature->Enemy, 9);
							CreatureEffect(item, MPStickBite2, DoBloodSplat);
							SoundEffect(SFX_TR4_LARA_THUD, &item->Pose);
							creature->Flags = 1;
						}
					}
				}

				break;
			}
		}

		CreatureTilt(item, tilt);
		CreatureJoint(item, 0, extraTorsoRot.y);
		CreatureJoint(item, 1, extraTorsoRot.x);
		CreatureJoint(item, 2, head);

		if (item->Animation.ActiveState < MPSTICK_STATE_DEATH)
		{
			switch (CreatureVault(itemNumber, angle, 2, MPSTICK_VAULT_SHIFT))
			{
			case 2:
				creature->MaxTurn = 0;
				SetAnimation(*item, MPSTICK_ANIM_VAULT_1_STEPS_UP);
				break;

			case 3:
				creature->MaxTurn = 0;
				SetAnimation(*item, MPSTICK_ANIM_VAULT_2_STEPS_UP);
				break;

			case 4:
				creature->MaxTurn = 0;
				SetAnimation(*item, MPSTICK_ANIM_VAULT_3_STEPS_UP);
				break;

			case -4:
				creature->MaxTurn = 0;
				SetAnimation(*item, MPSTICK_ANIM_VAULT_4_STEPS_DOWN);
				break;
			}
		}
		else
		{
			creature->MaxTurn = 0;
			CreatureAnimation(itemNumber, angle, tilt);
		}
	}
}
