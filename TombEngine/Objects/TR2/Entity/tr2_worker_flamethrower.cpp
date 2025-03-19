#include "framework.h"
#include "Objects/TR2/Entity/tr2_worker_flamethrower.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Game/itemdata/creature_info.h"
#include "Game/missile.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Specific/level.h"

using namespace TEN::Animation;

namespace TEN::Entities::Creatures::TR2
{
	constexpr auto WORKER_FLAME_ATTACK_RANGE = SQUARE(BLOCK(2));
	constexpr auto WORKER_FLAME_IDLE_RANGE	 = SQUARE(BLOCK(2));
	constexpr auto WORKER_FLAME_WALK_RANGE	 = SQUARE(BLOCK(2));
	constexpr auto WORKER_FLAME_RUN_RANGE	 = SQUARE(BLOCK(4));

	constexpr auto WORKER_FLAME_WALK_TURN_RATE_MAX = ANGLE(5.0f);
	constexpr auto WORKER_FLAME_RUN_TURN_RATE_MAX  = ANGLE(10.0f);

	const auto WorkerFlamethrowerOffset = Vector3i(0, 140, 0);
	const auto WorkerFlamethrowerBite = CreatureBiteInfo(Vector3(0.0f, 250.0f, 32.0f), 9);

	enum WorkerFlamethrowerState
	{
		// No state 0.
		WORKER_FLAME_STATE_IDLE = 1,
		WORKER_FLAME_STATE_WALK_FORWARD = 2,
		WORKER_FLAME_STATE_RUN = 3,
		WORKER_FLAME_STATE_WAIT = 4,
		WORKER_FLAME_STATE_ATTACK = 5,
		WORKER_FLAME_STATE_WALK_FORWARD_ATTACK = 6,
		WORKER_FLAME_STATE_DEATH = 7,
		WORKER_FLAME_STATE_AIM = 8,
		WORKER_FLAME_STATE_WALK_FORWARD_AIM = 9
	};

	// TODO: Fill out the rest.
	enum WorkerFlamethrowerAnim
	{
		WORKER_FLAME_ANIM_IDLE = 12,
		WORKER_FLAME_ANIM_DEATH = 19
	};

	void InitializeWorkerFlamethrower(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(*item, WORKER_FLAME_ANIM_IDLE);
	}

