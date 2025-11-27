#include "framework.h"
#include "Objects/TR5/Entity/tr5_brownbeast.h"

#include "Game/control/box.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/misc.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Sound/sound.h"
#include "Math/Math.h"
#include "Specific/level.h"

using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR5
{
	constexpr auto BROWN_BEAST_ATTACK_DAMAGE = 150;

	const auto BrownBeastBite1 = CreatureBiteInfo(Vector3::Zero, 16);
	const auto BrownBeastBite2 = CreatureBiteInfo(Vector3::Zero, 22);
	const auto BrownBeastAttackJoints1 = std::vector<unsigned int>{ 14, 15, 16, 17 };
	const auto BrownBeastAttackJoints2 = std::vector<unsigned int>{ 20, 21, 22, 23 };

	enum BrownBeastState
	{
		// No state 0.
		BROWN_BEAST_STATE_IDLE = 1,
		BROWN_BEAST_STATE_WALK_FORWARD = 2,
		BROWN_BEAST_STATE_RUN_FORWARD = 3,
		BROWN_BEAST_STATE_SWIPE_ATTACK = 4,
		BROWN_BEAST_STATE_JUMP_BITE_ATTACK = 5,
		BROWN_BEAST_STATE_JUMP_SWIPE_ATTACK = 6,
		BROWN_BEAST_STATE_DEATH = 7
	};

	enum BrownBeastAnim
	{
		BROWN_BEAST_ANIM_IDLE = 0,
		BROWN_BEAST_ANIM_RUN_FORWARD = 1,
		BROWN_BEAST_ANIM_SWIPE_ATTACK = 2,
		BROWN_BEAST_ANIM_JUMP_BITE_ATTACK_START = 3,
		BROWN_BEAST_ANIM_JUMP_BITE_ATTACK_CONTINUE = 4,
		BROWN_BEAST_ANIM_JUMP_BITE_ATTACK_END = 5,
		BROWN_BEAST_ANIM_WALK_FORWARD = 6,
		BROWN_BEAST_ANIM_JUMP_SWIPE_ATTACK_START = 7,
		BROWN_BEAST_ANIM_JUMP_SWIPE_ATTACK_CONTINUE = 8,
		BROWN_BEAST_ANIM_JUMP_SWIPE_ATTACK_END = 9,
		BROWN_BEAST_ANIM_DEATH = 10,
		BROWN_BEAST_ANIM_IDLE_TO_WALK_FORWARD = 11,
		BROWN_BEAST_ANIM_WALK_FORWARD_TO_IDLE = 12,
		BROWN_BEAST_ANIM_IDLE_TO_RUN_FORWARD = 13,
		BROWN_BEAST_ANIM_RUN_FORWARD_TO_IDLE = 14,
	};

	void InitializeBrownBeast(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(item, BROWN_BEAST_ANIM_IDLE);
	}

	void ControlBrowsBeast(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short headingAngle = 0;

		if (item->HitPoints <= 0)
		{
			item->HitPoints = 0;
			if (item->Animation.ActiveState != BROWN_BEAST_STATE_DEATH)
				SetAnimation(item, BROWN_BEAST_ANIM_DEATH);
		}
		else
		{
			if (item->AIBits)
				GetAITarget(creature);
			else if (creature->HurtByLara)
				creature->Enemy = LaraItem;

			AI_INFO ai;
			CreatureAIInfo(item, &ai);

			int distance;

			if (creature->Enemy == LaraItem)
				distance = ai.distance;
			else
			{
				int dx = LaraItem->Pose.Position.x - item->Pose.Position.x;
				int dz = LaraItem->Pose.Position.z - item->Pose.Position.z;
				phd_atan(dz, dz);

				distance = pow(dx, 2) + pow(dz, 2);
			}

			GetCreatureMood(item, &ai, true);
			CreatureMood(item, &ai, true);

			headingAngle = CreatureTurn(item, creature->MaxTurn);
			creature->MaxTurn = ANGLE(7.0f);

			switch (item->Animation.ActiveState)
			{
			case BROWN_BEAST_STATE_IDLE:
				creature->Flags = 0;

				if (creature->Mood == MoodType::Attack)
				{
					if (distance <= pow(BLOCK(1), 2))
					{
						if (Random::TestProbability(1 / 2.0f))
							item->Animation.TargetState = BROWN_BEAST_STATE_SWIPE_ATTACK;
						else
							item->Animation.TargetState = BROWN_BEAST_STATE_JUMP_SWIPE_ATTACK;
					}
					else if (Random::TestProbability(1 / 2.0f))
						item->Animation.TargetState = BROWN_BEAST_STATE_WALK_FORWARD;
					else
						item->Animation.TargetState = BROWN_BEAST_STATE_RUN_FORWARD;
				}
				else
					item->Animation.TargetState = BROWN_BEAST_STATE_IDLE;

				break;

			case BROWN_BEAST_STATE_WALK_FORWARD:
			case BROWN_BEAST_STATE_RUN_FORWARD:
				if (distance < pow(BLOCK(1), 2) || creature->Mood != MoodType::Attack)
					item->Animation.TargetState = BROWN_BEAST_STATE_IDLE;

				SoundEffect(SFX_TR5_IMP_BARREL_ROLL, &item->Pose);
				break;

			case BROWN_BEAST_STATE_SWIPE_ATTACK:
			case BROWN_BEAST_STATE_JUMP_SWIPE_ATTACK:
				creature->MaxTurn = 0;

				if (abs(ai.angle) >= ANGLE(2.0f))
				{
					if (ai.angle > 0)
						item->Pose.Orientation.y += ANGLE(2.0f);
					else
						item->Pose.Orientation.y -= ANGLE(2.0f);
				}
				else
					item->Pose.Orientation.y += ai.angle;

				if (creature->Flags)
					break;

				if (item->TouchBits.Test(BrownBeastAttackJoints1))
				{
					if (item->Animation.AnimNumber == BROWN_BEAST_ANIM_JUMP_SWIPE_ATTACK_CONTINUE)
					{
						if (TestAnimFrameRange(*item, 20, 24))
						{
							DoDamage(creature->Enemy, BROWN_BEAST_ATTACK_DAMAGE);
							CreatureEffect2(item, BrownBeastBite1, 20, item->Pose.Orientation.y, DoBloodSplat);
							creature->Flags |= 1;
							break;
						}
					}

					if (item->Animation.AnimNumber == BROWN_BEAST_ANIM_SWIPE_ATTACK)
					{
						if (TestAnimFrameRange(*item, 7, 15))
						{
							DoDamage(creature->Enemy, BROWN_BEAST_ATTACK_DAMAGE);
							CreatureEffect2(item, BrownBeastBite1, 20, item->Pose.Orientation.y, DoBloodSplat);
							creature->Flags |= 1;
							break;
						}
					}
				}

				if (!item->TouchBits.Test(BrownBeastAttackJoints2))
					break;

				if (item->Animation.AnimNumber == BROWN_BEAST_ANIM_JUMP_SWIPE_ATTACK_CONTINUE)
				{
					if (TestAnimFrameRange(*item, 14, 19))
					{
						DoDamage(creature->Enemy, BROWN_BEAST_ATTACK_DAMAGE);
						CreatureEffect2(item, BrownBeastBite2, 20, item->Pose.Orientation.y, DoBloodSplat);
						creature->Flags |= 2;
						break;
					}
				}

				if (item->Animation.AnimNumber == BROWN_BEAST_ANIM_SWIPE_ATTACK)
				{
					if (TestAnimFrameRange(*item, 34, 42))
					{
						DoDamage(creature->Enemy, BROWN_BEAST_ATTACK_DAMAGE);
						CreatureEffect2(item, BrownBeastBite2, 20, item->Pose.Orientation.y, DoBloodSplat);
						creature->Flags |= 2;
						break;
					}
				}

				break;

			default:
				break;
			}
		}

		CreatureAnimation(itemNumber, headingAngle, 0);
	}
}
