#include "framework.h"
#include "Objects/TR3/Entity/WhiteSoldier.h"

#include "Game/Animation/Animation.h"
#include "Game/control/box.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
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
	constexpr auto WHITE_SOLDIER_SHOT_DAMAGE = 28;

	constexpr auto WHITE_SOLDIER_RUN_RANGE		= SQUARE(BLOCK(2));
	constexpr auto WHITE_SOLDIER_SHOOT1_RANGE	= SQUARE(BLOCK(3));
	constexpr auto WHITE_SOLDIER_AWARE_DISTANCE = SQUARE(BLOCK(1));

	constexpr auto WHITE_SOLDIER_WALK_TURN_RATE_MAX = ANGLE(5.0f);
	constexpr auto WHITE_SOLDIER_RUN_TURN_RATE_MAX  = ANGLE(10.0f);

	constexpr auto WHITE_SOLDIER_DIE_ANIM		= 19;
	constexpr auto WHITE_SOLDIER_STOP_ANIM		= 12;
	constexpr auto WHITE_SOLDIER_WALK_STOP_ANIM = 17;

	const auto WhiteSoldierGunBite = CreatureBiteInfo(Vector3(0, 400, 64), 7);

	enum WhiteSoldierState
	{
		WHITE_SOLDIER_STATE_EMPTY = 0,
		WHITE_SOLDIER_STATE_STOP = 1,
		WHITE_SOLDIER_STATE_WALK = 2,
		WHITE_SOLDIER_STATE_RUN = 3,
		WHITE_SOLDIER_STATE_WAIT = 4,
		WHITE_SOLDIER_STATE_SHOOT1 = 5,
		WHITE_SOLDIER_STATE_SHOOT2 = 6,
		WHITE_SOLDIER_STATE_DEATH = 7,
		WHITE_SOLDIER_STATE_AIM1 = 8,
		WHITE_SOLDIER_STATE_AIM2 = 9,
		WHITE_SOLDIER_STATE_AIM3 = 10,
		WHITE_SOLDIER_STATE_SHOOT3 = 11
	};

	void InitializeWhiteSoldier(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(item, WHITE_SOLDIER_STOP_ANIM);
	}

	void ControlWhiteSoldier(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short tilt = 0;
		short head = 0;
		auto extraTorsoRot = EulerAngles::Identity;

		if (creature->MuzzleFlash[0].Delay != 0)
			creature->MuzzleFlash[0].Delay--;

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != WHITE_SOLDIER_STATE_DEATH)
			{
				SetAnimation(*item, WHITE_SOLDIER_DIE_ANIM);
				item->ItemFlags[FINAL_SHOT_FLAG_INDEX] = Random::GenerateInt(1, FINAL_SHOT_COUNT);
			}
			else
			{
				PerformFinalAttack(*item, WhiteSoldierGunBite, 8, WHITE_SOLDIER_DIE_ANIM, WHITE_SOLDIER_SHOT_DAMAGE * 3, SFX_TR3_SWAT_SMG_FIRE);
			}
		}
		else
		{
			if (item->AIBits)
				GetAITarget(creature);
			else
				creature->Enemy = LaraItem;

			AI_INFO ai;
			CreatureAIInfo(item, &ai);

			AI_INFO laraAI;
			if (creature->Enemy == LaraItem)
			{
				laraAI.angle = ai.angle;
				laraAI.distance = ai.distance;
			}
			else
			{
				int dx = LaraItem->Pose.Position.x - item->Pose.Position.x;
				int dz = LaraItem->Pose.Position.z - item->Pose.Position.z;
				laraAI.angle = phd_atan(dz, dx) - item->Pose.Orientation.y;
				laraAI.distance = SQUARE(dx) + SQUARE(dz);
			}

			bool metaMood = (creature->Enemy != LaraItem);
			GetCreatureMood(item, &ai, metaMood);
			CreatureMood(item, &ai, metaMood);

			angle = CreatureTurn(item, creature->MaxTurn);

			auto* realEnemy = creature->Enemy;
			creature->Enemy = LaraItem;

			if ((laraAI.distance < WHITE_SOLDIER_AWARE_DISTANCE || item->HitStatus || TargetVisible(item, &laraAI)) &&
				!(item->AIBits & FOLLOW))
			{
				if (!creature->Alerted)
					SoundEffect(SFX_TR3_AMERCAN_HOY, &item->Pose);

				AlertAllGuards(itemNumber);
			}

			creature->Enemy = realEnemy;

			switch (item->Animation.ActiveState)
			{
			case WHITE_SOLDIER_STATE_STOP:
				head = laraAI.angle;
				creature->Flags = 0;
				creature->MaxTurn = 0;

				if (item->Animation.AnimNumber == WHITE_SOLDIER_WALK_STOP_ANIM)
				{
					if (abs(ai.angle) < WHITE_SOLDIER_RUN_TURN_RATE_MAX)
						item->Pose.Orientation.y += ai.angle;
					else if (ai.angle < 0)
						item->Pose.Orientation.y -= WHITE_SOLDIER_RUN_TURN_RATE_MAX;
					else
						item->Pose.Orientation.y += WHITE_SOLDIER_RUN_TURN_RATE_MAX;
				}

				if (item->AIBits & GUARD)
				{
					head = AIGuard(creature);
					if (Random::TestProbability(1 / 256.0f))
					{
						if (item->Animation.ActiveState == WHITE_SOLDIER_STATE_STOP)
							item->Animation.TargetState = WHITE_SOLDIER_STATE_WAIT;
						else
							item->Animation.TargetState = WHITE_SOLDIER_STATE_STOP;
					}
				}
				else if (item->AIBits & PATROL1)
				{
					item->Animation.TargetState = WHITE_SOLDIER_STATE_WALK;
				}
				else if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = WHITE_SOLDIER_STATE_RUN;
				}
				else if (Targetable(item, &ai))
				{
					if (ai.distance < WHITE_SOLDIER_SHOOT1_RANGE || ai.zoneNumber != ai.enemyZone)
					{
						if (Random::TestProbability(0.5f))
							item->Animation.TargetState = WHITE_SOLDIER_STATE_AIM1;
						else
							item->Animation.TargetState = WHITE_SOLDIER_STATE_AIM3;
					}
					else
					{
						item->Animation.TargetState = WHITE_SOLDIER_STATE_WALK;
					}
				}
				else if (creature->Mood == MoodType::Bored ||
					((item->AIBits & FOLLOW) && (creature->ReachedGoal || laraAI.distance > SQUARE(BLOCK(2)))))
				{
					item->Animation.TargetState = WHITE_SOLDIER_STATE_STOP;
				}
				else if (creature->Mood != MoodType::Bored && ai.distance > WHITE_SOLDIER_RUN_RANGE)
				{
					item->Animation.TargetState = WHITE_SOLDIER_STATE_RUN;
				}
				else
				{
					item->Animation.TargetState = WHITE_SOLDIER_STATE_WALK;
				}

				break;

			case WHITE_SOLDIER_STATE_WAIT:
				head = laraAI.angle;
				creature->Flags = 0;
				creature->MaxTurn = 0;

				if (item->AIBits & GUARD)
				{
					head = AIGuard(creature);
					if (Random::TestProbability(1 / 256.0f))
					{
						if (item->Animation.ActiveState == WHITE_SOLDIER_STATE_STOP)
							item->Animation.TargetState = WHITE_SOLDIER_STATE_WAIT;
						else
							item->Animation.TargetState = WHITE_SOLDIER_STATE_STOP;
					}
				}
				else if (Targetable(item, &ai))
				{
					item->Animation.TargetState = WHITE_SOLDIER_STATE_SHOOT1;
				}
				else if (creature->Mood != MoodType::Bored || !ai.ahead)
				{
					item->Animation.TargetState = WHITE_SOLDIER_STATE_STOP;
				}

				break;

			case WHITE_SOLDIER_STATE_WALK:
				head = laraAI.angle;
				creature->Flags = 0;
				creature->MaxTurn = WHITE_SOLDIER_WALK_TURN_RATE_MAX;

				if (item->AIBits & PATROL1)
				{
					item->Animation.TargetState = WHITE_SOLDIER_STATE_WALK;
					head = 0;
				}
				else if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = WHITE_SOLDIER_STATE_RUN;
				}
				else if ((item->AIBits & GUARD) ||
					((item->AIBits & FOLLOW) && (creature->ReachedGoal || laraAI.distance > SQUARE(BLOCK(2)))))
				{
					item->Animation.TargetState = WHITE_SOLDIER_STATE_STOP;
				}
				else if (Targetable(item, &ai))
				{
					if (ai.distance < WHITE_SOLDIER_SHOOT1_RANGE || ai.zoneNumber != ai.enemyZone)
						item->Animation.TargetState = WHITE_SOLDIER_STATE_STOP;
					else
						item->Animation.TargetState = WHITE_SOLDIER_STATE_AIM2;
				}
				else if (creature->Mood == MoodType::Bored && ai.ahead)
				{
					item->Animation.TargetState = WHITE_SOLDIER_STATE_STOP;
				}
				else if (creature->Mood != MoodType::Bored && ai.distance > WHITE_SOLDIER_RUN_RANGE)
				{
					item->Animation.TargetState = WHITE_SOLDIER_STATE_RUN;
				}

				break;

			case WHITE_SOLDIER_STATE_RUN:
				if (ai.ahead)
					head = ai.angle;

				creature->MaxTurn = WHITE_SOLDIER_RUN_TURN_RATE_MAX;
				tilt = angle / 2;

				if ((item->AIBits & GUARD) ||
					((item->AIBits & FOLLOW) && (creature->ReachedGoal || laraAI.distance > SQUARE(BLOCK(2)))))
				{
					item->Animation.TargetState = WHITE_SOLDIER_STATE_WALK;
				}
				else if (creature->Mood == MoodType::Escape)
				{
					break;
				}
				else if (Targetable(item, &ai))
				{
					item->Animation.TargetState = WHITE_SOLDIER_STATE_WALK;
				}
				else if (creature->Mood == MoodType::Bored ||
					(creature->Mood == MoodType::Stalk && !(item->AIBits & FOLLOW) && ai.distance < WHITE_SOLDIER_RUN_RANGE))
				{
					item->Animation.TargetState = WHITE_SOLDIER_STATE_WALK;
				}

				break;

			case WHITE_SOLDIER_STATE_AIM1:
			case WHITE_SOLDIER_STATE_AIM3:
				creature->Flags = 0;

				if (ai.ahead)
				{
					extraTorsoRot.y = ai.angle;
					extraTorsoRot.x = ai.xAngle;

					if (Targetable(item, &ai))
					{
						item->Animation.TargetState = (item->Animation.ActiveState == WHITE_SOLDIER_STATE_AIM1) ?
							WHITE_SOLDIER_STATE_SHOOT1 : WHITE_SOLDIER_STATE_SHOOT3;
					}
					else
					{
						item->Animation.TargetState = WHITE_SOLDIER_STATE_STOP;
					}
				}

				break;

			case WHITE_SOLDIER_STATE_AIM2:
				creature->Flags = 0;

				if (ai.ahead)
				{
					extraTorsoRot.y = ai.angle;
					extraTorsoRot.x = ai.xAngle;

					if (Targetable(item, &ai))
						item->Animation.TargetState = WHITE_SOLDIER_STATE_SHOOT2;
					else
						item->Animation.TargetState = WHITE_SOLDIER_STATE_WALK;
				}

				break;

			case WHITE_SOLDIER_STATE_SHOOT3:
			case WHITE_SOLDIER_STATE_SHOOT2:
			case WHITE_SOLDIER_STATE_SHOOT1:
				// SHOOT3 re-checks whether to stop before the shared shoot logic runs.
				if (item->Animation.ActiveState == WHITE_SOLDIER_STATE_SHOOT3 &&
					item->Animation.TargetState != WHITE_SOLDIER_STATE_STOP)
				{
					if (creature->Mood == MoodType::Escape || ai.distance > WHITE_SOLDIER_SHOOT1_RANGE || !Targetable(item, &ai))
						item->Animation.TargetState = WHITE_SOLDIER_STATE_STOP;
				}
				if (ai.ahead)
				{
					extraTorsoRot.y = ai.angle;
					extraTorsoRot.x = ai.xAngle;
				}

				if (!creature->Flags)
				{
					ShotLara(item, &ai, WhiteSoldierGunBite, extraTorsoRot.y, WHITE_SOLDIER_SHOT_DAMAGE);
					creature->MuzzleFlash[0].Bite = WhiteSoldierGunBite;
					creature->MuzzleFlash[0].Delay = 2;
					creature->Flags = 5;
				}
				else
				{
					creature->Flags--;
				}

				break;
			}
		}

		CreatureTilt(item, tilt);
		CreatureJoint(item, 0, extraTorsoRot.y);
		CreatureJoint(item, 1, extraTorsoRot.x);
		CreatureJoint(item, 2, head);
		CreatureAnimation(itemNumber, angle, 0);
	}
}
