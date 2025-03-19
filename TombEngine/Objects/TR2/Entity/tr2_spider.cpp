#include "framework.h"
#include "Objects/TR2/Entity/tr2_spider.h"

#include "Game/Animation/Animation.h"
#include "Game/control/control.h"
#include "Game/control/box.h"
#include "Game/control/lot.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Game/itemdata/creature_info.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Specific/level.h"

using namespace TEN::Animation;

namespace TEN::Entities::Creatures::TR2
{
	constexpr auto SMALL_SPIDER_ATTACK_DAMAGE = 25;
	constexpr auto BIG_SPIDER_ATTACK_DAMAGE	  = 100;

	constexpr auto SMALL_SPIDER_SHORT_JUMP_RANGE = SQUARE(BLOCK(0.2f));
	constexpr auto SMALL_SPIDER_LONG_JUMP_RANGE	 = SQUARE(BLOCK(0.5f));
	constexpr auto BIG_SPIDER_IDLE_RANGE		 = SQUARE(BLOCK(0.75f)) + 15;

	constexpr auto SMALL_SPIDER_IDLE_CHANCE = 1 / 128.0f;
	constexpr auto BIG_SPIDER_IDLE_CHANCE	= 1 / 64.0f;

	constexpr auto SMALL_SPIDER_TURN_RATE_MAX = ANGLE(8.0f);
	constexpr auto BIG_SPIDER_TURN_RATE_MAX	  = ANGLE(4.0f);

	const auto SpiderBite = CreatureBiteInfo(Vector3(0, 0, 41), 1);

	enum SpiderState
	{
		// No state 0.
		SPIDER_STATE_IDLE = 1,
		SPIDER_STATE_WALK_FORWARD = 2,
		SPIDER_STATE_RUN_FORWARD = 3,
		SPIDER_STATE_IDLE_ATTACK = 4,
		SPIDER_STATE_LONG_JUMP_ATTACK = 5,	// Unused by big spider.
		SPIDER_STATE_SHORT_JUMP_ATTACK = 6, // Unused by big spider.
		SPIDER_STATE_DEATH = 7
	};

	enum SmallSpiderAnim
	{
		SMALL_SPIDER_ANIM_IDLE_ATTACK_END = 0,
		SMALL_SPIDER_ANIM_IDLE_ATTACK_CONTINUE = 1,
		SMALL_SPIDER_ANIM_SHORT_JUMP_ATTACK = 2,
		SMALL_SPIDER_ANIM_LONG_JUMP_ATTACK = 3,
		SMALL_SPIDER_ANIM_DEATH = 4,
		SMALL_SPIDER_ANIM_IDLE_ATTACK_START = 5,
		SMALL_SPIDER_ANIM_IDLE = 6,
		SMALL_SPIDER_ANIM_WALK_FORWARD = 7,
		SMALL_SPIDER_ANIM_RUN_FORWARD = 8
	};

	enum BigSpiderAnim
	{
		BIG_SPIDER_ANIM_IDLE_ATTACK_END = 0,
		BIG_SPIDER_ANIM_IDLE_ATTACK_CONTINUE = 1,
		BIG_SPIDER_ANIM_DEATH = 2,
		BIG_SPIDER_ANIM_IDLE_ATTACK_START = 3,
		BIG_SPIDER_ANIM_IDLE = 4,
		BIG_SPIDER_ANIM_WALK_FORWARD = 5,
		BIG_SPIDER_ANIM_RUN_FORWARD = 6,
		BIG_SPIDER_WALK_FORWARD_TO_IDLE = 7,
		BIG_SPIDER_RUN_FORWARD_TO_IDLE = 8
	};

	void DoSpiderBloodEffect(ItemInfo& item)
	{
		auto pos = GetJointPosition(&item, SpiderBite);
		DoBloodSplat(pos.x, pos.y, pos.z, 10, item.Pose.Position.y, item.RoomNumber);
	}

