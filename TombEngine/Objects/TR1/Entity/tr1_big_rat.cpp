#include "framework.h"
#include "Objects/TR1/Entity/tr1_big_rat.h"

#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/effects/Ripple.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Collision::Point;
using namespace TEN::Effects::Ripple;
using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR1
{
	constexpr auto BIG_RAT_BITE_ATTACK_DAMAGE   = 20;
	constexpr auto BIG_RAT_POUNCE_ATTACK_DAMAGE = 25; // 20 in OG.

	constexpr auto BIG_RAT_LAND_BITE_ATTACK_RANGE  = SQUARE(BLOCK(0.34f));
	constexpr auto BIG_RAT_POUNCE_ATTACK_RANGE     = SQUARE(BLOCK(1.0f)); // (3 / 2.0f) in OG, adapted to TEN to minimize false attack.
	constexpr auto BIG_RAT_WATER_BITE_ATTACK_RANGE = SQUARE(BLOCK(0.3f));

	constexpr auto BIG_RAT_REAR_POSE_CHANCE     = 1 / 128.0f;
	constexpr auto BIG_RAT_WATER_SURFACE_OFFSET = 25; // Increased to prevent the rat entering the slopes in water.
	constexpr auto BIG_RAT_RIPPLE_RADIUS        = 128.0f;

	constexpr auto BIG_RAT_RUN_TURN_RATE_MAX  = ANGLE(9.0f); // (6.0f) in OG, revert after spasm effect with velocity is implemented.
	constexpr auto BIG_RAT_SWIM_TURN_RATE_MAX = ANGLE(4.0f); // (3.0f) in OG, revert after spasm effect with velocity is implemented.

	const auto BigRatBite = CreatureBiteInfo(Vector3(0, -11, 108), 3);
	const auto BigRatAttackJoints = std::vector<unsigned int>{ 0, 1, 2, 3, 7, 8, 24, 25 };

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
		BIG_RAT_ANIM_WATER_DEATH = 11
	};

	void InitializeBigRat(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);

		if (TestEnvironment(ENV_FLAG_WATER, item) || TestEnvironment(ENV_FLAG_SWAMP, item))
			SetAnimation(item, BIG_RAT_ANIM_SWIM);
		else
			SetAnimation(item, BIG_RAT_ANIM_IDLE);
	}

	static bool IsBigRatOnWater(ItemInfo* item)
	{
		return (GetPointCollision(*item).GetWaterTopHeight() != NO_HEIGHT);
	}

	static void SetBigRatWater(ItemInfo* item)
	{
		auto& creature = *GetCreatureInfo(item);

		if (IsBigRatOnWater(item))
		{
			creature.LOT.Step = BLOCK(20);
			creature.LOT.Drop = -BLOCK(20);
		}
		else
		{
			creature.LOT.Step = CLICK(1.0f);
			creature.LOT.Drop = -CLICK(1.5f);
		}
	}

	void BigRatControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short head = 0;

		SetBigRatWater(item);
		bool isOnWater = IsBigRatOnWater(item);

		if (item->HitStatus)
			SoundEffect(SFX_TR1_RAT_CHIRP, &item->Pose);

		if (item->HitPoints <= 0)
		{
			bool doWaterDeath = isOnWater;
			if (item->Animation.ActiveState != BIG_RAT_STATE_LAND_DEATH &&
				item->Animation.ActiveState != BIG_RAT_STATE_WATER_DEATH)
			{
				if (doWaterDeath)
					SetAnimation(item, BIG_RAT_ANIM_WATER_DEATH);
				else
					SetAnimation(item, BIG_RAT_ANIM_LAND_DEATH);
			}
		}
		else
		{
			AI_INFO ai;
			CreatureAIInfo(item, &ai);

			if (ai.ahead)
				head = ai.angle;

			GetCreatureMood(item, &ai, isOnWater);
			CreatureMood(item, &ai, isOnWater);
			creature->MaxTurn = isOnWater ? BIG_RAT_SWIM_TURN_RATE_MAX : BIG_RAT_RUN_TURN_RATE_MAX;
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
				if (isOnWater)
				{
					SetAnimation(item, BIG_RAT_ANIM_SWIM);
					break;
				}

				if (ai.ahead && item->TouchBits.Test(BigRatAttackJoints))
				{
					if (Random::TestProbability(1 / 14.0f))
						SetAnimation(item, BIG_RAT_ANIM_IDLE); // Trick to increase BITE-ATTACK by randomizer.

					item->Animation.TargetState = BIG_RAT_STATE_IDLE;
					break;
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
					item->TouchBits.Test(BigRatAttackJoints))
				{
					DoDamage(creature->Enemy, BIG_RAT_BITE_ATTACK_DAMAGE);
					CreatureEffect(item, BigRatBite, DoBloodSplat);
					item->Animation.RequiredState = BIG_RAT_STATE_IDLE;
				}

				break;

			case BIG_RAT_STATE_POUNCE_ATTACK:
				if (item->Animation.RequiredState == NO_VALUE && ai.ahead &&
					item->TouchBits.Test(BigRatAttackJoints))
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
				if (!isOnWater)
				{
					SetAnimation(item, BIG_RAT_ANIM_RUN_FORWARD);
					break;
				}

				if (ai.bite && ai.distance < BIG_RAT_WATER_BITE_ATTACK_RANGE)
					item->Animation.TargetState = BIG_RAT_STATE_SWIM_BITE_ATTACK;

				break;

			case BIG_RAT_STATE_SWIM_BITE_ATTACK:
				if (item->Animation.RequiredState != BIG_RAT_STATE_SWIM && ai.ahead &&
					item->TouchBits.Test(BigRatAttackJoints))
				{
					DoDamage(creature->Enemy, BIG_RAT_BITE_ATTACK_DAMAGE);
					CreatureEffect(item, BigRatBite, DoBloodSplat);
					item->Animation.RequiredState = BIG_RAT_STATE_SWIM;
				}

				break;
			}

		}

		CreatureJoint(item, 0, head);
		CreatureAnimation(itemNumber, angle, 0);

		if (isOnWater)
		{
			CreatureUnderwater(item, 0);
			item->Pose.Position.y = GetPointCollision(*item).GetWaterTopHeight() - BIG_RAT_WATER_SURFACE_OFFSET;

			if (item->Animation.ActiveState == BIG_RAT_STATE_SWIM ||
				item->Animation.ActiveState == BIG_RAT_STATE_SWIM_BITE_ATTACK)
			{
				if (!(Wibble & 30))
				{
					SpawnRipple(
						item->Pose.Position.ToVector3(),
						item->RoomNumber,
						BIG_RAT_RIPPLE_RADIUS,
						(int)RippleFlags::SlowFade | (int)RippleFlags::LowOpacity);
				}
			}
		}
		else
		{
			item->Pose.Position.y = item->Floor;
		}
	}
}
