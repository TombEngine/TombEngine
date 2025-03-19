#include "framework.h"
#include "Objects/TR4/Entity/tr4_crocodile.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Math;

namespace TEN::Entities::TR4
{
	constexpr auto CROC_ATTACK_DAMAGE = 120;

	constexpr auto CROC_ALERT_RANGE		 = SQUARE(BLOCK(1.5f));
	constexpr auto CROC_VISIBILITY_RANGE = SQUARE(BLOCK(5));
	constexpr auto CROC_STATE_RUN_RANGE  = SQUARE(BLOCK(1));
	constexpr auto CROC_MAXRUN_RANGE	 = SQUARE(BLOCK(1.5f));
	constexpr auto CROC_ATTACK_RANGE	 = SQUARE(CLICK(2.4f)); // NOTE: It's CLICK(3) in TR4, but the crocodile does not go near Lara to do damage in certain cases.

	constexpr auto CROC_SWIM_SPEED = 16;

	constexpr auto CROC_STATE_WALK_TURN_RATE_MAX = ANGLE(3.0f);
	constexpr auto CROC_STATE_RUN_TURN_RATE_MAX	 = ANGLE(5.0f);
	constexpr auto CROC_STATE_SWIM_TURN_RATE_MAX = ANGLE(3.0f);

	const auto CrocodileBite = CreatureBiteInfo(Vector3(0, -100, 500), 9);
	const auto CrocodileBiteAttackJoints = std::vector<unsigned int>{ 8, 9 };

	enum CrocodileState
	{
		// No state 0.
		CROC_STATE_IDLE = 1,
		CROC_STATE_RUN_FORWARD = 2,
		CROC_STATE_WALK_FORWARD = 3,
		CROC_STATE_TURN_RIGHT = 4,
		CROC_STATE_BITE_ATTACK = 5,
		// No state 6.
		CROC_STATE_DEATH = 7,
		CROC_STATE_SWIM_FORWARD = 8,
		CROC_STATE_WATER_BITE_ATTACK = 9,
		CROC_STATE_WATER_DEATH = 10
	};

	enum CrocodileAnim
	{
		CROC_ANIM_IDLE = 0,
		CROC_ANIM_IDLE_TO_RUN_FORWARD = 1,
		CROC_ANIM_RUN_FORWARD = 2,
		CROC_ANIM_RUN_FORWARD_TO_IDLE_RIGHT = 3,
		CROC_ANIM_RUN_FORWARD_TO_IDLE_LEFT = 4,
		CROC_ANIM_WALK_FORWARD = 5,
		CROC_ANIM_IDLE_TO_WALK_FORWARD = 6,
		CROC_ANIM_TURN_RIGHT_START = 7,
		CROC_ANIM_TURN_RIGHT_CONTINUE = 8,
		CROC_ANIM_TURN_RIGHT_END = 9,
		CROC_ANIM_BITE_ATTACK = 10,
		CROC_ANIM_LAND_DEATH = 11,
		CROC_ANIM_SWIM_FORWARD = 12,
		CROC_ANIM_WATER_BITE_ATTACK_START = 13,
		CROC_ANIM_WATER_BITE_ATTACK_CONTINUE = 14,
		CROC_ANIM_WATER_BITE_ATTACK_END = 15,
		CROC_ANIM_WATER_DEATH = 16,
		CROC_ANIM_LAND_TO_WATER = 17,
		CROC_ANIM_WATER_TO_LAND = 18
	};

	void InitializeCrocodile(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);

