#include "framework.h"
#include "Objects/TR2/Entity/tr2_worker_shotgun.h"

#include "Game/Animation/Animation.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR2
{
	constexpr auto WORKER_SHOTGUN_NUM_SHOTS = 6;

	const auto WorkerShotgunBite = CreatureBiteInfo(Vector3(0, 350, 40), 9);

	enum ShotgunWorkerState
	{
		// No state 0.
		WORKER_SHOTGUN_STATE_WALK = 1,
		WORKER_SHOTGUN_STATE_IDLE = 2,
		WORKER_SHOTGUN_STATE_REST = 3,
		WORKER_SHOTGUN_STATE_STANDING_ATTACK = 4,
		WORKER_SHOTGUN_STATE_RUN = 5,
		WORKER_SHOTGUN_STATE_WALKING_ATTACK = 6,
		WORKER_SHOTGUN_STATE_DEATH = 7,
		WORKER_SHOTGUN_STATE_STANDING_ATTACK_AIM = 8,
		WORKER_SHOTGUN_STATE_KNEEL_ATTACK_AIM = 9,
		WORKER_SHOTGUN_STATE_KNEEL_ATTACK = 10
	};

	enum ShotgunWorkerAnim
	{
		WORKER_SHOTGUN_ANIM_WALK = 0,
		WORKER_SHOTGUN_ANIM_STANDING_ATTACK_AIM = 1,
		WORKER_SHOTGUN_ANIM_STANDING_ATTACK_SHOOT = 2,
		WORKER_SHOTGUN_ANIM_STANDING_ATTACK_STOP = 3,
		WORKER_SHOTGUN_ANIM_WALK_TO_IDLE = 4,
		WORKER_SHOTGUN_ANIM_IDLE = 5,
		WORKER_SHOTGUN_ANIM_IDLE_TO_WALK = 6,
		WORKER_SHOTGUN_ANIM_WALKING_ATTACK_AIM = 7,
		WORKER_SHOTGUN_ANIM_WALKING_ATTACK_SHOOT = 8,
		WORKER_SHOTGUN_ANIM_REST_TO_IDLE = 9,
		WORKER_SHOTGUN_ANIM_REST = 10,
		WORKER_SHOTGUN_ANIM_REST_TO_STANDING_ATTACK = 11,
		WORKER_SHOTGUN_ANIM_IDLE_TO_REST = 12,
		WORKER_SHOTGUN_ANIM_STANDING_ATTACK_TO_IDLE = 13,
		WORKER_SHOTGUN_ANIM_WALK_TO_RUN = 14,
		WORKER_SHOTGUN_ANIM_RUN_TO_WALK = 15,
		WORKER_SHOTGUN_ANIM_IDLE_TO_RUN = 16,
		WORKER_SHOTGUN_ANIM_RUN = 17,
		WORKER_SHOTGUN_ANIM_DEATH = 18,
		WORKER_SHOTGUN_ANIM_KNEEL_ATTACK_AIM = 19,
		WORKER_SHOTGUN_ANIM_KNEEL_ATTACK_SHOOT = 20,
		WORKER_SHOTGUN_ANIM_KNEEL_ATTACK_STOP = 21
	};

	static void ShootWorkerShotgun(ItemInfo& item, AI_INFO& ai, const CreatureBiteInfo& bite, short headingAngle, int damage)
	{
		for (int i = 0; i < WORKER_SHOTGUN_NUM_SHOTS; i++)
			ShotLara(&item, &ai, bite, headingAngle, damage);
	}

	void InitializeWorkerShotgun(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(item, 5);
	}

	void WorkerShotgunControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short headingAngle = 0;
		short tiltAngle = 0;
		auto extraHeadRot = EulerAngles::Identity;
		auto extraTorsoRot = EulerAngles::Identity;

		if (creature->MuzzleFlash[0].Delay != 0)
			creature->MuzzleFlash[0].Delay--;

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != WORKER_SHOTGUN_STATE_DEATH)
				SetAnimation(*item, WORKER_SHOTGUN_ANIM_DEATH);
		}
		else
		{
			AI_INFO ai;
			CreatureAIInfo(item, &ai);

			GetCreatureMood(item, &ai, true);
			CreatureMood(item, &ai, true);

			headingAngle = CreatureTurn(item, creature->MaxTurn);

			switch (item->Animation.ActiveState)
			{
			case WORKER_SHOTGUN_STATE_IDLE:
				creature->MaxTurn = 0;
				creature->Flags = 0;

				if (ai.ahead)
				{
					extraHeadRot.x = ai.xAngle;
					extraHeadRot.y = ai.angle;
				}

				if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = WORKER_SHOTGUN_STATE_RUN;
				}
				else if (Targetable(item, &ai))
				{
					if (ai.distance < SQUARE(BLOCK(3)) || ai.zoneNumber != ai.enemyZone)
					{
						item->Animation.TargetState = Random::TestProbability(1 / 2.0f) ? WORKER_SHOTGUN_STATE_KNEEL_ATTACK_AIM : WORKER_SHOTGUN_STATE_STANDING_ATTACK_AIM;
					}
					else
					{
						item->Animation.TargetState = WORKER_SHOTGUN_STATE_WALK;
					}
				}
				else if (creature->Mood == MoodType::Attack || !ai.ahead)
				{
					if (ai.distance <= SQUARE(BLOCK(2)))
					{
						item->Animation.TargetState = WORKER_SHOTGUN_STATE_WALK;
					}
					else
					{
						item->Animation.TargetState = WORKER_SHOTGUN_STATE_RUN;
					}
				}
				else
				{
					item->Animation.TargetState = WORKER_SHOTGUN_STATE_REST;
				}

				break;

			case WORKER_SHOTGUN_STATE_REST:
				if (ai.ahead)
				{
					extraHeadRot.x = ai.xAngle;
					extraHeadRot.y = ai.angle;
				}

				if (Targetable(item, &ai))
				{
					item->Animation.TargetState = WORKER_SHOTGUN_STATE_STANDING_ATTACK;
				}
				else if (creature->Mood == MoodType::Attack || !ai.ahead)
				{
					item->Animation.TargetState = WORKER_SHOTGUN_STATE_IDLE;
				}

				break;

			case WORKER_SHOTGUN_STATE_WALK:
				creature->MaxTurn = ANGLE(3.0f);

				if (ai.ahead)
				{
					extraHeadRot.y = ai.angle;
					extraHeadRot.x = ai.xAngle;
				}

				if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = WORKER_SHOTGUN_STATE_RUN;
				}
				else if (Targetable(item, &ai))
				{
					if (ai.distance < SQUARE(BLOCK(3)) || ai.zoneNumber != ai.enemyZone)
					{
						item->Animation.TargetState = WORKER_SHOTGUN_STATE_IDLE;
					}
					else
					{
						item->Animation.TargetState = WORKER_SHOTGUN_STATE_WALKING_ATTACK;
						creature->Flags = 0;
					}
				}
				else if (creature->Mood == MoodType::Attack || !ai.ahead)
				{
					if (ai.distance > SQUARE(BLOCK(2)))
						item->Animation.TargetState = WORKER_SHOTGUN_STATE_RUN;
				}
				else
				{
					item->Animation.TargetState = WORKER_SHOTGUN_STATE_IDLE;
				}

				break;

			case WORKER_SHOTGUN_STATE_RUN:
				creature->MaxTurn = ANGLE(5.0f);
				tiltAngle = headingAngle / 2;

				if (ai.ahead)
				{
					extraHeadRot.x = ai.xAngle;
					extraHeadRot.y = ai.angle;
				}

				if (creature->Mood != MoodType::Escape)
				{
					if (Targetable(item, &ai))
					{
						item->Animation.TargetState = WORKER_SHOTGUN_STATE_WALK;
					}
					else if (creature->Mood == MoodType::Bored || creature->Mood == MoodType::Stalk)
					{
						item->Animation.TargetState = WORKER_SHOTGUN_STATE_WALK;
					}
				}

				break;

			case WORKER_SHOTGUN_STATE_STANDING_ATTACK_AIM:
			case WORKER_SHOTGUN_STATE_KNEEL_ATTACK_AIM:
				creature->Flags = 0;

				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				if (Targetable(item, &ai))
				{
					if (item->Animation.ActiveState == WORKER_SHOTGUN_STATE_STANDING_ATTACK_AIM)
					{
						item->Animation.TargetState = WORKER_SHOTGUN_STATE_STANDING_ATTACK;
					}
					else
					{
						item->Animation.TargetState = WORKER_SHOTGUN_STATE_KNEEL_ATTACK;
					}
				}

				break;

			case WORKER_SHOTGUN_STATE_STANDING_ATTACK:
			case WORKER_SHOTGUN_STATE_KNEEL_ATTACK:
				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				if (creature->Flags == 0)
				{
					ShootWorkerShotgun(*item, ai, WorkerShotgunBite, extraTorsoRot.y, 25);
					creature->MuzzleFlash[0].Bite = WorkerShotgunBite;
					creature->MuzzleFlash[0].Delay = 2;
					creature->Flags = 1;
				}

				if (item->Animation.ActiveState == WORKER_SHOTGUN_STATE_STANDING_ATTACK && item->Animation.TargetState != WORKER_SHOTGUN_STATE_IDLE &&
					(creature->Mood == MoodType::Escape || ai.distance > SQUARE(BLOCK(3)) || !Targetable(item, &ai)))
				{
					item->Animation.TargetState = WORKER_SHOTGUN_STATE_IDLE;
				}

				break;

			case WORKER_SHOTGUN_STATE_WALKING_ATTACK:
				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				if (creature->Flags == 0)
				{
					creature->MuzzleFlash[0].Bite = WorkerShotgunBite;
					creature->MuzzleFlash[0].Delay = 1;
					ShootWorkerShotgun(*item, ai, WorkerShotgunBite, extraTorsoRot.y, 25);
					creature->Flags = 1;
				}

				break;
			}
		}

		CreatureTilt(item, tiltAngle);
		CreatureJoint(item, 0, extraTorsoRot.y);
		CreatureJoint(item, 1, extraTorsoRot.x);
		CreatureJoint(item, 2, extraHeadRot.y);
		CreatureJoint(item, 3, extraHeadRot.x);
		CreatureAnimation(itemNumber, headingAngle, tiltAngle);
	}
}
