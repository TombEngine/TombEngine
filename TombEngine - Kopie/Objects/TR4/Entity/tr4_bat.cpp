#include "framework.h"
#include "Objects/TR4/Entity/tr4_bat.h"

#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/control/lot.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"

using namespace TEN::Math;

namespace TEN::Entities::TR4
{
	constexpr auto BAT_ATTACK_DAMAGE = 2;

	constexpr auto BAT_UNFURL_HEIGHT_RANGE = BLOCK(0.87f);
	constexpr auto BAT_ATTACK_RANGE		   = SQUARE(BLOCK(1 / 4.0f));
	constexpr auto BAT_AWARE_RANGE		   = SQUARE(BLOCK(5));

	constexpr auto BAT_ANGLE = ANGLE(20.0f);

	const auto BatBite = CreatureBiteInfo(Vector3(0, 16, 45), 4);

	enum BatState
	{
		// No state 0.
		BAT_STATE_DROP_FROM_CEILING = 1,
		BAT_STATE_FLY = 2,
		BAT_STATE_ATTACK = 3,
		BAT_STATE_DEATH_FALL = 4,
		BAT_STATE_DEATH = 5,
		BAT_STATE_IDLE = 6
	};

	enum BatAnim
	{
		BAT_ANIM_DROP_FROM_CEILING = 0,
		BAT_ANIM_FLY = 1,
		BAT_ANIM_ATTACK = 2,
		BAT_ANIM_DEATH_FALL = 3,
		BAT_ANIM_DEATH = 4,
		BAT_ANIM_IDLE = 5 // NOTE: TR1 bat don't have this animation, which bug the bat, you need to add the animation manually - TokyoSU: 18/6/2023
	};

	void InitializeBat(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(item, BAT_ANIM_IDLE);
	}

	static bool BatCanAttackTarget(ItemInfo& item, const AI_INFO& ai)
	{
		const auto& creature = *GetCreatureInfo(&item);

		int deltaHeight = abs(item.Pose.Position.y - creature.Enemy->Pose.Position.y);
		if (item.TouchBits.TestAny() ||
			(ai.distance < BAT_ATTACK_RANGE && ai.ahead && deltaHeight < BAT_UNFURL_HEIGHT_RANGE))
		{
			return true;
		}

		return false;
	}

	void BatControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto& item = g_Level.Items[itemNumber];
		auto& creature = *GetCreatureInfo(&item);

		short headingAngle = 0;

		if (item.HitPoints <= 0)
		{
			if (item.Animation.ActiveState != BAT_STATE_DEATH_FALL && item.Animation.ActiveState != BAT_STATE_DEATH)
			{
				item.Animation.IsAirborne = true;
				item.Animation.Velocity.z = 0;
				SetAnimation(item, BAT_ANIM_DEATH_FALL);
			}
			else if (item.Pose.Position.y >= item.Floor && item.Animation.ActiveState != BAT_STATE_DEATH)
			{
				item.Animation.TargetState = BAT_STATE_DEATH;
				item.Animation.IsAirborne = false;
				item.Pose.Position.y = item.Floor;
			}
		}
		else
		{
			if (item.AIBits)
				GetAITarget(&creature);

			AI_INFO ai;
			CreatureAIInfo(&item, &ai);
			GetCreatureMood(&item, &ai, false);

			if (creature.Flags != 0)
				creature.Mood = MoodType::Escape;

			CreatureMood(&item, &ai, false);
			headingAngle = CreatureTurn(&item, BAT_ANGLE);

			switch (item.Animation.ActiveState)
			{
			case BAT_STATE_IDLE:
				if (ai.distance < BAT_AWARE_RANGE || item.HitStatus || creature.HurtByLara)
					item.Animation.TargetState = BAT_STATE_DROP_FROM_CEILING;

				break;

			case BAT_STATE_FLY:
				if (ai.distance < BAT_ATTACK_RANGE || Random::TestProbability(1 / 64.0f))
					creature.Flags = 0;

				if (creature.Flags == 0 && BatCanAttackTarget(item, ai))
					item.Animation.TargetState = BAT_STATE_ATTACK;

				break;

			case BAT_STATE_ATTACK:
				if (creature.Flags == 0 && BatCanAttackTarget(item, ai))
				{
					DoDamage(creature.Enemy, BAT_ATTACK_DAMAGE);
					CreatureEffect(&item, BatBite, DoBloodSplat);
					creature.Flags = 1;
				}
				else
				{
					item.Animation.TargetState = BAT_STATE_FLY;
					creature.Mood = MoodType::Bored;
				}

				break;
			}
		}

		CreatureAnimation(itemNumber, headingAngle, 0);
	}
}
