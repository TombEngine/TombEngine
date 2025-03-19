#include "framework.h"
#include "Objects/TR4/Entity/tr4_big_beetle.h"

#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Specific/level.h"
#include "Math/Math.h"

using namespace TEN::Math;

namespace TEN::Entities::TR4
{
	constexpr auto BIG_BEETLE_ATTACK_DAMAGE = 50;

	constexpr auto BIG_BEETLE_ATTACK_RANGE = SQUARE(CLICK(1));
	constexpr auto BIG_BEETLE_AWARE_RANGE  = SQUARE(CLICK(12));

	const auto BigBeetleBite = CreatureBiteInfo(Vector3::Zero, 12);
	const auto BigBeetleAttackJoints = std::vector<unsigned int>{ 5, 6 };

	enum BigBeetleState
	{
		// No state 0.
		BBEETLE_STATE_IDLE = 1,
		BBEETLE_STATE_TAKE_OFF = 2,
		BBEETLE_STATE_FLY_FORWARD = 3,
		BBEETLE_STATE_ATTACK = 4,
		BBEETLE_STATE_LAND = 5,
		BBEETLE_STATE_DEATH_START = 6,
		BBEETLE_STATE_DEATH_FALL = 7,
		BBEETLE_STATE_DEATH_END = 8,
		BBEETLE_STATE_FLY_IDLE = 9,
	};

	enum BigBeetleAnim
	{
		BBEETLE_ANIM_IDLE_TO_FLY_FORWARD = 0,
		BBEETLE_ANIM_FLY_FORWARD = 1,
		BBEETLE_ANIM_FLY_FORWARD_TO_IDLE = 2,
		BBEETLE_ANIM_IDLE = 3,
		BBEETLE_ANIM_ATTACK = 4,
		BBEETLE_ANIM_DEATH_START = 5,
		BBEETLE_ANIM_DEATH_FALL = 6,
		BBEETLE_ANIM_DEATH_END = 7,
		BBEETLE_ANIM_FLY_IDLE = 8,
		BBEETLE_ANIM_FLY_FORWARD_TO_FLY_IDLE = 9,
		BBEETLE_ANIM_FLY_IDLE_TO_FLY_FORWARD = 10,
	};

	// TODO
	enum BigBeetleFlags
	{

	};

	void InitializeBigBeetle(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(*item, BBEETLE_ANIM_IDLE);
	}

	void BigBeetleControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != BBEETLE_STATE_DEATH_START)
			{
				if (item->Animation.ActiveState != BBEETLE_STATE_DEATH_FALL)
				{
					if (item->Animation.ActiveState == BBEETLE_STATE_DEATH_END)
					{
						item->Pose.Position.y = item->Floor;
						item->Pose.Orientation.x = 0;
					}
					else
					{
						SetAnimation(*item, BBEETLE_ANIM_DEATH_START);
						item->Animation.IsAirborne = true;
						item->Animation.Velocity.z = 0;
						item->Pose.Orientation.x = 0;
					}
				}
				else if (item->Pose.Position.y >= item->Floor)
				{
					item->Animation.TargetState = BBEETLE_STATE_DEATH_END;
					item->Animation.IsAirborne = false;
					item->Animation.Velocity.y = 0;
					item->Pose.Position.y = item->Floor;
				}
			}

			item->Pose.Orientation.x = 0;
		}
		else
		{
			AI_INFO AI;
			CreatureAIInfo(item, &AI);
			GetCreatureMood(item, &AI, true);

			if (creature->Flags)
				creature->Mood = MoodType::Escape;

			CreatureMood(item, &AI, true);
			angle = CreatureTurn(item, creature->MaxTurn);

			if (item->HitStatus || AI.distance > BIG_BEETLE_AWARE_RANGE ||
				Random::TestProbability(1 / 128.0f))
			{
				creature->Flags = 0;
			}

			switch (item->Animation.ActiveState)
			{
			case BBEETLE_STATE_IDLE:
				creature->MaxTurn = ANGLE(1.0f);
				item->Pose.Position.y = item->Floor;

				if (item->HitStatus ||
					item->AIBits == MODIFY ||
					creature->HurtByLara ||
					AI.distance < BIG_BEETLE_AWARE_RANGE)
				{
					item->Animation.TargetState = BBEETLE_STATE_TAKE_OFF;
				}

				break;

			case BBEETLE_STATE_FLY_FORWARD:
				creature->MaxTurn = ANGLE(7.0f);

				if (item->Animation.RequiredState != NO_VALUE)
					item->Animation.TargetState = item->Animation.RequiredState;
				else if (AI.ahead && AI.distance < BIG_BEETLE_ATTACK_RANGE)
					item->Animation.TargetState = BBEETLE_STATE_FLY_IDLE;

				break;

			case BBEETLE_STATE_ATTACK:
				creature->MaxTurn = ANGLE(7.0f);

				if (AI.ahead)
				{
					if (AI.distance < BIG_BEETLE_ATTACK_RANGE)
						item->Animation.TargetState = BBEETLE_STATE_ATTACK;
					else
					{
						item->Animation.RequiredState = BBEETLE_STATE_FLY_FORWARD;
						item->Animation.TargetState = BBEETLE_STATE_FLY_IDLE;
					}
				}
				else if (AI.distance < BIG_BEETLE_ATTACK_RANGE)
					item->Animation.TargetState = BBEETLE_STATE_FLY_IDLE;
				else
				{
					item->Animation.RequiredState = BBEETLE_STATE_FLY_FORWARD;
					item->Animation.TargetState = BBEETLE_STATE_FLY_IDLE;
				}

				if (!creature->Flags &&
					item->TouchBits.Test(BigBeetleAttackJoints))
				{
					DoDamage(creature->Enemy, BIG_BEETLE_ATTACK_DAMAGE);
					CreatureEffect2(item, BigBeetleBite, 5, -1, DoBloodSplat);
					creature->Flags = 1;
				}

				break;

			case BBEETLE_STATE_LAND:
				item->Pose.Position.y += 51;
				creature->Flags = 0;

				if (item->Pose.Position.y > item->Floor)
					item->Pose.Position.y = item->Floor;

				break;

			case BBEETLE_STATE_FLY_IDLE:
				creature->MaxTurn = ANGLE(7.0f);

				if (item->Animation.RequiredState != NO_VALUE)
					item->Animation.TargetState = item->Animation.RequiredState;
				else if (!item->HitStatus && item->AIBits != MODIFY &&
					Random::TestProbability(0.99f) &&
					((creature->Mood != MoodType::Bored && Random::TestProbability(0.996f)) ||
						creature->HurtByLara || item->AIBits == MODIFY))
				{
					if (AI.ahead)
					{
						if (AI.distance < BIG_BEETLE_ATTACK_RANGE && !creature->Flags)
							item->Animation.TargetState = BBEETLE_STATE_ATTACK;
					}
				}
				else
					item->Animation.TargetState = BBEETLE_STATE_FLY_FORWARD;

				break;

			default:
				break;
			}
		}

		CreatureTilt(item, angle * 2);
		CreatureAnimation(itemNumber, angle, angle);
	}
}