	void WorkerFlamethrower(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short headingAngle = 0;
		short tiltAngle = 0;
		auto extraHeadRot = EulerAngles::Identity;
		auto extraTorsoRot = EulerAngles::Identity;

		auto pos = GetJointPosition(item, WorkerFlamethrowerBite.BoneID, WorkerFlamethrowerBite.Position);

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != WORKER_FLAME_STATE_DEATH)
				SetAnimation(*item, WORKER_FLAME_ANIM_DEATH);
		}
		else
		{
			if (item->Animation.ActiveState != WORKER_FLAME_STATE_ATTACK && item->Animation.ActiveState != WORKER_FLAME_STATE_WALK_FORWARD_ATTACK)
			{
				SpawnDynamicLight(pos.x, pos.y, pos.z, (GetRandomControl() & 4) + 10, (GetRandomControl() & 7) + 128, (GetRandomControl() & 7) + 64, GetRandomControl() & 7);
				TriggerPilotFlame(itemNumber, WorkerFlamethrowerBite.BoneID);
			}
			else
			{
				SpawnDynamicLight(pos.x, pos.y, pos.z, (GetRandomControl() & 4) + 14, (GetRandomControl() & 7) + 128, (GetRandomControl() & 7) + 64, GetRandomControl() & 7);
				ThrowFire(itemNumber, WorkerFlamethrowerBite.BoneID, WorkerFlamethrowerOffset, WorkerFlamethrowerOffset);
			}

			AI_INFO ai;
			CreatureAIInfo(item, &ai);

			GetCreatureMood(item, &ai, true);
			CreatureMood(item, &ai, true);

			headingAngle = CreatureTurn(item, creature->MaxTurn);

			switch (item->Animation.ActiveState)
			{
			case WORKER_FLAME_STATE_IDLE:
				creature->MaxTurn = 0;
				creature->Flags = 0;

				if (ai.ahead)
				{
					extraHeadRot.x = ai.xAngle;
					extraHeadRot.y = ai.angle;
				}

				if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = WORKER_FLAME_STATE_RUN;
				}
				else if (Targetable(item, &ai))
				{
					if (ai.distance < WORKER_FLAME_ATTACK_RANGE || ai.zoneNumber != ai.enemyZone)
					{
						item->Animation.TargetState = WORKER_FLAME_STATE_AIM;
					}
					else
					{
						item->Animation.TargetState = WORKER_FLAME_STATE_WALK_FORWARD;
					}
				}
				else if (creature->Mood == MoodType::Attack || !ai.ahead)
				{
					if (ai.distance <= WORKER_FLAME_WALK_RANGE)
					{
						item->Animation.TargetState = WORKER_FLAME_STATE_WALK_FORWARD;
					}
					else if (ai.distance >= WORKER_FLAME_RUN_RANGE)
					{
						item->Animation.TargetState = WORKER_FLAME_STATE_RUN;
					}
				}
				else
				{
					item->Animation.TargetState = WORKER_FLAME_STATE_WAIT;
				}

				break;

			case WORKER_FLAME_STATE_WALK_FORWARD:
				creature->MaxTurn = WORKER_FLAME_WALK_TURN_RATE_MAX;

				if (ai.ahead)
				{
					extraHeadRot.x = ai.xAngle;
					extraHeadRot.y = ai.angle;
				}

				if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = WORKER_FLAME_STATE_RUN;
				}
				else if (Targetable(item, &ai))
				{
					if (ai.distance < WORKER_FLAME_IDLE_RANGE || ai.zoneNumber != ai.enemyZone)
					{
						item->Animation.TargetState = WORKER_FLAME_STATE_IDLE;
					}
					else if (ai.distance < WORKER_FLAME_ATTACK_RANGE)
					{
						item->Animation.TargetState = WORKER_FLAME_STATE_WALK_FORWARD_AIM;
					}
				}
				else if (creature->Mood == MoodType::Attack || !ai.ahead)
				{
					if (ai.distance > WORKER_FLAME_WALK_RANGE)
						item->Animation.TargetState = WORKER_FLAME_STATE_RUN;
				}
				else
				{
					item->Animation.TargetState = WORKER_FLAME_STATE_WAIT;
				}

				break;

			case WORKER_FLAME_STATE_RUN:
				creature->MaxTurn = WORKER_FLAME_RUN_TURN_RATE_MAX;

				if (ai.ahead)
				{
					extraHeadRot.x = ai.xAngle;
					extraHeadRot.y = ai.angle;
				}

				if (creature->Mood != MoodType::Escape)
				{
					if (Targetable(item, &ai))
					{
						item->Animation.TargetState = WORKER_FLAME_STATE_WALK_FORWARD;
					}
					else if (creature->Mood == MoodType::Bored || creature->Mood == MoodType::Stalk)
					{
						item->Animation.TargetState = WORKER_FLAME_STATE_WALK_FORWARD;
					}
				}

				break;

			case 4:
				if (ai.ahead)
				{
					extraHeadRot.x = ai.xAngle;
					extraHeadRot.y = ai.angle;
				}

				if (Targetable(item, &ai))
				{
					item->Animation.TargetState = WORKER_FLAME_STATE_ATTACK;
				}
				else
				{
					if (creature->Mood == MoodType::Attack)
					{
						item->Animation.TargetState = WORKER_FLAME_STATE_IDLE;
					}
					else if (!ai.ahead)
					{
						item->Animation.TargetState = WORKER_FLAME_STATE_IDLE;
					}
				}

				break;

			case WORKER_FLAME_STATE_ATTACK:
			case WORKER_FLAME_STATE_WALK_FORWARD_ATTACK:
				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				if (item->Animation.TargetState != WORKER_FLAME_STATE_IDLE &&
					(!Targetable(item, &ai) || creature->Mood == MoodType::Escape || ai.distance > WORKER_FLAME_ATTACK_RANGE))
				{
					item->Animation.TargetState = WORKER_FLAME_STATE_IDLE;
				}

				break;

			case WORKER_FLAME_STATE_AIM:
			case WORKER_FLAME_STATE_WALK_FORWARD_AIM:
				creature->Flags = 0;

				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				if (Targetable(item, &ai) && ai.distance <= WORKER_FLAME_ATTACK_RANGE)
				{
					item->Animation.TargetState = (item->Animation.ActiveState == WORKER_FLAME_STATE_AIM) ? WORKER_FLAME_STATE_ATTACK : WORKER_FLAME_STATE_WALK_FORWARD_ATTACK;
				}
				else
				{
					item->Animation.TargetState = WORKER_FLAME_STATE_IDLE;
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
