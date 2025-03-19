#include "framework.h"
#include "Objects/TR4/Entity/tr4_small_scorpion.h"

#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Specific/level.h"

using namespace TEN::Math;

namespace TEN::Entities::TR4
{
	constexpr auto SMALL_SCORPION_PINCER_ATTACK_DAMAGE	 = 50;
	constexpr auto SMALL_SCORPION_STINGER_ATTACK_DAMAGE	 = 20;
	constexpr auto SMALL_SCORPION_STINGER_POISON_POTENCY = 2;

	constexpr auto SMALL_SCORPION_ATTACK_RANGE = SQUARE(BLOCK(0.31));

	const auto SmallScorpionBite1 = CreatureBiteInfo(Vector3::Zero, 0);
	const auto SmallScorpionBite2 = CreatureBiteInfo(Vector3::Zero, 23);
	const auto SmallScorpionAttackJoints = std::vector<unsigned int>{ 8, 22, 23, 25, 26 };

	enum SmallScorionState
	{
		// No state 0.
		SSCORPION_STATE_IDLE = 1,
		SSCORPION_STATE_WALK = 2,
		SSCORPION_STATE_RUN = 3,
		SSCORPION_STATE_ATTACK_1 = 4,
		SSCORPION_STATE_ATTACK_2 = 5,
		SSCORPION_STATE_DEATH_1 = 6,
		SSCORPION_STATE_DEATH_2 = 7
	};

	// TODO
	enum SmallScorpionAnim
	{
		SSCORPION_ANIM_WALK = 0,
		SSCORPION_ANIM_RUN = 1,
		SSCORPION_ANIM_IDLE = 2,
		SSCORPION_ANIM_STAB = 3,
		SSCORPION_ANIM_STING = 4,
		SSCORPION_ANIM_DEATH = 5,
		SSCORPION_ANIM_FLAT_1 = 6,
		SSCORPION_ANIM_FLAT_2 = 7,
	};

	void InitializeSmallScorpion(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(*item, SSCORPION_ANIM_IDLE);
	}

	void SmallScorpionControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short head = 0;
		short joint0 = 0;
		short joint1 = 0;
		short joint2 = 0;
		short joint3 = 0;

		if (item->HitPoints <= 0)
		{
			item->HitPoints = 0;
			if (item->Animation.ActiveState != SSCORPION_STATE_DEATH_1 &&
				item->Animation.ActiveState != SSCORPION_STATE_DEATH_2)
			{
				SetAnimation(*item, SSCORPION_ANIM_DEATH);
			}
		}
		else
		{
			if (item->AIBits & GUARD)
				GetAITarget(creature);
			else
				creature->Enemy = LaraItem;

			AI_INFO AI;
			CreatureAIInfo(item, &AI);

			GetCreatureMood(item, &AI, true);
			CreatureMood(item, &AI, true);

			angle = CreatureTurn(item, creature->MaxTurn);

			switch (item->Animation.ActiveState)
			{
			case SSCORPION_STATE_IDLE:
				creature->MaxTurn = 0;
				creature->Flags = 0;

				if (AI.distance > SMALL_SCORPION_ATTACK_RANGE)
					item->Animation.TargetState = SSCORPION_STATE_WALK;
				else if (AI.bite)
				{
					creature->MaxTurn = ANGLE(6.0f);

					if (Random::TestProbability(1 / 2.0f))
						item->Animation.TargetState = SSCORPION_STATE_ATTACK_1;
					else
						item->Animation.TargetState = SSCORPION_STATE_ATTACK_2;
				}
				else if (!AI.ahead)
					item->Animation.TargetState = SSCORPION_STATE_RUN;

				break;

			case SSCORPION_STATE_WALK:
				creature->MaxTurn = ANGLE(6.0f);

				if (AI.distance >= SMALL_SCORPION_ATTACK_RANGE)
						item->Animation.TargetState = SSCORPION_STATE_RUN;
				else
					item->Animation.TargetState = SSCORPION_STATE_IDLE;

				break;

			case SSCORPION_STATE_RUN:
				creature->MaxTurn = ANGLE(8.0f);

				if (AI.distance < SMALL_SCORPION_ATTACK_RANGE)
					item->Animation.TargetState = SSCORPION_STATE_IDLE;

				break;

			case SSCORPION_STATE_ATTACK_1:
			case SSCORPION_STATE_ATTACK_2:
				creature->MaxTurn = 0;

				if (abs(AI.angle) >= ANGLE(6.0f))
				{
					if (AI.angle >= 0)
						item->Pose.Orientation.y += ANGLE(6.0f);
					else
						item->Pose.Orientation.y -= ANGLE(6.0f);
				}
				else
					item->Pose.Orientation.y += AI.angle;

				if (!creature->Flags)
				{
					if (item->TouchBits.Test(SmallScorpionAttackJoints))
					{
						if (item->Animation.FrameNumber > 20 &&
							item->Animation.FrameNumber < 32)
						{
							short rotation;
							CreatureBiteInfo biteInfo;

							// Pincer attack
							if (item->Animation.ActiveState == SSCORPION_STATE_ATTACK_1)
							{
								DoDamage(creature->Enemy, SMALL_SCORPION_PINCER_ATTACK_DAMAGE);
								rotation = item->Pose.Orientation.y - ANGLE(180.0f);
								biteInfo = SmallScorpionBite1;
							}
							// Stinger attack
							else
							{
								if (creature->Enemy->IsLara())
									GetLaraInfo(creature->Enemy)->Status.Poison += SMALL_SCORPION_STINGER_POISON_POTENCY;

								DoDamage(creature->Enemy, SMALL_SCORPION_STINGER_ATTACK_DAMAGE);
								rotation = item->Pose.Orientation.y - ANGLE(180.0f);
								biteInfo = SmallScorpionBite2;
							}

							CreatureEffect2(item, biteInfo, 3, rotation, DoBloodSplat);
							creature->Flags = 1;
						}
					}
				}

				break;
			}
		}

		CreatureAnimation(itemNumber, angle, 0);
	}
}
