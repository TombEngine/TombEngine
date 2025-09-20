#include "framework.h"
#include "Objects/TR1/Entity/tr1_bear.h"

#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
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

using namespace TEN::Collision::Point;
using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR1
{
	constexpr auto BEAR_RUN_CONTACT_DAMAGE		 = 3;
	constexpr auto BEAR_LOW_CHARGE_ATTACK_DAMAGE = 200;
	constexpr auto BEAR_HIGH_CLAW_ATTACK_DAMAGE	 = 400;
	constexpr auto BEAR_DEATH_CRUSH_DAMAGE		 = 200;

	constexpr auto BEAR_LOW_BITE_ATTACK_RANGE	= SQUARE(BLOCK(3 / 4.0f));
	constexpr auto BEAR_LOW_CHARGE_ATTACK_RANGE = SQUARE(BLOCK(1));
	constexpr auto BEAR_LOW_STANCE_RANGE		= SQUARE(BLOCK(2));
	constexpr auto BEAR_HIGH_STANCE_RANGE		= SQUARE(BLOCK(3 / 5.0f));

	constexpr auto BEAR_ROAR_CHANCE		   = 1 / 400.0f;
	constexpr auto BEAR_LOW_STANCE_CHANCE  = 1 / 22.0f;
	constexpr auto BEAR_HIGH_STANCE_CHANCE = 1 / 40.0f;

	constexpr auto BEAR_WALK_TURN_RATE_MAX = ANGLE(2.0f);
	constexpr auto BEAR_RUN_TURN_RATE_MAX  = ANGLE(5.0f);

	const auto BearBite = CreatureBiteInfo(Vector3(0, 96, 335), 14);
	const auto BearAttackJoints = std::vector<unsigned int>{ 2, 3, 5, 6, 14, 17 };

	enum BearState
	{
		BEAR_STATE_LOW_WALK = 0,
		BEAR_STATE_LOW_IDLE = 1,
		BEAR_STATE_HIGH_WALK = 2,
		BEAR_STATE_LOW_RUN = 3,
		BEAR_STATE_HIGH_IDLE = 4,
		BEAR_STATE_ROAR = 5,
		BEAR_STATE_LOW_CHARGE_ATTACK = 6,
		BEAR_STATE_HIGH_CLAW_ATTACK = 7,
		BEAR_STATE_LOW_BITE_ATTACK = 8,
		BEAR_STATE_DEATH = 9
	};

	const auto BearHighStanceStates = std::vector<int>{ BEAR_STATE_HIGH_WALK, BEAR_STATE_HIGH_IDLE, BEAR_STATE_HIGH_CLAW_ATTACK };

	enum BearAnim
	{
		BEAR_ANIM_LOW_IDLE = 0,
		BEAR_ANIM_LOW_IDLE_TO_RUN = 1,
		BEAR_ANIM_LOW_RUN = 2,
		BEAR_ANIM_LOW_RUN_TO_IDLE = 3,
		BEAR_ANIM_HIGH_IDLE_TO_LOW_IDLE = 4,
		BEAR_ANIM_HIGH_ROAR = 5,
		BEAR_ANIM_HIGH_WALK = 6,
		BEAR_ANIM_HIGH_IDLE = 7,
		BEAR_ANIM_LOW_IDLE_TO_HIGH_IDLE = 8,
		BEAR_ANIM_HIGH_IDLE_TO_WALK = 9,
		BEAR_ANIM_HIGH_WALK_TO_IDLE = 10,
		BEAR_ANIM_LOW_ROAR = 11,
		BEAR_ANIM_LOW_CHARGE_ATTACK_START = 12,
		BEAR_ANIM_LOW_CHARGE_ATTACK_END = 13,
		BEAR_ANIM_HIGH_ATTACK = 14,
		BEAR_ANIM_LOW_BITE_ATTACK = 15,
		BEAR_ANIM_LOW_IDLE_TO_WALK = 16,
		BEAR_ANIM_LOW_WALK = 17,
		BEAR_ANIM_LOW_WALK_TO_IDLE = 18,
		BEAR_ANIM_HIGH_DEATH = 19,
		BEAR_ANIM_LOW_DEATH = 20
	};

	void BearControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto& item = g_Level.Items[itemNumber];
		const auto& object = Objects[item.ObjectNumber];
		auto& creature = *GetCreatureInfo(&item);

		short headingAngle = 0;
		auto extraHeadRot = EulerAngles::Identity;

		if (item.HitPoints <= 0)
		{
			headingAngle = CreatureTurn(&item, ANGLE(1.0f));

			switch (item.Animation.ActiveState)
			{
			// Low stance states.
			case BEAR_STATE_LOW_WALK:
			case BEAR_STATE_LOW_RUN:
				item.Animation.TargetState = BEAR_STATE_LOW_IDLE;
				break;
				
			case BEAR_STATE_LOW_IDLE:
				item.Animation.TargetState = BEAR_STATE_DEATH;
				creature.Flags = 0;
				break;

			// High stance states.
			case BEAR_STATE_HIGH_WALK:
				item.Animation.TargetState = BEAR_STATE_HIGH_IDLE;
				break;
				
			case BEAR_STATE_HIGH_IDLE:
				item.Animation.TargetState = BEAR_STATE_DEATH;
				creature.Flags = 1;
				break;
				
			// Low/high stance states.
			case BEAR_STATE_DEATH:
				if (creature.Flags && item.TouchBits.Test(BearAttackJoints))
				{
					DoDamage(creature.Enemy, BEAR_DEATH_CRUSH_DAMAGE);
					creature.Flags = 0;
				}

				break;
			}
		}
		else
		{
			AI_INFO ai;
			CreatureAIInfo(&item, &ai);

			if (ai.ahead)
				extraHeadRot.y = ai.angle;

			GetCreatureMood(&item, &ai, true);
			CreatureMood(&item, &ai, true);

			headingAngle = CreatureTurn(&item, creature.MaxTurn);

			if (item.HitStatus)
				creature.Flags = 1;

			bool isPlayerDead = LaraItem->HitPoints <= 0;

			auto pointColl = GetPointCollision(item);
			int floorToCeilHeight = abs(pointColl.GetCeilingHeight() - pointColl.GetFloorHeight());

			switch (item.Animation.ActiveState)
			{
			// Low stance states.
			case BEAR_STATE_LOW_WALK:
				creature.MaxTurn = BEAR_WALK_TURN_RATE_MAX;

				if (isPlayerDead && item.TouchBits.Test(BearAttackJoints) && ai.ahead)
				{
					// Go to EAT state.
					item.Animation.TargetState = BEAR_STATE_LOW_IDLE;
				}
				else if (creature.Mood != MoodType::Bored)
				{
					// Go to RUN state.
					item.Animation.TargetState = BEAR_STATE_LOW_IDLE;
				}
				else if (Random::TestProbability(BEAR_ROAR_CHANCE))
				{
					item.Animation.TargetState = BEAR_STATE_LOW_IDLE;
					item.Animation.RequiredState = BEAR_STATE_ROAR;
				}

				break;

			case BEAR_STATE_LOW_IDLE:
				if (isPlayerDead)
				{
					if (ai.bite && ai.distance < BEAR_LOW_BITE_ATTACK_RANGE)
					{
						item.Animation.TargetState = BEAR_STATE_LOW_BITE_ATTACK;
					}
					else
					{
						item.Animation.TargetState = BEAR_STATE_LOW_WALK;
					}
				}
				else if (item.Animation.RequiredState != NO_VALUE)
				{
					item.Animation.TargetState = item.Animation.RequiredState;
				}
				else if (creature.Mood == MoodType::Bored)
				{
					item.Animation.TargetState = BEAR_STATE_LOW_WALK;
				}
				else
				{
					item.Animation.TargetState = BEAR_STATE_LOW_RUN;
				}

				break;

			case BEAR_STATE_LOW_RUN:
				creature.MaxTurn = BEAR_RUN_TURN_RATE_MAX;

				if (item.TouchBits.Test(BearAttackJoints))
					DoDamage(creature.Enemy, BEAR_RUN_CONTACT_DAMAGE);

				if (creature.Mood == MoodType::Bored ||
					isPlayerDead ||
					item.Animation.RequiredState != NO_VALUE)
				{
					// Go to WALK, EAT, or RequiredState state.
					item.Animation.TargetState = BEAR_STATE_LOW_IDLE;
				}
				else if (ai.ahead)
				{
					if (Random::TestProbability(BEAR_HIGH_STANCE_CHANCE))
					{
						if (ai.distance < BEAR_LOW_STANCE_RANGE && creature.Flags == 0 && floorToCeilHeight > CLICK(7))
						{
							item.Animation.TargetState = BEAR_STATE_LOW_IDLE;
							item.Animation.RequiredState = BEAR_STATE_HIGH_IDLE;
						}
					}
					else if (ai.distance < BEAR_LOW_CHARGE_ATTACK_RANGE)
					{
						item.Animation.TargetState = BEAR_STATE_LOW_CHARGE_ATTACK;
					}
				}

				break;

			case BEAR_STATE_LOW_CHARGE_ATTACK:
				if (item.Animation.RequiredState == NO_VALUE &&
					item.TouchBits.Test(BearAttackJoints))
				{
					DoDamage(creature.Enemy, BEAR_LOW_CHARGE_ATTACK_DAMAGE);
					CreatureEffect(&item, BearBite, DoBloodSplat);
					item.Animation.RequiredState = BEAR_STATE_LOW_IDLE;
				}

				break;

			// High stance states.
			case BEAR_STATE_HIGH_WALK:
				if (creature.Flags)
				{
					item.Animation.TargetState = BEAR_STATE_HIGH_IDLE;
					item.Animation.RequiredState = BEAR_STATE_LOW_WALK;
				}
				else if (item.TouchBits.Test(BearAttackJoints))
				{
					// Go to ATTACK state.
					item.Animation.TargetState = BEAR_STATE_HIGH_IDLE;
				}
				else if (creature.Mood == MoodType::Escape)
				{
					item.Animation.TargetState = BEAR_STATE_HIGH_IDLE;
					item.Animation.RequiredState = BEAR_STATE_LOW_RUN;
				}
				// Roar when bored or at random.
				else if (creature.Mood == MoodType::Bored || 
					Random::TestProbability(BEAR_ROAR_CHANCE))
				{
					item.Animation.TargetState = BEAR_STATE_HIGH_IDLE;
					item.Animation.RequiredState = BEAR_STATE_ROAR;
				}
				else if (ai.distance > BEAR_LOW_STANCE_RANGE ||
					floorToCeilHeight <= CLICK(7) ||
					(ai.distance > BEAR_HIGH_STANCE_RANGE && Random::TestProbability(BEAR_LOW_STANCE_CHANCE)))
				{
					item.Animation.TargetState = BEAR_STATE_HIGH_IDLE;
					item.Animation.RequiredState = BEAR_STATE_LOW_IDLE;
				}

				break;

			case BEAR_STATE_HIGH_IDLE:
				if (creature.Flags)
				{
					item.Animation.RequiredState = BEAR_STATE_LOW_WALK;
					item.Animation.TargetState = BEAR_STATE_LOW_IDLE;
				}
				else if (item.Animation.RequiredState != NO_VALUE)
				{
					item.Animation.TargetState = item.Animation.RequiredState;
				}
				else if (creature.Mood == MoodType::Bored ||
					creature.Mood == MoodType::Escape ||
					ai.distance > BEAR_LOW_STANCE_RANGE ||
					floorToCeilHeight <= CLICK(7))
				{
					item.Animation.TargetState = BEAR_STATE_LOW_IDLE;
				}
				else if (abs(LaraItem->Pose.Position.y - item.Pose.Position.y) <= CLICK(2) &&
					item.TouchBits.Test(BearAttackJoints))
				{
					item.Animation.TargetState = BEAR_STATE_HIGH_CLAW_ATTACK;
				}
				else
				{
					item.Animation.TargetState = BEAR_STATE_HIGH_WALK;
				}

				break;

			case BEAR_STATE_HIGH_CLAW_ATTACK:
				if (item.Animation.RequiredState == NO_VALUE &&
					item.TouchBits.Test(BearAttackJoints))
				{
					DoDamage(creature.Enemy, BEAR_HIGH_CLAW_ATTACK_DAMAGE);
					item.Animation.RequiredState = BEAR_STATE_HIGH_IDLE;
				}

				break;
			}
		}

		CreatureJoint(&item, 0, extraHeadRot.y);
		CreatureAnimation(itemNumber, headingAngle, 0);

		// Align with floor.
		if (!TestState(item.Animation.ActiveState, BearHighStanceStates))
		{
			auto radius = Vector2(object.radius);
			AlignEntityToSurface(&item, radius);
		}
	}
}
