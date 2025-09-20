#include "framework.h"
#include "Objects/TR1/Entity/tr1_wolf.h"

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

namespace TEN::Entities::Creatures::TR1
{
	constexpr auto WOLF_BITE_ATTACK_DAMAGE = 100;
	constexpr auto WOLF_JUMP_ATTACK_DAMAGE = 50;

	constexpr auto WOLF_JUMP_ATTACK_RANGE = SQUARE(BLOCK(1.5f));
	constexpr auto WOLF_BITE_ATTACK_RANGE = SQUARE(BLOCK(1 / 3.0f));
	constexpr auto WOLF_STALK_RANGE		  = SQUARE(BLOCK(2));
	constexpr auto WOLF_RUN_RANGE		  = SQUARE(BLOCK(3));

	constexpr auto WOLF_WAKE_CHANCE	 = 1 / 1024.0f;
	constexpr auto WOLF_SLEEP_CHANCE = 1 / 1024.0f;
	constexpr auto WOLF_HOWL_CHANCE  = 1 / 85.0f;

	constexpr auto WOLF_WALK_TURN_RATE_MAX	= ANGLE(2.0f);
	constexpr auto WOLF_RUN_TURN_RATE_MAX	= ANGLE(5.0f);
	constexpr auto WOLF_STALK_TURN_RATE_MAX = ANGLE(2.0f);

	constexpr auto WOLF_SLEEP_FRAME = 96;

	const auto WolfBite = CreatureBiteInfo(Vector3(0, -14, 174), 6);
	const auto WolfAttackJoints = std::vector<unsigned int>{ 0, 1, 2, 3, 6, 8, 9, 10, 12, 13, 14 };

	enum WolfState
	{
		// No state 0.
		WOLF_STATE_IDLE = 1,
		WOLF_STATE_WALK = 2,
		WOLF_STATE_RUN = 3,
		WOLF_STATE_JUMP = 4,
		WOLF_STATE_STALK = 5,
		WOLF_STATE_JUMP_ATTACK = 6,
		WOLF_STATE_HOWL = 7,
		WOLF_STATE_SLEEP = 8,
		WOLF_STATE_CROUCH = 9,
		WOLF_STATE_FAST_TURN = 10,
		WOLF_STATE_DEATH = 11,
		WOLF_STATE_BITE_ATTACK = 12
	};

	enum WolfAnim
	{
		WOLF_ANIM_SLEEP = 0,
		WOLF_ANIM_AWAKEN = 1,
		WOLF_ANIM_IDLE_TO_WALK_FORWARD = 2,
		WOLF_ANIM_WALK_FORWARD = 3,
		WOLF_ANIM_WALK_FORWARD_TO_STALK = 4,
		WOLF_ANIM_STALK_FORWARD = 5,
		WOLF_ANIM_STALK_FORWARD_TO_RUN_FORWARD = 6,
		WOLF_ANIM_RUN_FORWARD = 7,
		WOLF_ANIM_IDLE = 8,
		WOLF_ANIM_JUMP_ATTACK = 9,
		WOLF_ANIM_STALK_IDLE_TO_STALK_FORWARD = 10,
		WOLF_ANIM_STALK_FORWARD_TO_STALK_IDLE_START = 11,
		WOLF_ANIM_STALK_FORWARD_TO_STALK_IDLE_END = 12,
		WOLF_ANIM_STALK_IDLE_TO_RUN_FORWARD = 13,
		WOLF_ANIM_STALK_IDLE = 14,
		WOLF_ANIM_RUN_FORWARD_TO_STALK_IDLE = 15,
		WOLF_ANIM_HOWL = 16,
		WOLF_ANIM_IDLE_TO_STALK_IDLE = 17,
		WOLF_ANIM_RUN_FORWARD_RIGHT = 18, // Unused.
		WOLF_ANIM_WALK_FORWARD_TO_IDLE = 19,
		WOLF_ANIM_DEATH_1 = 20,
		WOLF_ANIM_DEATH_2 = 21,
		WOLF_ANIM_DEATH_3 = 22,
		WOLF_ANIM_BITE_ATTACK = 23,
		WOLF_ANIM_STALK_IDLE_TO_IDLE = 24
	};

