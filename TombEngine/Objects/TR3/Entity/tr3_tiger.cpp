#include "framework.h"
#include "Objects/TR3/Entity/tr3_tiger.h"

#include "Game/collision/collide_room.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/itemdata/creature_info.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Specific/level.h"

using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto TIGER_BITE_ATTACK_DAMAGE	 = 90;
	constexpr auto TIGER_SWIPE_ATTACK_DAMAGE = 150;

	constexpr auto TIGER_BITE_ATTACK_RANGE	 = SQUARE(BLOCK(0.4f));
	constexpr auto TIGER_POUNCE_ATTACK_RANGE = SQUARE(BLOCK(0.75f));
	constexpr auto TIGER_RUN_ATTACK_RANGE	 = SQUARE(BLOCK(1.5f));

	constexpr auto TIGER_WALK_CHANCE   = 1 / 32.0f;
	constexpr auto TIGER_ROAR_CHANCE   = 1 / 340.0f;
	constexpr auto TIGER_ATTACK_CHANCE = 4 / 10.0f;

	constexpr auto TIGER_WALK_TURN_RATE_MAX			 = ANGLE(3.0f);
	constexpr auto TIGER_RUN_TURN_RATE_MAX			 = ANGLE(6.0f);
	constexpr auto TIGER_POUNCE_ATTACK_TURN_RATE_MAX = ANGLE(3.0f);

	constexpr auto TIGER_PLAYER_ALERT_VELOCITY = 10.0f;

	const auto TigerBite = CreatureBiteInfo(Vector3(19, -13, 3), 26);
	const auto TigerSwipeAttackJoints = std::vector<unsigned int>{ 14, 15, 16 };
	const auto TigerBiteAttackJoints  = std::vector<unsigned int>{ 22, 25, 26 };

	enum TigerState
	{
		TIGER_STATE_DEATH = 0,
		TIGER_STATE_IDLE = 1,
		TIGER_STATE_WALK_FORWARD = 2,
		TIGER_STATE_RUN_FORWARD = 3,
		// No state 4.
		TIGER_STATE_ROAR = 5,
		TIGER_STATE_BITE_ATTACK = 6,
		TIGER_STATE_RUN_SWIPE_ATTACK = 7,
		TIGER_STATE_POUNCE_ATTACK = 8
	};

	enum TigerAnim
	{
		TIGER_ANIM_IDLE_TO_RUN_FORWARD = 0,
		TIGER_ANIM_BITE_ATTACK = 1,
		TIGER_ANIM_RUN_SWIPE_ATTACK = 2,
		TIGER_ANIM_POUNCE_ATTACK_START = 3,
		TIGER_ANIM_ROAR = 4,
		TIGER_ANIM_RUN_FORWARD = 5,
		TIGER_ANIM_RUN_FORWARD_TO_IDLE = 6,
		TIGER_ANIM_IDLE = 7,
		TIGER_ANIM_WALK_FORWARD = 8,
		TIGER_ANIM_IDLE_TO_WALK_FORWARD = 9,
		TIGER_ANIM_WALK_FORWARD_TO_IDLE = 10,
		TIGER_ANIM_DEATH = 11,
		TIGER_ANIM_POUNCE_ATTACK_END = 12
	};

	void TigerControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* object = &Objects[item->ObjectNumber];
		auto* creature = GetCreatureInfo(item);

		short headingAngle = 0;
		short tiltAngle = 0;
		auto extraHeadRot = EulerAngles::Identity;

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != TIGER_STATE_DEATH)
				SetAnimation(*item, TIGER_ANIM_DEATH);
		}
		else
		{
			AI_INFO ai;
			CreatureAIInfo(item, &ai);

			if (ai.ahead)
				extraHeadRot.y = ai.angle;

			GetCreatureMood(item, &ai, true);

			if (creature->Alerted && ai.zoneNumber != ai.enemyZone)
				creature->Mood = MoodType::Escape;

			CreatureMood(item, &ai, true);

			headingAngle = CreatureTurn(item, creature->MaxTurn);

			switch (item->Animation.ActiveState)
			{
			case TIGER_STATE_IDLE:
				creature->MaxTurn = 0;
				creature->Flags = 0;

				if (item->Animation.RequiredState != NO_VALUE)
				{
					item->Animation.TargetState = item->Animation.RequiredState;
				}
				else if (creature->Mood == MoodType::Escape)
				{
					if (Lara.TargetEntity != item && ai.ahead)
						item->Animation.TargetState = TIGER_STATE_IDLE;
					else
						item->Animation.TargetState = TIGER_STATE_RUN_FORWARD;
				}
				else if (creature->Mood == MoodType::Bored)
				{
					if (Random::TestProbability(TIGER_ROAR_CHANCE))
					{
						item->Animation.TargetState = TIGER_STATE_ROAR;
					}
					else if (Random::TestProbability(TIGER_WALK_CHANCE))
					{
						item->Animation.TargetState = TIGER_STATE_WALK_FORWARD;
					}
				}
				else if (ai.bite && ai.distance < TIGER_BITE_ATTACK_RANGE)
				{
					item->Animation.TargetState = TIGER_STATE_BITE_ATTACK;
				}
				else if (ai.bite && ai.distance < TIGER_POUNCE_ATTACK_RANGE)
				{
					item->Animation.TargetState = TIGER_STATE_POUNCE_ATTACK;
					creature->MaxTurn = TIGER_POUNCE_ATTACK_TURN_RATE_MAX;
				}
				else if (creature->Mood != MoodType::Attack && Random::TestProbability(TIGER_ROAR_CHANCE))
				{
					item->Animation.TargetState = TIGER_STATE_ROAR;
				}
				else
				{
					item->Animation.TargetState = TIGER_STATE_RUN_FORWARD;
				}
					
				break;

			case TIGER_STATE_WALK_FORWARD:
				creature->MaxTurn = TIGER_WALK_TURN_RATE_MAX;

				if (creature->Mood == MoodType::Escape ||
					creature->Mood == MoodType::Attack)
				{
					item->Animation.TargetState = TIGER_STATE_RUN_FORWARD;
				}
				else if (Random::TestProbability(TIGER_ROAR_CHANCE))
				{
					item->Animation.TargetState = TIGER_STATE_IDLE;
					item->Animation.RequiredState = TIGER_STATE_ROAR;
				}

				break;

			case TIGER_STATE_RUN_FORWARD:
				creature->MaxTurn = TIGER_RUN_TURN_RATE_MAX;

				if (creature->Mood == MoodType::Bored)
				{
					item->Animation.TargetState = TIGER_STATE_IDLE;
				}
				else if (creature->Flags && ai.ahead)
				{
					item->Animation.TargetState = TIGER_STATE_IDLE;
				}
				else if (ai.bite && ai.distance < TIGER_RUN_ATTACK_RANGE)
				{
					if (LaraItem->Animation.Velocity.z <= TIGER_PLAYER_ALERT_VELOCITY &&
						Random::TestProbability(TIGER_ATTACK_CHANCE))
					{
						item->Animation.TargetState = TIGER_STATE_IDLE;
					}
					else if (ai.ahead)
					{
						item->Animation.TargetState = TIGER_STATE_RUN_SWIPE_ATTACK;
					}
					else
					{
						item->Animation.TargetState = TIGER_STATE_RUN_FORWARD;
					}
				}
				else if (creature->Mood != MoodType::Attack && Random::TestProbability(TIGER_ROAR_CHANCE))
				{
					item->Animation.TargetState = TIGER_STATE_ROAR;
				}
				else if (creature->Mood == MoodType::Escape && Lara.TargetEntity != item && ai.ahead)
				{
					item->Animation.TargetState = TIGER_STATE_IDLE;
				}

				creature->Flags = 0;
				break;

			case TIGER_STATE_BITE_ATTACK:
			case TIGER_STATE_POUNCE_ATTACK:
				if (!creature->Flags && item->TouchBits.Test(TigerBiteAttackJoints))
				{
					if ((item->Animation.AnimNumber == TIGER_ANIM_BITE_ATTACK &&
							item->Animation.FrameNumber > 4) ||
						(item->Animation.AnimNumber == TIGER_ANIM_POUNCE_ATTACK_START &&
							item->Animation.FrameNumber > 12))
					{
						DoDamage(creature->Enemy, TIGER_BITE_ATTACK_DAMAGE);
						CreatureEffect(item, TigerBite, DoBloodSplat);
						creature->Flags = 1; // Flag 1 = is attacking.
					}
				}

				break;

			case TIGER_STATE_RUN_SWIPE_ATTACK:
				if (!creature->Flags && item->TouchBits.Test(TigerSwipeAttackJoints))
				{
					if (item->Animation.AnimNumber == TIGER_ANIM_RUN_SWIPE_ATTACK &&
						TestAnimFrameRange(*item, 6, 15))
					{
						DoDamage(creature->Enemy, TIGER_SWIPE_ATTACK_DAMAGE);
						CreatureEffect(item, TigerBite, DoBloodSplat);
						creature->Flags = 1; // 1 = is attacking.
					}
				}

				break;
			}
		}

		CreatureTilt(item, tiltAngle);
		CreatureJoint(item, 0, extraHeadRot.y);
		CreatureAnimation(itemNumber, headingAngle, tiltAngle);

		auto radius = Vector2(object->radius);
		AlignEntityToSurface(item, radius);
	}
}