		if (TestEnvironment(ENV_FLAG_WATER, item))
			SetAnimation(*item, CROC_ANIM_SWIM_FORWARD);
		else
			SetAnimation(*item, CROC_ANIM_IDLE);
	}

	bool IsCrocodileInWater(ItemInfo* item)
	{
		auto* creature = GetCreatureInfo(item);

		int waterDepth = GetPointCollision(*item).GetWaterSurfaceHeight();
		if (waterDepth != NO_HEIGHT)
		{
			creature->LOT.Step = BLOCK(20);
			creature->LOT.Drop = -BLOCK(20);
			creature->LOT.Fly = CROC_SWIM_SPEED;
		}
		else
		{
			creature->LOT.Step = CLICK(1);
			creature->LOT.Drop = -CLICK(1);
			creature->LOT.Fly = NO_FLYING;
		}

		return waterDepth != NO_HEIGHT;
	}

	void CrocodileControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* object = &Objects[item->ObjectNumber];
		auto* creature = GetCreatureInfo(item);

		short headingAngle = 0;
		short boneAngle = 0;
		auto extraHeadRot = EulerAngles::Identity;
		auto extraTorsoRot = EulerAngles::Identity;

		AI_INFO ai;

		if (item->HitPoints <= 0)
		{
			bool isInWater = TestEnvironment(ENV_FLAG_WATER, item);

			if (item->Animation.ActiveState != CROC_STATE_DEATH && item->Animation.ActiveState != CROC_STATE_WATER_DEATH)
			{
				if (isInWater)
				{
					SetAnimation(*item, CROC_ANIM_WATER_DEATH);
					item->HitPoints = NOT_TARGETABLE;
				}
				else
				{
					SetAnimation(*item, CROC_ANIM_LAND_DEATH);
				}
			}

			if (isInWater)
			{
				CreatureFloat(itemNumber);
				return;
			}
		}
		else
		{
			if (item->AIBits & ALL_AIOBJ)
				GetAITarget(creature);
			else if (creature->HurtByLara)
				creature->Enemy = LaraItem; // TODO: Deal with LaraItem global.

			CreatureAIInfo(item, &ai);
			GetCreatureMood(item, &ai, true);
			CreatureMood(item, &ai, true);

			headingAngle = CreatureTurn(item, creature->MaxTurn);
			boneAngle = headingAngle;

			if ((item->HitStatus || ai.distance < CROC_ALERT_RANGE) ||
				(TargetVisible(item, &ai) && ai.distance < CROC_VISIBILITY_RANGE))
			{
				if (!creature->Alerted)
					creature->Alerted = true;
				AlertAllGuards(itemNumber);
			}

			switch (item->Animation.ActiveState)
			{
			case CROC_STATE_IDLE:
				creature->MaxTurn = 0;

				if (item->AIBits & GUARD)
				{
					boneAngle = item->ItemFlags[0];
					item->Animation.TargetState = CROC_STATE_IDLE;
					item->ItemFlags[0] += item->ItemFlags[1];

					if (Random::TestProbability(1 / 30.0f))
					{
						if (Random::TestProbability(1 / 2.0f))
						{
							item->ItemFlags[1] = 0;
						}
						else
						{
							if (Random::TestProbability(1 / 2.0f))
							{
								item->ItemFlags[1] = 12;
							}
							else
							{
								item->ItemFlags[1] = -12;
							}
						}
					}

					item->ItemFlags[0] = std::clamp<short>(item->ItemFlags[0], -1024, 1024);
				}
				else if (ai.bite && ai.distance < CROC_ATTACK_RANGE)
					item->Animation.TargetState = CROC_STATE_BITE_ATTACK;
				else if(ai.ahead && ai.distance < CROC_STATE_RUN_RANGE)
					item->Animation.TargetState = CROC_STATE_WALK_FORWARD;
				else
					item->Animation.TargetState = CROC_STATE_RUN_FORWARD;

				break;

			case CROC_STATE_WALK_FORWARD:
				creature->MaxTurn = CROC_STATE_WALK_TURN_RATE_MAX;

				// Land to water transition.
				if (IsCrocodileInWater(item) && !item->Animation.RequiredState)
				{
					item->Animation.TargetState = CROC_STATE_SWIM_FORWARD;
					break;
				}

				if (item->Animation.RequiredState != NO_VALUE)
					item->Animation.TargetState = item->Animation.RequiredState;
				else if (ai.bite && ai.distance < CROC_ATTACK_RANGE)
					item->Animation.TargetState = CROC_STATE_IDLE;
				else if (!ai.ahead || ai.distance > CROC_MAXRUN_RANGE)
					item->Animation.TargetState = CROC_STATE_RUN_FORWARD;

				break;

			case CROC_STATE_RUN_FORWARD:
				creature->MaxTurn = CROC_STATE_RUN_TURN_RATE_MAX;

				// Land to water transition.
				if (IsCrocodileInWater(item))
				{
					item->Animation.RequiredState = CROC_STATE_SWIM_FORWARD;
					item->Animation.TargetState = CROC_STATE_WALK_FORWARD;
					break;
				}

				if (item->Animation.RequiredState != NO_VALUE)
					item->Animation.TargetState = item->Animation.RequiredState;
				else if (ai.bite && ai.distance < CROC_ATTACK_RANGE)
					item->Animation.TargetState = CROC_STATE_IDLE;
				else if (ai.ahead && ai.distance < CROC_STATE_RUN_RANGE)
					item->Animation.TargetState = CROC_STATE_WALK_FORWARD;

				break;

			case CROC_STATE_BITE_ATTACK:
				if (item->Animation.FrameNumber == 0)
					item->Animation.RequiredState = NO_VALUE;

				if (ai.bite &&
					item->TouchBits.Test(CrocodileBiteAttackJoints))
				{
					if (item->Animation.RequiredState == NO_VALUE)
					{
						CreatureEffect2(item, CrocodileBite, 10, -1, DoBloodSplat);
						DoDamage(creature->Enemy, CROC_ATTACK_DAMAGE);
						item->Animation.RequiredState = CROC_STATE_IDLE;
					}
				}
				else
					item->Animation.TargetState = CROC_STATE_IDLE;

				break;

			case CROC_STATE_SWIM_FORWARD:
				creature->MaxTurn = CROC_STATE_SWIM_TURN_RATE_MAX;

				// Water to land transition.
				if (!IsCrocodileInWater(item))
				{
					SetAnimation(*item, CROC_ANIM_WATER_TO_LAND);
					break;
				}

				if (item->Animation.RequiredState != NO_VALUE)
					item->Animation.TargetState = item->Animation.RequiredState;
				else if (ai.bite)
				{
					if (item->TouchBits.Test(CrocodileBiteAttackJoints))
						item->Animation.TargetState = CROC_STATE_WATER_BITE_ATTACK;
				}

				break;

			case CROC_STATE_WATER_BITE_ATTACK:
				if (item->Animation.FrameNumber == 0)
					item->Animation.RequiredState = NO_VALUE;

				if (ai.bite && item->TouchBits.Test(CrocodileBiteAttackJoints))
				{
					if (item->Animation.RequiredState == NO_VALUE)
					{
						CreatureEffect2(item, CrocodileBite, 10, -1, DoBloodSplat);
						DoDamage(creature->Enemy, CROC_ATTACK_DAMAGE);
						item->Animation.RequiredState = CROC_STATE_SWIM_FORWARD;
					}
				}
				else
					item->Animation.TargetState = CROC_STATE_SWIM_FORWARD;

				break;
			}
		}

		if (item->Animation.ActiveState == CROC_STATE_IDLE || item->Animation.ActiveState == CROC_STATE_BITE_ATTACK || item->Animation.ActiveState == CROC_STATE_WATER_BITE_ATTACK)
		{
			extraHeadRot.y = ai.angle / 3;
			extraHeadRot.x = ai.angle / 2;
			extraTorsoRot.y = 0;
			extraTorsoRot.x = 0;
		}
		else
		{
			extraHeadRot.y = boneAngle;
			extraHeadRot.x = boneAngle;
			extraTorsoRot.y = -boneAngle;
			extraTorsoRot.x = -boneAngle;
		}

		CreatureTilt(item, 0);
		CreatureJoint(item, 0, extraHeadRot.y);
		CreatureJoint(item, 1, extraHeadRot.x);
		CreatureJoint(item, 2, extraTorsoRot.y);
		CreatureJoint(item, 3, extraTorsoRot.x);
		CreatureAnimation(itemNumber, headingAngle, 0);

		if (item->Animation.ActiveState < CROC_STATE_SWIM_FORWARD)
		{
			auto radius = Vector2(object->radius, object->radius * 1.5f);
			AlignEntityToSurface(item, radius);
		}

		if (item->Animation.ActiveState >= CROC_STATE_SWIM_FORWARD && item->Animation.ActiveState <= CROC_STATE_WATER_DEATH)
		{
			CreatureUnderwater(item, CLICK(1));
		}
		else
		{
			CreatureUnderwater(item, 0);
		}
	}
}