	const auto WolfDeathAnims = std::vector<int>{ WOLF_ANIM_DEATH_1, WOLF_ANIM_DEATH_2, WOLF_ANIM_DEATH_3 };

	void InitializeWolf(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);

		if (item.TriggerFlags == 1)
		{
			item.Animation.FrameNumber = WOLF_SLEEP_FRAME;
		}
		else
		{
			SetAnimation(item, WOLF_ANIM_WALK_FORWARD);
		}
	}

	void WolfControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto& item = g_Level.Items[itemNumber];
		const auto& object = Objects[item.ObjectNumber];
		auto& creature = *GetCreatureInfo(&item);

		short headingAngle = 0;
		short tiltAngle = 0;
		auto extraHeadRot = EulerAngles::Identity;

		if (item.HitPoints <= 0)
		{
			if (item.Animation.ActiveState != WOLF_STATE_DEATH)
				SetAnimation(item, WolfDeathAnims[Random::GenerateInt(0, (int)WolfDeathAnims.size() - 1)]);
		}
		else
		{
			AI_INFO ai;
			CreatureAIInfo(&item, &ai);

			if (ai.ahead)
				extraHeadRot.y = ai.angle;

			GetCreatureMood(&item, &ai, false);
			CreatureMood(&item, &ai, false);

			if (item.Animation.ActiveState != WOLF_STATE_SLEEP)
				headingAngle = CreatureTurn(&item, creature.MaxTurn);

			switch (item.Animation.ActiveState)
			{
			case WOLF_STATE_SLEEP:
				extraHeadRot.y = 0;

				if (creature.Mood == MoodType::Escape || ai.zoneNumber == ai.enemyZone)
				{
					item.Animation.TargetState = WOLF_STATE_IDLE;
					item.Animation.RequiredState = WOLF_STATE_CROUCH;
				}
				else if (Random::TestProbability(WOLF_WAKE_CHANCE))
				{
					item.Animation.TargetState = WOLF_STATE_IDLE;
					item.Animation.RequiredState = WOLF_STATE_WALK;
				}

				break;

			case WOLF_STATE_IDLE:
				if (item.Animation.RequiredState != NO_VALUE)
				{
					item.Animation.TargetState = item.Animation.RequiredState;
				}
				else
				{
					item.Animation.TargetState = WOLF_STATE_WALK;
				}

				break;

			case WOLF_STATE_WALK:
				creature.MaxTurn = WOLF_WALK_TURN_RATE_MAX;

				if (creature.Mood != MoodType::Bored)
				{
					item.Animation.TargetState = WOLF_STATE_STALK;
					item.Animation.RequiredState = NO_VALUE;
				}
				else if (Random::TestProbability(WOLF_SLEEP_CHANCE))
				{
					item.Animation.TargetState = WOLF_STATE_IDLE;
					item.Animation.RequiredState = WOLF_STATE_SLEEP;
				}

				break;

			case WOLF_STATE_CROUCH:
				if (item.Animation.RequiredState != NO_VALUE)
				{
					item.Animation.TargetState = item.Animation.RequiredState;
				}
				else if (creature.Mood == MoodType::Escape)
				{
					item.Animation.TargetState = WOLF_STATE_RUN;
				}
				else if (ai.distance < WOLF_BITE_ATTACK_RANGE && ai.bite)
				{
					item.Animation.TargetState = WOLF_STATE_BITE_ATTACK;
				}
				else if (creature.Mood == MoodType::Stalk)
				{
					item.Animation.TargetState = WOLF_STATE_STALK;
				}
				else if (creature.Mood == MoodType::Bored)
				{
					item.Animation.TargetState = WOLF_STATE_IDLE;
				}
				else
				{
					item.Animation.TargetState = WOLF_STATE_RUN;
				}

				break;

			case WOLF_STATE_STALK:
				creature.MaxTurn = WOLF_STALK_TURN_RATE_MAX;

				if (creature.Mood == MoodType::Escape)
				{
					item.Animation.TargetState = WOLF_STATE_RUN;
				}
				else if (ai.distance < WOLF_BITE_ATTACK_RANGE && ai.bite)
				{
					item.Animation.TargetState = WOLF_STATE_BITE_ATTACK;
				}
				else if (ai.distance > WOLF_RUN_RANGE)
				{
					item.Animation.TargetState = WOLF_STATE_RUN;
				}
				else if (creature.Mood == MoodType::Attack)
				{
					if (!ai.ahead || ai.distance > WOLF_JUMP_ATTACK_RANGE ||
						(ai.enemyFacing < FRONT_ARC && ai.enemyFacing > -FRONT_ARC))
					{
						item.Animation.TargetState = WOLF_STATE_RUN;
					}
				}
				else if (Random::TestProbability(WOLF_HOWL_CHANCE))
				{
					item.Animation.TargetState = WOLF_STATE_CROUCH;
					item.Animation.RequiredState = WOLF_STATE_HOWL;
				}
				else if (creature.Mood == MoodType::Bored)
				{
					item.Animation.TargetState = WOLF_STATE_CROUCH;
				}

				break;

			case WOLF_STATE_RUN:
				creature.MaxTurn = WOLF_RUN_TURN_RATE_MAX;
				tiltAngle = headingAngle;

				if (ai.ahead && ai.distance < WOLF_JUMP_ATTACK_RANGE)
				{
					if (ai.distance > (WOLF_JUMP_ATTACK_RANGE / 2) &&
						(ai.enemyFacing > FRONT_ARC || ai.enemyFacing < -FRONT_ARC))
					{
						item.Animation.TargetState = WOLF_STATE_CROUCH;
						item.Animation.RequiredState = WOLF_STATE_STALK;
					}
					else
					{
						item.Animation.TargetState = WOLF_STATE_JUMP_ATTACK;
						item.Animation.RequiredState = NO_VALUE;
					}
				}
				else if (creature.Mood == MoodType::Stalk &&
					ai.distance < WOLF_STALK_RANGE)
				{
					item.Animation.TargetState = WOLF_STATE_CROUCH;
					item.Animation.RequiredState = WOLF_STATE_STALK;
				}
				else if (creature.Mood == MoodType::Bored)
				{
					item.Animation.TargetState = WOLF_STATE_CROUCH;
				}

				break;

			case WOLF_STATE_JUMP_ATTACK:
				tiltAngle = headingAngle;

				if (item.Animation.RequiredState == NO_VALUE &&
					item.TouchBits.Test(WolfAttackJoints))
				{
					item.Animation.RequiredState = WOLF_STATE_RUN;
					DoDamage(creature.Enemy, WOLF_JUMP_ATTACK_DAMAGE);
					CreatureEffect(&item, WolfBite, DoBloodSplat);
				}

				item.Animation.TargetState = WOLF_STATE_RUN;
				break;

			case WOLF_STATE_BITE_ATTACK:
				if (ai.ahead && item.Animation.RequiredState == NO_VALUE &&
					item.TouchBits.Test(WolfAttackJoints))
				{
					item.Animation.RequiredState = WOLF_STATE_CROUCH;
					DoDamage(creature.Enemy, WOLF_BITE_ATTACK_DAMAGE);
					CreatureEffect(&item, WolfBite, DoBloodSplat);
				}

				break;
			}
		}

		CreatureTilt(&item, tiltAngle);
		CreatureJoint(&item, 0, extraHeadRot.y);
		CreatureAnimation(itemNumber, headingAngle, tiltAngle);

		auto radius = Vector2(object.radius);
		AlignEntityToSurface(&item, radius);
	}
}
