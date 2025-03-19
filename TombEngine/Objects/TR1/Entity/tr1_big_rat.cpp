#include "framework.h"
#include "Objects/TR1/Entity/tr1_big_rat.h"

#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Specific/level.h"

using namespace TEN::Collision::Point;
using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR1
{
	constexpr auto BIG_RAT_BITE_ATTACK_DAMAGE	= 20;
	constexpr auto BIG_RAT_POUNCE_ATTACK_DAMAGE = 25;

	constexpr auto BIG_RAT_ALERT_RANGE			   = SQUARE(BLOCK(3 / 2.0f));
	constexpr auto BIG_RAT_VISIBILITY_RANGE		   = SQUARE(BLOCK(5));
	constexpr auto BIG_RAT_LAND_BITE_ATTACK_RANGE  = SQUARE(BLOCK(0.34f));
	constexpr auto BIG_RAT_POUNCE_ATTACK_RANGE	   = SQUARE(BLOCK(1 / 2.0f));
	constexpr auto BIG_RAT_WATER_BITE_ATTACK_RANGE = SQUARE(BLOCK(0.3f));

	constexpr auto BIG_RAT_REAR_POSE_CHANCE = 1 / 128.0f;
	constexpr auto BIG_RAT_SWIM_UP_DOWN_SPEED = 32;
	constexpr auto BIG_RAT_WATER_SURFACE_OFFSET = 10;

	constexpr auto BIG_RAT_RUN_TURN_RATE_MAX  = ANGLE(6.0f);
	constexpr auto BIG_RAT_SWIM_TURN_RATE_MAX = ANGLE(3.0f);

	const auto BigRatBite = CreatureBiteInfo(Vector3(0, -11, 108), 3);

	enum BigRatState
	{
		// No state 0.
		BIG_RAT_STATE_IDLE = 1,
		BIG_RAT_STATE_POUNCE_ATTACK = 2,
		BIG_RAT_STATE_RUN_FORWARD = 3,
		BIG_RAT_STATE_LAND_BITE_ATTACK = 4,
		BIG_RAT_STATE_LAND_DEATH = 5,
		BIG_RAT_STATE_REAR_POSE = 6,
		BIG_RAT_STATE_SWIM = 7,
		BIG_RAT_STATE_SWIM_BITE_ATTACK = 8,
		BIG_RAT_STATE_WATER_DEATH = 9
	};

	enum BigRatAnim
	{
		BIG_RAT_ANIM_IDLE = 0,
		BIG_RAT_ANIM_IDLE_TO_RUN_FORWARD = 1,
		BIG_RAT_ANIM_RUN_FORWARD = 2,
		BIG_RAT_ANIM_RUN_FORWARD_TO_IDLE = 3,
		BIG_RAT_ANIM_REAR_POSE = 4,
		BIG_RAT_ANIM_REAR_POSE_TO_IDLE = 5,
		BIG_RAT_ANIM_LAND_BITE_ATTACK = 6,
		BIG_RAT_ANIM_POUNCE_ATTACK = 7,
		BIG_RAT_ANIM_LAND_DEATH = 8,
		BIG_RAT_ANIM_SWIM = 9,
		BIG_RAT_ANIM_WATER_BITE_ATTACK = 10,
		BIG_RAT_ANIM_WATER_DEATH = 11,

		// NOTE: These animations don't exist for the TR2 rat. -- TokyoSU 2022.08.10
		BIG_RAT_ANIM_RUN_FORWARD_TO_SWIM = 12,
		BIG_RAT_ANIM_SWIM_TO_RUN_FORWARD = 13
	};

	void InitializeBigRat(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);

		if (TestEnvironment(ENV_FLAG_WATER, item))
			SetAnimation(*item, BIG_RAT_ANIM_SWIM);
		else
			SetAnimation(*item, BIG_RAT_ANIM_IDLE);
	}

	bool RatOnWater(ItemInfo* item)
	{
		int waterDepth = GetPointCollision(*item).GetWaterSurfaceHeight();
		if (item->IsCreature())
		{
			auto& creature = *GetCreatureInfo(item);

			if (waterDepth != NO_HEIGHT)
			{
				creature.LOT.Step = BLOCK(20);
				creature.LOT.Drop = -BLOCK(20);
			}
			else
			{
				creature.LOT.Step = CLICK(1);
				creature.LOT.Drop = -CLICK(1);
			}
		}
		
		return waterDepth != NO_HEIGHT;
	}

	void BigRatControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short head = 0;

		if (item->HitPoints <= 0)
		{
			bool doWaterDeath = RatOnWater(item);
			if (item->Animation.ActiveState != BIG_RAT_STATE_LAND_DEATH &&
				item->Animation.ActiveState != BIG_RAT_STATE_WATER_DEATH)
			{
				if (doWaterDeath)
					SetAnimation(*item, BIG_RAT_ANIM_WATER_DEATH);
				else
					SetAnimation(*item, BIG_RAT_ANIM_LAND_DEATH);
			}

			if (doWaterDeath)
				CreatureFloat(itemNumber);
		}
		else
		{
			AI_INFO ai;
			CreatureAIInfo(item, &ai);

			if (ai.ahead)
				head = ai.angle;

			GetCreatureMood(item, &ai, false);
			CreatureMood(item, &ai, false);
			angle = CreatureTurn(item, creature->MaxTurn);

			switch (item->Animation.ActiveState)
			{
			case BIG_RAT_STATE_IDLE:
				if (item->Animation.RequiredState != NO_VALUE)
					item->Animation.TargetState = item->Animation.RequiredState;
				else if (ai.bite && ai.distance < BIG_RAT_LAND_BITE_ATTACK_RANGE)
					item->Animation.TargetState = BIG_RAT_STATE_LAND_BITE_ATTACK;
				else
					item->Animation.TargetState = BIG_RAT_STATE_RUN_FORWARD;

				break;

			case BIG_RAT_STATE_RUN_FORWARD:
				creature->MaxTurn = BIG_RAT_RUN_TURN_RATE_MAX;

				if (RatOnWater(item))
				{
					SetAnimation(*item, BIG_RAT_ANIM_SWIM);
					break;
				}

				if (ai.ahead && item->TouchBits.Test(BigRatBite.BoneID))
				{
					item->Animation.TargetState = BIG_RAT_STATE_IDLE;
				}
				else if (ai.bite && ai.distance < BIG_RAT_POUNCE_ATTACK_RANGE)
				{
					item->Animation.TargetState = BIG_RAT_STATE_POUNCE_ATTACK;
				}
				else if (ai.ahead && Random::TestProbability(BIG_RAT_REAR_POSE_CHANCE))
				{
					item->Animation.TargetState = BIG_RAT_STATE_IDLE;
					item->Animation.RequiredState = BIG_RAT_STATE_REAR_POSE;
				}

				break;

			case BIG_RAT_STATE_LAND_BITE_ATTACK:
				if (item->Animation.RequiredState == NO_VALUE && ai.ahead &&
					item->TouchBits.Test(BigRatBite.BoneID))
				{
					DoDamage(creature->Enemy, BIG_RAT_BITE_ATTACK_DAMAGE);
					CreatureEffect(item, BigRatBite, DoBloodSplat);
					item->Animation.RequiredState = BIG_RAT_STATE_IDLE;
				}

				break;

			case BIG_RAT_STATE_POUNCE_ATTACK:
				if (item->Animation.RequiredState == NO_VALUE && ai.ahead &&
					item->TouchBits.Test(BigRatBite.BoneID))
				{
					DoDamage(creature->Enemy, BIG_RAT_POUNCE_ATTACK_DAMAGE);
					CreatureEffect(item, BigRatBite, DoBloodSplat);
					item->Animation.RequiredState = BIG_RAT_STATE_RUN_FORWARD;
				}

				break;

			case BIG_RAT_STATE_REAR_POSE:
				if (creature->Mood != MoodType::Bored || Random::TestProbability(BIG_RAT_REAR_POSE_CHANCE))
					item->Animation.TargetState = BIG_RAT_STATE_IDLE;

				break;

			case BIG_RAT_STATE_SWIM:
				creature->MaxTurn = BIG_RAT_SWIM_TURN_RATE_MAX;

				if (!RatOnWater(item))
				{
					SetAnimation(*item, BIG_RAT_ANIM_RUN_FORWARD);
					break;
				}

				if (ai.ahead && item->TouchBits.Test(BigRatBite.BoneID))
					item->Animation.TargetState = BIG_RAT_STATE_SWIM_BITE_ATTACK;

				break;

			case BIG_RAT_STATE_SWIM_BITE_ATTACK:
				if (item->Animation.RequiredState == NO_VALUE && ai.ahead &&
					item->TouchBits.Test(BigRatBite.BoneID))
				{
					DoDamage(creature->Enemy, BIG_RAT_BITE_ATTACK_DAMAGE);
					CreatureEffect(item, BigRatBite, DoBloodSplat);
				}

				break;
			}

		}

		CreatureJoint(item, 0, head);
		CreatureAnimation(itemNumber, angle, 0);

		if (RatOnWater(item))
		{
			CreatureUnderwater(item, 0);
			item->Pose.Position.y = GetPointCollision(*item).GetWaterTopHeight() - BIG_RAT_WATER_SURFACE_OFFSET;
		}
		else
		{
			item->Pose.Position.y = item->Floor;
		}
	}
}