	void SmallSpiderControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short headingAngle = 0;

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != SPIDER_STATE_DEATH)
			{
				SoundEffect(SFX_TR2_SPIDER_EXPLODE, &item->Pose);
				SetAnimation(*item, SMALL_SPIDER_ANIM_DEATH);
				ExplodingDeath(itemNumber, 0);
				DisableEntityAI(itemNumber);
				KillItem(itemNumber);
			}
		}
		else
		{
			AI_INFO ai;
			CreatureAIInfo(item, &ai);

			GetCreatureMood(item, &ai, true);
			CreatureMood(item, &ai, true);
			headingAngle = CreatureTurn(item, creature->MaxTurn);

			switch (item->Animation.ActiveState)
			{
			case SPIDER_STATE_IDLE:
				creature->MaxTurn = 0;
				creature->Flags = 0;

				if (creature->Mood == MoodType::Bored)
				{
					if (Random::TestProbability(SMALL_SPIDER_IDLE_CHANCE))
						item->Animation.TargetState = SPIDER_STATE_WALK_FORWARD;
				}
				else if (ai.ahead && item->TouchBits.TestAny())
				{
					item->Animation.TargetState = SPIDER_STATE_IDLE_ATTACK;
				}
				else if (creature->Mood == MoodType::Stalk)
				{
					item->Animation.TargetState = SPIDER_STATE_WALK_FORWARD;
				}
				else if (creature->Mood == MoodType::Escape || creature->Mood == MoodType::Attack)
				{
					item->Animation.TargetState = SPIDER_STATE_RUN_FORWARD;
				}

				break;

			case SPIDER_STATE_WALK_FORWARD:
				creature->MaxTurn = SMALL_SPIDER_TURN_RATE_MAX;
				if (creature->Mood == MoodType::Bored)
				{
					if (Random::TestProbability(SMALL_SPIDER_IDLE_CHANCE))
						item->Animation.TargetState = SPIDER_STATE_IDLE;
					else
						break;
				}
				else if (creature->Mood == MoodType::Escape || creature->Mood == MoodType::Attack)
				{
					item->Animation.TargetState = SPIDER_STATE_RUN_FORWARD;
				}

				break;

			case SPIDER_STATE_RUN_FORWARD:
				creature->MaxTurn = SMALL_SPIDER_TURN_RATE_MAX;
				creature->Flags = 0;

				if (creature->Mood == MoodType::Bored || creature->Mood == MoodType::Stalk)
				{
					item->Animation.TargetState = SPIDER_STATE_WALK_FORWARD;
				}
				else if (ai.ahead && item->TouchBits.TestAny())
				{
					item->Animation.TargetState = SPIDER_STATE_IDLE;
				}
				else if (ai.ahead && ai.distance < SMALL_SPIDER_SHORT_JUMP_RANGE)
				{
					item->Animation.TargetState = SPIDER_STATE_SHORT_JUMP_ATTACK;
				}
				else if (ai.ahead && ai.distance < SMALL_SPIDER_LONG_JUMP_RANGE)
				{
					item->Animation.TargetState = SPIDER_STATE_LONG_JUMP_ATTACK;
				}

				break;

			case SPIDER_STATE_IDLE_ATTACK:
			case SPIDER_STATE_LONG_JUMP_ATTACK:
			case SPIDER_STATE_SHORT_JUMP_ATTACK:
				creature->MaxTurn = 0;

				if (!creature->Flags && item->TouchBits.TestAny())
				{
					DoSpiderBloodEffect(*item);
					DoDamage(creature->Enemy, SMALL_SPIDER_ATTACK_DAMAGE);
					SoundEffect(SFX_TR2_SPIDER_BITE, &item->Pose);
					creature->Flags = 1;
				}

				break;
			}
		}

		if (item->Animation.ActiveState == SPIDER_STATE_IDLE ||
			item->Animation.ActiveState == SPIDER_STATE_WALK_FORWARD ||
			item->Animation.ActiveState == SPIDER_STATE_RUN_FORWARD)
		{
			// Only 2 block climb.
			if (CreatureVault(itemNumber, headingAngle, 2, 0) == 2)
			{
				creature->MaxTurn = 0;
				SetAnimation(*item, SMALL_SPIDER_ANIM_LONG_JUMP_ATTACK); // HACK: Long jump serves as climb.
				return;
			}
		}
		else
		{
			CreatureAnimation(itemNumber, headingAngle, 0);
		}
	}

	void BigSpiderControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short headingAngle = 0;

		AI_INFO ai;
		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != 7)
			{
				item->Animation.AnimNumber = 2;
				item->Animation.FrameNumber = 0;
				item->Animation.ActiveState = 7;
			}
		}
		else
		{
			CreatureAIInfo(item, &ai);
			GetCreatureMood(item, &ai, true);
			CreatureMood(item, &ai, true);
			headingAngle = CreatureTurn(item, creature->MaxTurn);

			switch (item->Animation.ActiveState)
			{
			case SPIDER_STATE_IDLE:
				creature->MaxTurn = 0;
				creature->Flags = 0;

				if (creature->Mood == MoodType::Bored)
				{
					if (Random::TestProbability(BIG_SPIDER_IDLE_CHANCE))
						item->Animation.TargetState = SPIDER_STATE_IDLE;
					else
						break;
				}
				else if (ai.ahead && ai.distance < BIG_SPIDER_IDLE_RANGE)
				{
					item->Animation.TargetState = SPIDER_STATE_IDLE_ATTACK;
				}
				else if (creature->Mood == MoodType::Stalk)
				{
					item->Animation.TargetState = SPIDER_STATE_WALK_FORWARD;
				}
				else if (creature->Mood == MoodType::Escape || creature->Mood == MoodType::Attack)
				{
					item->Animation.TargetState = SPIDER_STATE_RUN_FORWARD;
				}

				break;

			case SPIDER_STATE_WALK_FORWARD:
				creature->MaxTurn = BIG_SPIDER_TURN_RATE_MAX;

				if (creature->Mood == MoodType::Bored)
				{
					if (Random::TestProbability(BIG_SPIDER_IDLE_CHANCE))
						item->Animation.TargetState = SPIDER_STATE_IDLE;
					else
						break;
				}
				else if (creature->Mood == MoodType::Escape || creature->Mood == MoodType::Attack)
				{
					item->Animation.TargetState = SPIDER_STATE_RUN_FORWARD;
				}

				break;

			case SPIDER_STATE_RUN_FORWARD:
				creature->MaxTurn = BIG_SPIDER_TURN_RATE_MAX;
				creature->Flags = 0;

				if (creature->Mood == MoodType::Bored || creature->Mood == MoodType::Stalk)
				{
					item->Animation.TargetState = SPIDER_STATE_WALK_FORWARD;
				}
				else if (ai.ahead && item->TouchBits.TestAny())
				{
					item->Animation.TargetState = SPIDER_STATE_IDLE;
				}

				break;

			case SPIDER_STATE_IDLE_ATTACK:
				creature->MaxTurn = 0;

				if (!creature->Flags && item->TouchBits.TestAny())
				{
					DoSpiderBloodEffect(*item);
					DoDamage(creature->Enemy, BIG_SPIDER_ATTACK_DAMAGE);
					creature->Flags = 1;
				}

				break;
			}
		}

		CreatureAnimation(itemNumber, headingAngle, 0);
	}
}
