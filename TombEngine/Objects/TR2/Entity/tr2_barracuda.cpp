#include "framework.h"
#include "Objects/TR2/Entity/tr2_barracuda.h"

#include "Game/control/box.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Specific/level.h"

namespace TEN::Entities::Creatures::TR2
{
	constexpr auto BARRACUDA_ATTACK_DAMAGE = 100;
	constexpr auto BARRACUDA_IDLE_ATTACK_RANGE		= SQUARE(BLOCK(0.67f));
	constexpr auto BARRACUDA_SWIM_FAST_ATTACK_RANGE = SQUARE(BLOCK(0.34f));

	const auto BarracudaBite = CreatureBiteInfo(Vector3(2, -60, 121), 7);
	const auto BarracudaAttackJoints = std::vector<unsigned int>{ 5, 6, 7 };

	enum BarracudaState
	{
		// No state 0.
		BARRACUDA_STATE_IDLE = 1,
		BARRACUDA_STATE_SWIM_SLOW = 2,
		BARRACUDA_STATE_SWIM_FAST = 3,
		BARRACUDA_STATE_IDLE_ATTACK = 4,
		BARRACUDA_STATE_SWIM_FAST_ATTACK = 5,
		BARRACUDA_STATE_DEATH = 6,
	};

	enum BarracudaAnim
	{
		BARRACUDA_ANIM_SWIM_FAST_ATTACK_LEFT_END = 0,
		BARRACUDA_ANIM_SWIM_FAST_ATTACK_RIGHT_END = 1,
		BARRACUDA_ANIM_IDLE_ATTACK_END = 2,
		BARRACUDA_ANIM_IDLE_ATTACK_CONTINUE = 3,
		BARRACUDA_ANIM_SWIM_FAST_ATTACK_LEFT_CONTINUE = 4,
		BARRACUDA_ANIM_SWIM_FAST_ATTACK_RIGHT_CONTINUE = 5,
		BARRACUDA_ANIM_DEATH_START = 6,
		BARRACUDA_ANIM_DEATH_END = 7,
		BARRACUDA_ANIM_IDLE_ATTACK_START = 8,
		BARRACUDA_ANIM_IDLE_TO_SWIM_SLOW = 9,
		BARRACUDA_ANIM_IDLE_TO_SWIM_FAST = 10,
		BARRACUDA_ANIM_IDLE = 11,
		BARRACUDA_ANIM_SWIM_SLOW = 12,
		BARRACUDA_ANIM_SWIM_FAST = 13,
		BARRACUDA_ANIM_SWIM_SLOW_TO_IDLE = 13,
		BARRACUDA_ANIM_SWIM_SLOW_TO_FAST = 14,
		BARRACUDA_ANIM_SWIM_FAST_ATTACK_LEFT_START = 16,
		BARRACUDA_ANIM_SWIM_FAST_ATTACK_RIGHT_START = 17,
		BARRACUDA_ANIM_SWIM_FAST_TO_IDLE = 18,
		BARRACUDA_ANIM_SWIM_FAST_TO_SLOW = 19
	};

	void BarracudaControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short head = 0;

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != BARRACUDA_STATE_DEATH)
			{
				item->Animation.AnimNumber = BARRACUDA_ANIM_DEATH_START;
				item->Animation.FrameNumber = 0;
				item->Animation.ActiveState = BARRACUDA_STATE_DEATH;
			}

			CreatureFloat(itemNumber);
			return;
		}
		else
		{
			AI_INFO AI;
			CreatureAIInfo(item, &AI);

			GetCreatureMood(item, &AI, false);
			CreatureMood(item, &AI, false);

			angle = CreatureTurn(item, creature->MaxTurn);

			switch (item->Animation.ActiveState)
			{
			case BARRACUDA_STATE_IDLE:
				creature->Flags = 0;

				if (creature->Mood == MoodType::Bored)
					item->Animation.TargetState = BARRACUDA_STATE_SWIM_SLOW;
				else if (AI.ahead && AI.distance < BARRACUDA_IDLE_ATTACK_RANGE)
					item->Animation.TargetState = BARRACUDA_STATE_IDLE_ATTACK;
				else if (creature->Mood == MoodType::Stalk)
					item->Animation.TargetState = BARRACUDA_STATE_SWIM_SLOW;
				else
					item->Animation.TargetState = BARRACUDA_STATE_SWIM_FAST;

				break;

			case BARRACUDA_STATE_SWIM_SLOW:
				creature->MaxTurn = ANGLE(2.0f);

				if (creature->Mood == MoodType::Bored)
					break;
				else if (AI.ahead && item->TouchBits.Test(BarracudaAttackJoints))
					item->Animation.TargetState = BARRACUDA_STATE_IDLE;
				else if (creature->Mood != MoodType::Stalk)
					item->Animation.TargetState = BARRACUDA_STATE_SWIM_FAST;

				break;

			case BARRACUDA_STATE_SWIM_FAST:
				creature->MaxTurn = ANGLE(4.0f);
				creature->Flags = 0;

				if (creature->Mood == MoodType::Bored)
					item->Animation.TargetState = BARRACUDA_STATE_SWIM_SLOW;
				else if (AI.ahead && AI.distance < BARRACUDA_SWIM_FAST_ATTACK_RANGE)
					item->Animation.TargetState = BARRACUDA_STATE_SWIM_FAST_ATTACK;
				else if (AI.ahead && AI.distance < BARRACUDA_IDLE_ATTACK_RANGE)
					item->Animation.TargetState = BARRACUDA_STATE_IDLE;
				else if (creature->Mood == MoodType::Stalk)
					item->Animation.TargetState = BARRACUDA_STATE_SWIM_SLOW;

				break;

			case BARRACUDA_STATE_IDLE_ATTACK:
			case BARRACUDA_STATE_SWIM_FAST_ATTACK:
				if (AI.ahead)
					head = AI.angle;

				if (item->TouchBits.Test(BarracudaAttackJoints) &&
					!creature->Flags)
				{
					DoDamage(creature->Enemy, BARRACUDA_ATTACK_DAMAGE);
					CreatureEffect(item, BarracudaBite, DoBloodSplat);
					creature->Flags = 1;
				}

				break;
			}
		}

		CreatureJoint(item, 0, head);
		CreatureAnimation(itemNumber, angle, 0);
		CreatureUnderwater(item, CLICK(1));
	}
}
