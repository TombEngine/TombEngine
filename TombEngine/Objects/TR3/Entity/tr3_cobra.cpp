#include "framework.h"
#include "Objects/TR3/Entity/tr3_cobra.h"

#include "Game/control/box.h"
#include "Game/control/los.h"
#include "Game/Animation/Animation.h"
#include "Game/itemdata/creature_info.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_struct.h"
#include "Game/misc.h"
#include "Specific/level.h"

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto COBRA_BITE_ATTACK_DAMAGE = 80;
	constexpr auto COBRA_BITE_POISON_POTENCY = 8;

	constexpr auto COBRA_ATTACK_RANGE = SQUARE(BLOCK(1));
	constexpr auto COBRA_AWARE_RANGE = SQUARE(BLOCK(1.5f));
	constexpr auto COBRA_SLEEP_RANGE = SQUARE(BLOCK(2.5f));

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
		SetAnimation(item, COBRA_ANIM_IDLE_TO_SLEEP, COBRA_SLEEP_FRAME);
		item->ItemFlags[2] = item->HitStatus;
	}

	void CobraControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short tilt  = 0;

		if (item->HitPoints <= 0 && item->HitPoints != NOT_TARGETABLE)
		{
			if (item->Animation.ActiveState != COBRA_STATE_DEATH)
				SetAnimation(item, COBRA_ANIM_DEATH);
		}
		else
		{
			AI_INFO AI;
			CreatureAIInfo(item, &AI);

			AI.angle += ANGLE(16.8f);

			bool isEnemyMoving  = false;
			bool isEnemyVisible = false;

			if (creature->Enemy && item->Animation.ActiveState != COBRA_STATE_SLEEP)
			{
				auto enemyPos = creature->Enemy->Pose.Position;
				enemyPos.y -= CLICK(1.5f);

				auto origin = GameVector(enemyPos, creature->Enemy->RoomNumber);
				auto target = GameVector(GetJointPosition(item, 9), item->RoomNumber);

				isEnemyVisible = LOS(&origin, &target);

				if (creature->Enemy->Animation.Velocity.z > COBRA_DISTURBANCE_VELOCITY ||
					abs(creature->Enemy->Animation.Velocity.y) > COBRA_DISTURBANCE_VELOCITY)
				{
					isEnemyMoving = true;
				}

				creature->Target.x = creature->Enemy->Pose.Position.x;
				creature->Target.z = creature->Enemy->Pose.Position.z;
			}

			if (isEnemyVisible && item->Animation.ActiveState != COBRA_STATE_SLEEP)
			{
				angle = CreatureTurn(item, creature->MaxTurn);

				if (AI.ahead)
					tilt = -AI.xAngle;

				short turnStep = (item->Animation.ActiveState == COBRA_STATE_WAKE_UP) ? ANGLE(10.0f) : ANGLE(3.0f);

				if (abs(AI.angle) < turnStep)
					item->Pose.Orientation.y += AI.angle;
				else if (AI.angle < 0)
					item->Pose.Orientation.y -= turnStep;
				else
					item->Pose.Orientation.y += turnStep;
			}

			switch (item->Animation.ActiveState)
			{
			case COBRA_STATE_IDLE:
				creature->Flags = 0;

				if (AI.distance > COBRA_SLEEP_RANGE)
				{
					item->Animation.TargetState = COBRA_STATE_SLEEP;
				}
				else if (creature->Enemy->HitPoints > 0 && (isEnemyVisible || item->HitStatus) &&
					((AI.ahead && AI.distance < COBRA_ATTACK_RANGE && abs(AI.verticalDistance) <= GameBoundingBox(item).GetHeight() / 2) || item->HitStatus || isEnemyMoving))
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

				if (AI.distance < COBRA_AWARE_RANGE && creature->Enemy && creature->Enemy->HitPoints > 0)
				{
					item->Animation.TargetState = COBRA_STATE_WAKE_UP;
					item->HitPoints = item->ItemFlags[2];
				}

				break;

			case COBRA_STATE_ATTACK:
				if (!(creature->Flags & 1) &&
					item->Animation.FrameNumber <= (GetFrameCount(*item) / 2) &&
					item->TouchBits.Test(CobraAttackJoints))
				{
					creature->Flags |= 1;
					DoDamage(creature->Enemy, COBRA_BITE_ATTACK_DAMAGE);
					CreatureEffect(item, CobraBite, DoBloodSplat);

					if (creature->Enemy && creature->Enemy->IsLara())
						GetLaraInfo(creature->Enemy)->Status.Poison += COBRA_BITE_POISON_POTENCY;
				}

				break;

			case COBRA_STATE_WAKE_UP:
				item->HitPoints = item->ItemFlags[2];
				break;
			}
		}

		CreatureJoint(item, 1, tilt / 1.5f);
		CreatureAnimation(itemNumber, angle, 0);
	}
}
