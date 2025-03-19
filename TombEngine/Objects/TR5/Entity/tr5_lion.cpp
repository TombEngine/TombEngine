#include "framework.h"
#include "Objects/TR5/Entity/tr5_lion.h"

#include "Game/collision/collide_room.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Specific/level.h"

using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR5
{
	constexpr auto LION_POUNCE_ATTACK_DAMAGE = 200;
	constexpr auto LION_BITE_ATTACK_DAMAGE	 = 60;

	constexpr auto LION_POUNCE_ATTACK_RANGE = SQUARE(BLOCK(1.25f));
	constexpr auto LION_BITE_ATTACK_RANGE	= SQUARE(BLOCK(0.7f));
	constexpr auto LION_ATTACK_RANGE		= SQUARE(BLOCK(0.4f));

	constexpr auto LION_WALK_CHANCE = 63 / 64.0f;
	constexpr auto LION_ROAR_CHANCE = 1 / 256.0f;

	constexpr auto LION_HEAD_DISTANCE = BLOCK(0.4f);

	constexpr auto LION_WALK_TURN_RATE_MAX	 = ANGLE(2.0f);
	constexpr auto LION_RUN_TURN_RATE_MAX	 = ANGLE(5.0f);
	constexpr auto LION_ATTACK_TURN_RATE_MAX = ANGLE(1.0f);

	const auto LionBite1 = CreatureBiteInfo(Vector3(2, -10, 250), 21);
	const auto LionBite2 = CreatureBiteInfo(Vector3(-2, -10, 132), 21);
	const auto LionAttackJoints = std::vector<unsigned int>{ 3, 6, 21 };

	enum LionState
	{
		LION_STATE_NONE = 0,
		LION_STATE_IDLE = 1,
		LION_STATE_WALK_FORWARD = 2,
		LION_STATE_RUN_FORWARD = 3,
		LION_STATE_POUNCE_ATTACK = 4,
		LION_STATE_DEATH = 5,
		LION_STATE_ROAR = 6,
		LION_STATE_BITE_ATTACK = 7
	};

	enum LionAnim
	{
		LION_ANIM_IDLE = 0,
		LION_ANIM_IDLE_TO_WALK_FORWARD = 1,
		LION_ANIM_WALK_FORWARD = 2,
		LION_ANIM_WALK_FORWARD_TO_IDLE = 3,
		LION_ANIM_RUN_FORWARD = 4,
		LION_ANIM_IDLE_TO_RUN_FORWARD = 5,
		LION_ANIM_RUN_FORWARD_TO_IDLE = 6,
		LION_ANIM_DEATH_1 = 7,
		LION_ANIM_DEATH_2 = 8,
		LION_ANIM_POUNCE_ATTACK_START = 9,
		LION_ANIM_POUNCE_ATTACK_END = 10,
		LION_ANIM_BITE_ATTACK = 11,
		LION_ANIM_ROAR = 12
	};

	const std::array LionDeathAnims = { LION_ANIM_DEATH_1, LION_ANIM_DEATH_2 };

	void InitializeLion(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(*item, LION_ANIM_IDLE);
	}

	void LionControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* object = &Objects[item->ObjectNumber];
		auto* creature = GetCreatureInfo(item);

		short headingAngle = 0;
		short tiltAngle = 0;
		short joint0 = 0;
		short joint1 = 0;

		bool isTargetAhead = false;

		if (item->HitPoints <= 0)
		{
			item->HitPoints = 0;

			if (item->Animation.ActiveState != LION_STATE_DEATH)
				SetAnimation(*item, LionDeathAnims[Random::GenerateInt(0, (int)LionDeathAnims.size() - 1)]);
		}
		else
		{
			AI_INFO ai;
			CreatureAIInfo(item, &ai);

			// NOTE: Because the lion is quite big, AI.ahead doesn't calculate the lion's head angle properly.
			// Calculate manually like with tr1_giant_mutant.

			auto forwardVector = Vector3(
				cos(item->Pose.Orientation.x) * sin(item->Pose.Orientation.y),
				-sin(item->Pose.Orientation.x),
				cos(item->Pose.Orientation.x) * cos(item->Pose.Orientation.y));
			forwardVector.Normalize();
			forwardVector *= LION_HEAD_DISTANCE;

			short headAngle = (short)phd_atan(
				creature->Target.z - item->Pose.Position.z + forwardVector.z,
				creature->Target.x - item->Pose.Position.x + forwardVector.x) -
				item->Pose.Orientation.y;
			isTargetAhead = (headAngle > -FRONT_ARC && headAngle < FRONT_ARC);

			if (isTargetAhead)
				joint1 = headAngle;

			GetCreatureMood(item, &ai, true);
			CreatureMood(item, &ai, true);

			headingAngle = CreatureTurn(item, creature->MaxTurn);
			joint0 = headingAngle * -16;

			switch (item->Animation.ActiveState)
			{
			case LION_STATE_IDLE:
				creature->MaxTurn = 0;
				creature->Flags = 0;

				if (item->Animation.RequiredState != NO_VALUE)
				{
					item->Animation.TargetState = item->Animation.RequiredState;
					break;
				}

				if (creature->Mood == MoodType::Bored)
				{
					if (Random::TestProbability(LION_WALK_CHANCE))
						item->Animation.TargetState = LION_STATE_WALK_FORWARD;

					break;
				}

				if (isTargetAhead && ai.bite)
				{
					if (ai.distance >= LION_ATTACK_RANGE)
					{
						if (ai.distance <= LION_BITE_ATTACK_RANGE)
						{
							item->Animation.TargetState = LION_STATE_BITE_ATTACK;
							break;
						}

						if (ai.distance < LION_POUNCE_ATTACK_RANGE)
						{
							item->Animation.TargetState = LION_STATE_POUNCE_ATTACK;
							break;
						}
					}
				}

				item->Animation.TargetState = LION_STATE_RUN_FORWARD;
				break;

			case LION_STATE_WALK_FORWARD:
				creature->MaxTurn = LION_WALK_TURN_RATE_MAX;

				if (creature->Mood == MoodType::Bored && Random::TestProbability(LION_ROAR_CHANCE))
					item->Animation.TargetState = LION_STATE_ROAR;
				else
					item->Animation.TargetState = LION_STATE_IDLE;

			break;

			case LION_STATE_RUN_FORWARD:
				creature->MaxTurn = LION_RUN_TURN_RATE_MAX;
				tiltAngle = headingAngle;

				if (creature->Mood != MoodType::Bored)
				{
					if (isTargetAhead &&
						ai.bite &&
						ai.distance < LION_POUNCE_ATTACK_RANGE &&
						ai.distance >= LION_ATTACK_RANGE)
					{
						item->Animation.TargetState = LION_STATE_IDLE;
					}
					else if (creature->Mood != MoodType::Escape)
					{
						if (Random::TestProbability(LION_ROAR_CHANCE))
							item->Animation.TargetState = LION_STATE_ROAR;
					}
				}
				else
				{
					item->Animation.TargetState = LION_STATE_IDLE;
				}

			break;

			case LION_STATE_POUNCE_ATTACK:
				creature->MaxTurn = LION_ATTACK_TURN_RATE_MAX;

				if (!creature->Flags &&
					item->Animation.AnimNumber == LION_ANIM_POUNCE_ATTACK_END &&
					item->TouchBits.Test(LionAttackJoints))
				{
					DoDamage(creature->Enemy, LION_POUNCE_ATTACK_DAMAGE);
					CreatureEffect2(item, LionBite1, 10, item->Pose.Orientation.y, DoBloodSplat);
					creature->Flags = 1;
				}

				break;

			case LION_STATE_BITE_ATTACK:
				creature->MaxTurn = LION_ATTACK_TURN_RATE_MAX;

				if (!creature->Flags &&
					item->Animation.AnimNumber == LION_ANIM_BITE_ATTACK &&
					item->TouchBits.Test(LionAttackJoints))
				{
					DoDamage(creature->Enemy, LION_BITE_ATTACK_DAMAGE);
					CreatureEffect2(item, LionBite2, 10, item->Pose.Orientation.y, DoBloodSplat);
					creature->Flags = 1;
				}

				break;
			}
		}

		CreatureTilt(item, tiltAngle);
		CreatureJoint(item, 0, joint0);
		CreatureJoint(item, 1, joint1);
		CreatureAnimation(itemNumber, headingAngle, 0);

		auto radius = Vector2(object->radius);
		AlignEntityToSurface(item, radius);
	}
}
