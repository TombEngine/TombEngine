#include "framework.h"
#include "Objects/TR2/Entity/tr2_rat.h"

#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Specific/level.h"

using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR2
{
	constexpr auto RAT_ATTACK_DAMAGE = 20;
	constexpr auto RAT_ATTACK_RANGE = SQUARE(CLICK(0.7f));

	constexpr auto RAT_SQUEAK_CHANCE = 0.04f;
	constexpr auto RAT_IDLE_CHANCE	 = 0.08f;
	constexpr auto RAT_WALK_CHANCE	 = 0.92f;

	constexpr auto RAT_TURN_RATE_MAX = ANGLE(6.0f);

	const auto RatBite = CreatureBiteInfo(Vector3(0, 0, 57), 2);

	enum RatState
	{
		// No state 0.
		RAT_STATE_WALK_FORWARD = 1,
		RAT_STATE_IDLE = 2,
		RAT_STATE_SQUEAK = 3,
		RAT_STATE_BITE_ATTACK = 4,
		RAT_STATE_POUNCE_ATTACK = 5,
		RAT_STATE_DEATH = 6
	};

	enum RatAnim
	{
		RAT_ANIM_IDLE = 0,
		RAT_ANIM_IDLE_TO_WALK_FORWARD = 1,
		RAT_ANIM_WALK_FORWARD = 2,
		RAT_ANIM_WALK_FORWARD_TO_IDLE = 3,
		RAT_ANIM_SQUEAK = 4,
		RAT_ANIM_BITE_ATTACK_START = 5,
		RAT_ANIM_BITE_ATTACK_CONTINUE = 6,
		RAT_ANIM_BITE_ATTACK_END = 7,
		RAT_ANIM_POUNCE_ATTACK = 8,
		RAT_ANIM_DEATH = 9
	};

	void RatControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short head = 0;

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != RAT_STATE_DEATH)
				SetAnimation(*item, RAT_ANIM_DEATH);
		}
		else
		{
			AI_INFO AI;
			CreatureAIInfo(item, &AI);

			if (AI.ahead)
				head = AI.angle;

			GetCreatureMood(item, &AI, false);
			CreatureMood(item, &AI, false);

			angle = CreatureTurn(item, RAT_TURN_RATE_MAX);

			switch (item->Animation.ActiveState)
			{
			case RAT_STATE_BITE_ATTACK:
				if (creature->Mood == MoodType::Bored ||
					creature->Mood == MoodType::Stalk)
				{
					if (Random::TestProbability(RAT_SQUEAK_CHANCE))
						item->Animation.RequiredState = RAT_STATE_SQUEAK;
					else if (Random::TestProbability(RAT_WALK_CHANCE))
						item->Animation.RequiredState = RAT_STATE_WALK_FORWARD;
				}
				else if (AI.distance < RAT_ATTACK_RANGE)
					item->Animation.RequiredState = RAT_STATE_POUNCE_ATTACK;
				else
					item->Animation.RequiredState = RAT_STATE_WALK_FORWARD;

				if (item->Animation.RequiredState != NO_VALUE)
					item->Animation.TargetState = RAT_STATE_IDLE;

				break;

			case RAT_STATE_IDLE:
				creature->MaxTurn = 0;

				if (item->Animation.RequiredState != NO_VALUE)
					item->Animation.TargetState = item->Animation.RequiredState;

				break;

			case RAT_STATE_WALK_FORWARD:
				creature->MaxTurn = RAT_TURN_RATE_MAX;

				if (creature->Mood == MoodType::Bored ||
					creature->Mood == MoodType::Stalk)
				{
					if (Random::TestProbability(RAT_SQUEAK_CHANCE))
					{
						item->Animation.TargetState = RAT_STATE_IDLE;
						item->Animation.RequiredState = RAT_STATE_SQUEAK;
					}
					else if (Random::TestProbability(RAT_IDLE_CHANCE))
						item->Animation.TargetState = RAT_STATE_IDLE;
				}
				else if (AI.ahead && AI.distance < RAT_ATTACK_RANGE)
					item->Animation.TargetState = RAT_STATE_IDLE;

				break;

			case RAT_STATE_POUNCE_ATTACK:
				if (item->Animation.RequiredState == NO_VALUE &&
					item->TouchBits.Test(RatBite.BoneID))
				{
					item->Animation.RequiredState = RAT_STATE_IDLE;
					DoDamage(creature->Enemy, RAT_ATTACK_DAMAGE);
					CreatureEffect(item, RatBite, DoBloodSplat);
				}

				break;

			case RAT_STATE_SQUEAK:
				creature->MaxTurn = 0;

				if (Random::TestProbability(RAT_SQUEAK_CHANCE))
					item->Animation.TargetState = RAT_STATE_IDLE;

				break;
			}
		}

		CreatureJoint(item, 0, head);
		CreatureAnimation(itemNumber, angle, 0);
	}
}
