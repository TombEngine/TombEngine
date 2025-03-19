#include "framework.h"
#include "Objects/TR3/Entity/tr3_cobra.h"

#include "Game/control/box.h"
#include "Game/control/los.h"
#include "Game/itemdata/creature_info.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Specific/level.h"

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto COBRA_BITE_ATTACK_DAMAGE	 = 80;
	constexpr auto COBRA_BITE_POISON_POTENCY = 8;

	constexpr auto COBRA_ATTACK_RANGE = SQUARE(BLOCK(1));
	constexpr auto COBRA_AWARE_RANGE  = SQUARE(BLOCK(1.5f));
	constexpr auto COBRA_SLEEP_RANGE  = SQUARE(BLOCK(2.5f));

	constexpr auto COBRA_DISTURBANCE_VELOCITY = 15.0f;
	constexpr auto COBRA_SLEEP_FRAME = 45;

	const auto CobraBite = CreatureBiteInfo(Vector3::Zero, 13);
	const auto CobraAttackJoints = std::vector<unsigned int>{ 13 };

	enum CobraState
	{
		COBRA_STATE_WAKE_UP = 0,
		COBRA_STATE_IDLE = 1,
		COBRA_STATE_ATTACK = 2,
		COBRA_STATE_SLEEP = 3,
		COBRA_STATE_DEATH = 4
	};

	enum CobraAnim
	{
		COBRA_ANIM_IDLE = 0,
		COBRA_ANIM_SLEEP_TO_IDLE = 1,
		COBRA_ANIM_IDLE_TO_SLEEP = 2,
		COBRA_ANIM_BITE_ATTACK = 3,
		COBRA_ANIM_DEATH = 4
	};

	void InitializeCobra(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(*item, COBRA_ANIM_IDLE_TO_SLEEP, COBRA_SLEEP_FRAME);
		item->ItemFlags[2] = item->HitStatus;
	}

	void CobraControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short tilt = 0;
		short head = 0;

		if (item->HitPoints <= 0 && item->HitPoints != NOT_TARGETABLE)
		{
			if (item->Animation.ActiveState != COBRA_STATE_DEATH)
				SetAnimation(*item, COBRA_ANIM_DEATH);
		}
		else
		{
			AI_INFO AI;
			CreatureAIInfo(item, &AI);

			AI.angle += ANGLE(16.8f);

			GetCreatureMood(item, &AI, 1);
			CreatureMood(item, &AI, 1);

			bool isEnemyMoving  = false;
			bool isEnemyVisible = false;

			if (creature->Enemy != nullptr && (GlobalCounter & 2))
			{
				auto origin = GameVector(creature->Enemy->Pose.Position, creature->Enemy->RoomNumber);
				auto target = GameVector(item->Pose.Position, item->RoomNumber);
				isEnemyVisible = LOS(&origin, &target);

				if (creature->Enemy->Animation.Velocity.z > COBRA_DISTURBANCE_VELOCITY ||
					abs(creature->Enemy->Animation.Velocity.y) > COBRA_DISTURBANCE_VELOCITY)
				{
					isEnemyMoving = true;
				}
			}

			if (isEnemyVisible && item->Animation.ActiveState != COBRA_STATE_SLEEP)
			{
				creature->Target.x = creature->Enemy->Pose.Position.x;
				creature->Target.z = creature->Enemy->Pose.Position.z;
				angle = CreatureTurn(item, creature->MaxTurn);

				if (AI.ahead)
					head = AI.angle;

				if (abs(AI.angle) < ANGLE(10.0f))
					item->Pose.Orientation.y += AI.angle;
				else if (AI.angle < 0)
					item->Pose.Orientation.y -= ANGLE(10.0f);
				else
					item->Pose.Orientation.y += ANGLE(10.0f);
			}

			switch (item->Animation.ActiveState)
			{
			case COBRA_STATE_IDLE:
				creature->Flags = 0;

				if (AI.distance > COBRA_SLEEP_RANGE)
					item->Animation.TargetState = COBRA_STATE_SLEEP;
				else if (creature->Enemy->HitPoints > 0 && isEnemyVisible &&
					((AI.ahead && AI.distance < COBRA_ATTACK_RANGE && abs(AI.verticalDistance) <= GameBoundingBox(item).GetHeight()) ||
						item->HitStatus || isEnemyMoving))
				{
					item->Animation.TargetState = COBRA_STATE_ATTACK;
				}

				break;

			case COBRA_STATE_SLEEP:
				creature->Flags = 0;

				if (item->HitPoints != NOT_TARGETABLE)
				{
					item->ItemFlags[2] = item->HitPoints;
					item->HitPoints = NOT_TARGETABLE;
				}

				if (AI.distance < COBRA_AWARE_RANGE && creature->Enemy->HitPoints > 0)
				{
					item->Animation.TargetState = COBRA_STATE_WAKE_UP;
					item->HitPoints = item->ItemFlags[2];
				}

				break;

			case COBRA_STATE_ATTACK:
				if (!(creature->Flags & 1) && // 1 = is attacking.
					item->TouchBits.Test(CobraAttackJoints))
				{
					DoDamage(creature->Enemy, COBRA_BITE_ATTACK_DAMAGE);
					CreatureEffect(item, CobraBite, DoBloodSplat);
					creature->Flags |= 1; // 1 = is attacking.

					if (creature->Enemy->IsLara())
						GetLaraInfo(creature->Enemy)->Status.Poison += COBRA_BITE_POISON_POTENCY;
				}

				break;

			case COBRA_STATE_WAKE_UP:
				item->HitPoints = item->ItemFlags[2];
				break;
			}
		}

		CreatureTilt(item, tilt);
		CreatureJoint(item, 0, head / 2);
		CreatureJoint(item, 1, head / 2);
		CreatureAnimation(itemNumber, angle, tilt);
	}
}
